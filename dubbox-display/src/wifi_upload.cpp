#include "wifi_upload.h"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <string.h>

#include "teensy_link.h"
#include "upload_page.h"

namespace wifiup {
namespace {

constexpr uint32_t kConnectTimeoutMs = 20000;
constexpr uint32_t kRetryMs = 30000;
constexpr size_t kRingSize = 24576;
constexpr int kWindow = 12;  // chunks in flight to the Teensy
constexpr uint32_t kStallMs = 10000;

Preferences g_prefs;
State g_state = State::Off;
char g_ssid[33] = {0};
char g_pass[65] = {0};
char g_addr[16] = {0};
char g_fail[24] = {0};
bool g_saved = false;
bool g_pendingSave = false;
uint32_t g_connectStartMs = 0;
uint32_t g_lastTryMs = 0;
bool g_serverStarted = false;

char g_scanSsid[kMaxScan][33];
int g_scanRssi[kMaxScan];
bool g_scanOpen[kMaxScan];
int g_scanCount = 0;
bool g_scanning = false;

// ---- The upload job, shared between the web server task (core 0) and update() (main loop) ----
enum JobStage : int { kIdle = 0, kRequested, kSending, kDone, kFailed };
struct Job {
  volatile int stage = kIdle;
  char name[48] = {0};
  char error[24] = {0};
  uint32_t size = 0;
  volatile uint32_t received = 0;  // bytes the browser side has put in the ring
  volatile uint32_t sent = 0;      // bytes handed to the Teensy
  volatile uint32_t confirmed = 0;
  volatile bool inputDone = false;
  volatile bool cancel = false;
  uint32_t sentChunks = 0;
  uint32_t startedMs = 0;
  uint32_t lastProgressMs = 0;
  bool startSent = false;
};
Job g_job;
uint8_t* g_ring = nullptr;
volatile uint32_t g_head = 0;  // bytes ever pushed
volatile uint32_t g_tail = 0;  // bytes ever popped
WebServer g_server(80);

void failJob(const char* why) {
  strncpy(g_job.error, why, sizeof(g_job.error) - 1);
  g_job.error[sizeof(g_job.error) - 1] = '\0';
  g_job.stage = kFailed;
}

bool jobRunning() { return g_job.stage == kRequested || g_job.stage == kSending; }
uint32_t g_dbgTotal = 0;  // test hook: bytes of generated WAV to feed, and how many were fed
uint32_t g_dbgFed = 0;
uint32_t g_dbgStartMs = 0;
bool g_refused = false;  // the upload that just arrived was turned away because another is running

// ---- Web server (runs in its own task) ----

void sendJson(int code, const char* body) {
  g_server.sendHeader("Cache-Control", "no-store");
  g_server.send(code, "application/json", body);
}

void onStatus() {
  char buf[96];
  const bool linked = teensylink::connected();
  snprintf(buf, sizeof(buf), "{\"linked\":%s,\"busy\":%s,\"playing\":%s}", linked ? "true" : "false",
           jobRunning() ? "true" : "false", teensylink::state().playing ? "true" : "false");
  sendJson(200, buf);
}

void beginJob(const char* name, uint32_t size) {
  g_head = 0;
  g_tail = 0;
  g_job.error[0] = '\0';
  g_job.received = g_job.sent = g_job.confirmed = 0;
  g_job.sentChunks = 0;
  g_job.inputDone = false;
  g_job.cancel = false;
  g_job.startSent = false;
  g_job.size = size;
  strncpy(g_job.name, name, sizeof(g_job.name) - 1);
  g_job.name[sizeof(g_job.name) - 1] = '\0';
  g_job.startedMs = g_job.lastProgressMs = millis();
  g_job.stage = kRequested;
}

void onUploadData() {
  HTTPUpload& up = g_server.upload();
  if (up.status == UPLOAD_FILE_START) {
    if (jobRunning() || g_ring == nullptr) {
      g_refused = true;  // answered in onUploadDone
      return;
    }
    g_refused = false;
    beginJob(g_server.arg("name").c_str(), static_cast<uint32_t>(g_server.arg("size").toInt()));
    // The main loop asks the Teensy; wait for its answer before taking data.
    for (int i = 0; i < 800 && g_job.stage == kRequested; ++i) vTaskDelay(5 / portTICK_PERIOD_MS);
  } else if (up.status == UPLOAD_FILE_WRITE) {
    size_t off = 0;
    uint32_t waitStart = millis();
    while (off < up.currentSize && g_job.stage == kSending) {
      const uint32_t used = g_head - g_tail;
      const size_t room = kRingSize - used;
      if (room == 0) {
        if (millis() - waitStart > 15000) {
          g_job.cancel = true;
          break;
        }
        vTaskDelay(2 / portTICK_PERIOD_MS);
        continue;
      }
      size_t n = up.currentSize - off;
      if (n > room) n = room;
      const size_t pos = g_head % kRingSize;
      size_t first = kRingSize - pos;
      if (first > n) first = n;
      memcpy(g_ring + pos, up.buf + off, first);
      if (n > first) memcpy(g_ring, up.buf + off + first, n - first);
      g_head = g_head + static_cast<uint32_t>(n);
      g_job.received = g_job.received + static_cast<uint32_t>(n);
      off += n;
      waitStart = millis();
    }
  } else if (up.status == UPLOAD_FILE_END) {
    g_job.inputDone = true;
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    g_job.cancel = true;
  }
}

void onUploadDone() {
  if (g_refused || g_ring == nullptr) {
    g_refused = false;
    sendJson(200, "{\"ok\":false,\"error\":\"busy\"}");
    return;
  }
  // Wait for the Teensy to finish writing what is still in the pipeline.
  const uint32_t t0 = millis();
  while (jobRunning() && millis() - t0 < 30000) vTaskDelay(10 / portTICK_PERIOD_MS);
  char buf[112];
  if (g_job.stage == kDone) {
    snprintf(buf, sizeof(buf), "{\"ok\":true,\"name\":\"%s\"}", g_job.name);
    sendJson(200, buf);
  } else {
    snprintf(buf, sizeof(buf), "{\"ok\":false,\"error\":\"%s\"}", g_job.stage == kFailed ? g_job.error : "timeout");
    sendJson(200, buf);
  }
}

void onRoot() { g_server.send_P(200, "text/html", kUploadPage); }

void serverTask(void*) {
  for (;;) {
    g_server.handleClient();
    vTaskDelay(2 / portTICK_PERIOD_MS);
  }
}

void startServer() {
  if (g_serverStarted) return;
  if (g_ring == nullptr) g_ring = static_cast<uint8_t*>(malloc(kRingSize));
  g_server.on("/", HTTP_GET, onRoot);
  g_server.on("/status", HTTP_GET, onStatus);
  g_server.on("/upload", HTTP_POST, onUploadDone, onUploadData);
  g_server.onNotFound([]() { g_server.send(404, "text/plain", "not found"); });
  g_server.begin();
  MDNS.begin("dubbox");
  MDNS.addService("http", "tcp", 80);
  xTaskCreatePinnedToCore(serverTask, "web", 8192, nullptr, 1, nullptr, 0);
  g_serverStarted = true;
}

bool feedDebugOnce();

// Feeds the generated tone into the ring (a 44-byte WAV header, then a 440 Hz sine, stereo 16-bit 44.1 kHz).
void feedDebug() {
  for (int round = 0; round < 16; ++round) {
    if (!feedDebugOnce()) break;
  }
}

bool feedDebugOnce() {
  if (g_dbgFed >= g_dbgTotal || g_job.stage != kSending) return false;
  uint8_t tmp[512];
  uint32_t room = kRingSize - (g_head - g_tail);
  uint32_t n = g_dbgTotal - g_dbgFed;
  if (n > room) n = room;
  if (n > sizeof(tmp)) n = sizeof(tmp);
  n &= ~3u;  // whole frames (the header is 44 bytes, also a multiple of 4)
  if (n == 0) return false;
  for (uint32_t i = 0; i < n; i += 4) {
    const uint32_t off = g_dbgFed + i;
    if (off < 44) {
      uint8_t hdr[44] = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 2, 0,
                         0x44, 0xAC, 0, 0, 0x10, 0xB1, 2, 0, 4, 0, 16, 0, 'd', 'a', 't', 'a', 0, 0, 0, 0};
      const uint32_t data = g_dbgTotal - 44, riff = g_dbgTotal - 8;
      memcpy(hdr + 4, &riff, 4);
      memcpy(hdr + 40, &data, 4);
      memcpy(tmp + i, hdr + off, 4);
    } else {
      const uint32_t frame = (off - 44) / 4;
      static int16_t table[100];  // 441 Hz: exactly 100 samples per cycle
      static bool ready = false;
      if (!ready) {
        for (int k = 0; k < 100; ++k) table[k] = static_cast<int16_t>(9000.0f * sinf(6.2831853f * k / 100.0f));
        ready = true;
      }
      const int16_t v = table[frame % 100];
      memcpy(tmp + i, &v, 2);
      memcpy(tmp + i + 2, &v, 2);
    }
  }
  for (uint32_t i = 0; i < n; ++i) g_ring[(g_head + i) % kRingSize] = tmp[i];
  g_head = g_head + n;
  g_job.received = g_job.received + n;
  g_dbgFed += n;
  if (g_dbgFed >= g_dbgTotal) g_job.inputDone = true;
  return true;
}

// ---- Relaying the upload to the Teensy (main loop) ----

void pumpJob(uint32_t now) {
  if (!jobRunning()) return;
  if (g_job.cancel) {
    teensylink::uploadCancel();
    failJob("cancelled");
    return;
  }
  const teensylink::UploadReply& rep = teensylink::uploadReply();
  if (g_job.stage == kRequested) {
    if (!teensylink::connected()) {
      failJob("not connected");
    } else if (!g_job.startSent) {
      if (g_job.size == 0) {
        failJob("size");
      } else {
        teensylink::uploadStart(g_job.size, g_job.name);
        g_job.startSent = true;
        g_job.startedMs = now;
      }
    } else if (rep.err[0] != '\0') {
      failJob(rep.err);
    } else if (rep.ok) {
      strncpy(g_job.name, rep.name, sizeof(g_job.name) - 1);
      g_job.stage = kSending;
      g_job.lastProgressMs = now;
    } else if (now - g_job.startedMs > 3000) {
      failJob("timeout");
    }
    return;
  }

  // kSending
  if (rep.err[0] != '\0') {
    failJob(rep.err);
    return;
  }
  const uint32_t confirmed = rep.acked * teensylink::kUploadChunk;
  if (confirmed != g_job.confirmed) {
    g_job.confirmed = confirmed < g_job.size ? confirmed : g_job.size;
    g_job.lastProgressMs = now;
  }
  if (rep.done) {
    g_job.confirmed = g_job.size;
    g_job.stage = kDone;
    return;
  }
  static uint8_t chunk[teensylink::kUploadChunk];
  while (g_job.sentChunks - rep.acked < static_cast<uint32_t>(kWindow) && g_job.sent < g_job.size) {
    uint32_t want = g_job.size - g_job.sent;
    if (want > static_cast<uint32_t>(teensylink::kUploadChunk)) want = teensylink::kUploadChunk;
    const uint32_t avail = g_head - g_tail;
    if (avail < want) {
      if (g_job.inputDone) failJob("size");  // the browser sent less than it promised
      break;
    }
    const uint32_t pos = g_tail % kRingSize;
    uint32_t first = kRingSize - pos;
    if (first > want) first = want;
    memcpy(chunk, g_ring + pos, first);
    if (want > first) memcpy(chunk + first, g_ring, want - first);
    if (!teensylink::uploadSendChunk(g_job.sentChunks, chunk, static_cast<int>(want))) break;
    g_tail = g_tail + want;
    g_job.sent = g_job.sent + want;
    ++g_job.sentChunks;
    g_job.lastProgressMs = now;
  }
  if (jobRunning() && now - g_job.lastProgressMs > kStallMs) {
    teensylink::uploadCancel();
    failJob("timeout");
  }
}

// ---- Connection ----

void beginConnect() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(g_ssid, g_pass);
  g_state = State::Connecting;
  g_connectStartMs = g_lastTryMs = millis();
  g_fail[0] = '\0';
}

}  // namespace

void begin() {
  g_prefs.begin("wifi", false);
  String s = g_prefs.getString("ssid", "");
  String p = g_prefs.getString("pass", "");
  strncpy(g_ssid, s.c_str(), sizeof(g_ssid) - 1);
  strncpy(g_pass, p.c_str(), sizeof(g_pass) - 1);
  g_saved = g_ssid[0] != '\0';
  WiFi.persistent(false);
  WiFi.setHostname("dubbox");
  if (g_saved) beginConnect();
}

void update() {
  const uint32_t now = millis();
  if (g_state != State::Off) {
    const wl_status_t st = WiFi.status();
    if (st == WL_CONNECTED) {
      if (g_state != State::Connected) {
        g_state = State::Connected;
        strncpy(g_addr, WiFi.localIP().toString().c_str(), sizeof(g_addr) - 1);
        if (g_pendingSave) {
          g_prefs.putString("ssid", g_ssid);
          g_prefs.putString("pass", g_pass);
          g_saved = true;
          g_pendingSave = false;
        }
        startServer();
      }
    } else {
      if (g_state == State::Connected) {
        g_state = State::Connecting;  // dropped; the radio retries by itself
        g_addr[0] = '\0';
        g_connectStartMs = now;
      }
      if (g_state == State::Connecting) {
        if (st == WL_NO_SSID_AVAIL && now - g_connectStartMs > 4000) {
          g_state = State::Failed;
          strcpy(g_fail, "NETWORK NOT FOUND");
        } else if (st == WL_CONNECT_FAILED && now - g_connectStartMs > 4000) {
          g_state = State::Failed;
          strcpy(g_fail, "WRONG PASSWORD?");
        } else if (now - g_connectStartMs > kConnectTimeoutMs) {
          g_state = State::Failed;
          strcpy(g_fail, "COULD NOT CONNECT");
        }
      } else if (g_state == State::Failed && g_saved && !g_pendingSave && now - g_lastTryMs > kRetryMs) {
        beginConnect();  // a saved network that was away (router restarting): try again now and then
      }
    }
  }

  if (g_scanning) {
    const int n = WiFi.scanComplete();
    if (n >= 0) {
      g_scanCount = 0;
      for (int i = 0; i < n && g_scanCount < kMaxScan * 4; ++i) {
        String name = WiFi.SSID(i);
        if (name.length() == 0 || name.length() > 32) continue;
        bool dup = false;
        for (int k = 0; k < g_scanCount && k < kMaxScan; ++k) dup = dup || name == g_scanSsid[k];
        if (dup) continue;
        // keep the strongest few, in order
        int pos = g_scanCount < kMaxScan ? g_scanCount : kMaxScan - 1;
        if (g_scanCount >= kMaxScan && WiFi.RSSI(i) <= g_scanRssi[kMaxScan - 1]) continue;
        while (pos > 0 && g_scanRssi[pos - 1] < WiFi.RSSI(i)) {
          strcpy(g_scanSsid[pos], g_scanSsid[pos - 1]);
          g_scanRssi[pos] = g_scanRssi[pos - 1];
          g_scanOpen[pos] = g_scanOpen[pos - 1];
          --pos;
        }
        strncpy(g_scanSsid[pos], name.c_str(), 32);
        g_scanSsid[pos][32] = '\0';
        g_scanRssi[pos] = WiFi.RSSI(i);
        g_scanOpen[pos] = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
        if (g_scanCount < kMaxScan) ++g_scanCount;
      }
      WiFi.scanDelete();
      g_scanning = false;
    } else if (n == WIFI_SCAN_FAILED) {
      g_scanCount = 0;
      g_scanning = false;
    }
  }

  feedDebug();
  pumpJob(now);

  static int lastStage = kIdle;
  if (g_job.stage != lastStage) {
    lastStage = g_job.stage;
    if (lastStage == kDone) {
      Serial.printf("WIFI: upload %s done (%lu bytes) in %lu ms\n", g_job.name, static_cast<unsigned long>(g_job.size),
                    static_cast<unsigned long>(millis() - g_dbgStartMs));
    } else if (lastStage == kFailed) {
      Serial.printf("WIFI: upload failed: %s\n", g_job.error);
    }
  }
}

State state() { return g_state; }
const char* ssid() { return g_ssid; }
const char* address() { return g_state == State::Connected ? g_addr : ""; }
const char* failReason() { return g_fail; }
bool hasSaved() { return g_saved; }

void connect(const char* ssid, const char* password) {
  strncpy(g_ssid, ssid, sizeof(g_ssid) - 1);
  g_ssid[sizeof(g_ssid) - 1] = '\0';
  strncpy(g_pass, password, sizeof(g_pass) - 1);
  g_pass[sizeof(g_pass) - 1] = '\0';
  g_pendingSave = true;
  WiFi.disconnect();
  g_addr[0] = '\0';
  beginConnect();
}

void forget() {
  g_prefs.remove("ssid");
  g_prefs.remove("pass");
  g_saved = false;
  g_pendingSave = false;
  g_ssid[0] = g_pass[0] = g_addr[0] = g_fail[0] = '\0';
  WiFi.disconnect(true);
  g_state = State::Off;
}

void startScan() {
  if (g_scanning) return;
  if (g_state == State::Off) WiFi.mode(WIFI_STA);
  g_scanCount = -1;
  g_scanning = true;
  WiFi.scanDelete();
  WiFi.scanNetworks(true);
}

int scanCount() { return g_scanning ? -1 : g_scanCount; }
const char* scanSsid(int i) { return g_scanSsid[i]; }
int scanRssi(int i) { return g_scanRssi[i]; }
bool scanOpen(int i) { return g_scanOpen[i]; }

void debugUpload(int seconds) {
  if (g_ring == nullptr) g_ring = static_cast<uint8_t*>(malloc(kRingSize));
  if (jobRunning() || g_ring == nullptr || seconds < 1 || seconds > 600) {
    Serial.println("WIFI: test upload refused (busy, or out of memory)");
    return;
  }
  g_dbgTotal = 44 + static_cast<uint32_t>(seconds) * 44100 * 4;
  g_dbgFed = 0;
  g_dbgStartMs = millis();
  beginJob("TESTTONE", g_dbgTotal);
}

Progress progress() {
  Progress p;
  switch (g_job.stage) {
    case kRequested: p.stage = Stage::Starting; break;
    case kSending: p.stage = Stage::Sending; break;
    case kDone: p.stage = Stage::Done; break;
    case kFailed: p.stage = Stage::Failed; break;
    default: p.stage = Stage::Idle; break;
  }
  p.size = g_job.size;
  p.sent = g_job.confirmed;
  strncpy(p.name, g_job.name, sizeof(p.name) - 1);
  strncpy(p.error, g_job.error, sizeof(p.error) - 1);
  return p;
}

}  // namespace wifiup
