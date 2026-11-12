#include "bt_audio.h"

#include <Arduino.h>
#include <BluetoothA2DPSource.h>
#include <Preferences.h>
#include <driver/i2s.h>
#include <esp_bt.h>
#include <string.h>

#include "debug_log.h"

namespace btaudio {
namespace {

constexpr int kBclkPin = 34;
constexpr int kLrclkPin = 35;
constexpr int kDataPin = 36;
constexpr i2s_port_t kPort = I2S_NUM_0;

// The Teensy's audio clock is 44117.6 Hz, Bluetooth consumes 44100 Hz: input above kHighWater frames
// buffered is dropped so the delay cannot creep up. Output waits for kPrefill frames before it starts.
constexpr uint32_t kRingFrames = 4096;  // power of two
constexpr uint32_t kRingMask = kRingFrames - 1;
constexpr uint32_t kHighWater = 2048;
constexpr uint32_t kPrefill = 1024;
constexpr int kReadFrames = 128;
constexpr int kDmaBuffers = 6;

constexpr uint32_t kConnectingMs = 20000;

Preferences g_prefs;
BluetoothA2DPSource* g_a2dp = nullptr;

volatile bool g_wantOn = false;
volatile bool g_running = false;
volatile bool g_busy = false;
volatile bool g_restart = false;
volatile uint32_t g_connectingUntil = 0;
uint32_t g_restartAtMs = 0;  // nonzero: reboot at this time to apply a new on/off choice
bool g_nextOn = false;
char g_saved[kNameLen] = {0};

portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
Device g_devs[kMaxDevices];
int g_devCount = 0;

int16_t g_ring[kRingFrames * 2];
volatile uint32_t g_head = 0;  // written by the capture task
volatile uint32_t g_tail = 0;  // written by the Bluetooth data callback
volatile bool g_primed = false;
volatile bool g_capRun = false;
volatile bool g_capExited = true;
volatile uint32_t g_capFrames = 0;
volatile uint16_t g_peak = 0;
volatile uint32_t g_underruns = 0;
volatile uint32_t g_dropped = 0;
volatile uint32_t g_zeroCross = 0;

// ---- I2S capture ----

bool installI2s() {
  i2s_config_t cfg = {};
  cfg.mode = static_cast<i2s_mode_t>(I2S_MODE_SLAVE | I2S_MODE_RX);
  cfg.sample_rate = 44100;  // ignored in slave mode: the Teensy's clocks rule
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;  // the Teensy sends 16-bit audio in 32-bit slots
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = kDmaBuffers;
  cfg.dma_buf_len = kReadFrames;
  cfg.use_apll = false;
  if (i2s_driver_install(kPort, &cfg, 0, nullptr) != ESP_OK) return false;
  i2s_pin_config_t pins = {};
  pins.mck_io_num = I2S_PIN_NO_CHANGE;
  pins.bck_io_num = kBclkPin;
  pins.ws_io_num = kLrclkPin;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num = kDataPin;
  if (i2s_set_pin(kPort, &pins) != ESP_OK) {
    i2s_driver_uninstall(kPort);
    return false;
  }
  return true;
}

void captureTask(void*) {
  static int32_t buf[kReadFrames * 2];
  int zcState = 0;
  while (g_capRun) {
    size_t got = 0;
    i2s_read(kPort, buf, sizeof(buf), &got, pdMS_TO_TICKS(100));
    const int frames = static_cast<int>(got / 8);
    uint16_t peak = 0;
    uint32_t head = g_head;
    for (int i = 0; i < frames; ++i) {
      const int16_t l = static_cast<int16_t>(buf[2 * i] >> 16);
      const int16_t r = static_cast<int16_t>(buf[2 * i + 1] >> 16);
      const uint16_t al = l < 0 ? -l : l;
      const uint16_t ar = r < 0 ? -r : r;
      if (al > peak) peak = al;
      if (ar > peak) peak = ar;
      if (l > 800 && zcState <= 0) {
        zcState = 1;
        ++g_zeroCross;
      } else if (l < -800 && zcState >= 0) {
        zcState = -1;
        ++g_zeroCross;
      }
      if (head - g_tail >= kHighWater) {
        ++g_dropped;
        continue;
      }
      g_ring[(head & kRingMask) * 2] = l;
      g_ring[(head & kRingMask) * 2 + 1] = r;
      ++head;
    }
    g_head = head;
    g_capFrames += frames;
    if (peak > g_peak) g_peak = peak;
  }
  g_capExited = true;
  vTaskDelete(nullptr);
}

bool startCapture() {
  if (!installI2s()) return false;
  g_head = g_tail = 0;
  g_primed = false;
  g_capRun = true;
  g_capExited = false;
  xTaskCreatePinnedToCore(captureTask, "i2scap", 4096, nullptr, 3, nullptr, 1);
  return true;
}

void stopCapture() {
  g_capRun = false;
  for (int i = 0; i < 40 && !g_capExited; ++i) vTaskDelay(pdMS_TO_TICKS(10));
  i2s_driver_uninstall(kPort);
}

// ---- Bluetooth ----

// Called on the Bluetooth task for every audio frame the library wants.
int32_t onFrames(Frame* out, int32_t count) {
  const uint32_t head = g_head;
  uint32_t tail = g_tail;
  const uint32_t avail = head - tail;
  if (!g_primed) {
    if (avail < kPrefill) {
      memset(out, 0, static_cast<size_t>(count) * sizeof(Frame));
      return count;
    }
    g_primed = true;
  }
  const int32_t n = avail < static_cast<uint32_t>(count) ? static_cast<int32_t>(avail) : count;
  for (int32_t i = 0; i < n; ++i, ++tail) {
    out[i].channel1 = g_ring[(tail & kRingMask) * 2];
    out[i].channel2 = g_ring[(tail & kRingMask) * 2 + 1];
  }
  g_tail = tail;
  if (n < count) {
    memset(out + n, 0, static_cast<size_t>(count - n) * sizeof(Frame));
    g_primed = false;
    ++g_underruns;
  }
  return count;
}

void rememberDevice(const char* name, int rssi) {
  portENTER_CRITICAL(&g_mux);
  int slot = -1;
  for (int i = 0; i < g_devCount; ++i) {
    if (strcmp(g_devs[i].name, name) == 0) slot = i;
  }
  if (slot < 0 && g_devCount < kMaxDevices) slot = g_devCount++;
  if (slot >= 0) {
    strlcpy(g_devs[slot].name, name, kNameLen);
    g_devs[slot].rssi = rssi;
  }
  portEXIT_CRITICAL(&g_mux);
}

// Returning true makes the library connect to this device.
bool onSsid(const char* ssid, esp_bd_addr_t, int rssi) {
  rememberDevice(ssid, rssi);
  portENTER_CRITICAL(&g_mux);
  const bool match = g_saved[0] != '\0' && strcmp(ssid, g_saved) == 0;
  portEXIT_CRITICAL(&g_mux);
  return match;
}

// Brings the radio up for Bluetooth Classic only. The Arduino core's own btStart() would enable the
// dual-mode (Classic + BLE) controller, whose BLE half costs memory this app does not have to spare.
bool startController() {
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED) return true;
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE) {
    esp_bt_controller_mem_release(ESP_BT_MODE_BLE);  // only valid before init; an error on a repeat is harmless
    esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    cfg.mode = ESP_BT_MODE_CLASSIC_BT;
    if (esp_bt_controller_init(&cfg) != ESP_OK) return false;
    while (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE) vTaskDelay(pdMS_TO_TICKS(10));
  }
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_INITED) {
    if (esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT) != ESP_OK) return false;
  }
  return esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED;
}

void startAll() {
  g_busy = true;
  portENTER_CRITICAL(&g_mux);
  g_devCount = 0;
  portEXIT_CRITICAL(&g_mux);
  // If the chip resets while the stack is starting, the next boot sees this flag and stays off.
  g_prefs.putBool("crash", true);
  dbg::printf("BT: heap before start: free %u, largest block %u\n", (unsigned)ESP.getFreeHeap(),
              (unsigned)ESP.getMaxAllocHeap());
  if (!startController()) {
    dbg::println("BT: controller failed to start");
    g_prefs.putBool("crash", false);
    g_wantOn = false;
    g_busy = false;
    return;
  }
  dbg::printf("BT: controller up, free heap %u, largest block %u\n", (unsigned)ESP.getFreeHeap(),
              (unsigned)ESP.getMaxAllocHeap());
  if (g_a2dp == nullptr) g_a2dp = new BluetoothA2DPSource();
  g_a2dp->set_ssid_callback(onSsid);
  g_a2dp->set_data_callback_in_frames(onFrames);
  g_a2dp->set_local_name("Dub-Box");
  g_a2dp->set_auto_reconnect(true);
  g_a2dp->start();
  g_a2dp->set_volume(127);
  // Bluetooth took its memory first; the I2S capture buffers come out of what is left.
  if (!startCapture()) dbg::println("BT: I2S capture failed to start");
  g_running = true;
  g_busy = false;
  dbg::printf("BT: started (saved speaker: '%s', free heap %u)\n", g_saved, (unsigned)ESP.getFreeHeap());
  vTaskDelay(pdMS_TO_TICKS(4000));
  g_prefs.putBool("crash", false);  // survived the risky part
}

void stopAll() {
  g_busy = true;
  if (g_a2dp != nullptr) g_a2dp->end(false);
  stopCapture();
  g_running = false;
  g_busy = false;
  dbg::println("BT: stopped");
}

void controlTask(void*) {
  for (;;) {
    if (g_restart && g_running) {
      g_restart = false;
      stopAll();
    }
    g_restart = false;
    if (g_wantOn && !g_running) {
      startAll();
    } else if (!g_wantOn && g_running) {
      stopAll();
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

uint32_t g_lastStatsMs = 0;
uint32_t g_lastFrames = 0;
uint32_t g_lastZc = 0;
uint32_t g_fps = 0;
uint32_t g_fpsMs = 0;

// Refreshes the frames-per-second figure about once a second (shared by the screen and the debug log).
void refreshFps() {
  const uint32_t now = millis();
  const uint32_t dt = now - g_fpsMs;
  if (dt < 1000) return;
  const uint32_t frames = g_capFrames;
  g_fps = static_cast<uint32_t>((frames - g_lastFrames) * 1000ull / dt);
  g_lastFrames = frames;
  g_fpsMs = now;
}

}  // namespace

void begin() {
  g_prefs.begin("bt", false);
  g_wantOn = g_prefs.getBool("on", false);
  // Version 2: an earlier build could leave "on" saved and then crash on every boot. Also, a crash
  // flag left set means the last start attempt took the chip down: stay off and let the user retry.
  if (g_prefs.getUChar("v", 0) < 2 || g_prefs.getBool("crash", false)) {
    g_wantOn = false;
    g_prefs.putBool("on", false);
    g_prefs.putBool("crash", false);
    g_prefs.putUChar("v", 2);
  }
  g_prefs.getString("name", g_saved, sizeof(g_saved));
  xTaskCreatePinnedToCore(controlTask, "btctl", 8192, nullptr, 1, nullptr, 0);
}

bool active() { return g_restartAtMs != 0 ? g_nextOn : g_wantOn; }

// Bluetooth Classic and the WiFi upload server do not both fit in this chip's memory, so switching
// either way saves the choice and restarts: the next boot brings up only one of them (WiFi is held
// back in wifiup::begin() while Bluetooth is on).
void setActive(bool on) {
  if (on == g_wantOn || g_restartAtMs != 0) return;
  g_prefs.putBool("on", on);
  g_restartAtMs = millis() + 400;  // let the screen show the change first
  g_nextOn = on;
}

Status status() {
  if (g_restartAtMs != 0) return g_nextOn ? Status::Starting : Status::Stopping;
  if (g_busy) return g_wantOn ? Status::Starting : Status::Stopping;
  if (!g_running) return g_wantOn ? Status::Starting : Status::Off;
  if (g_a2dp != nullptr && g_a2dp->is_connected()) return Status::Connected;
  if (g_saved[0] != '\0' && static_cast<int32_t>(g_connectingUntil - millis()) > 0) return Status::Connecting;
  return Status::Scanning;
}

const char* savedName() { return g_saved; }

int deviceCount() {
  portENTER_CRITICAL(&g_mux);
  const int n = g_devCount;
  portEXIT_CRITICAL(&g_mux);
  return n;
}

Device device(int index) {
  Device d;
  portENTER_CRITICAL(&g_mux);
  if (index >= 0 && index < g_devCount) d = g_devs[index];
  portEXIT_CRITICAL(&g_mux);
  return d;
}

void selectDevice(int index) {
  const Device d = device(index);
  if (d.name[0] == '\0') return;
  const bool wasConnected = g_a2dp != nullptr && g_a2dp->is_connected();
  portENTER_CRITICAL(&g_mux);
  strlcpy(g_saved, d.name, kNameLen);
  portEXIT_CRITICAL(&g_mux);
  g_prefs.putString("name", d.name);
  g_connectingUntil = millis() + kConnectingMs;
  // While scanning, the next scan pass sees the name and connects. If already connected elsewhere,
  // the Bluetooth stack has to restart to scan again.
  if (wasConnected) g_restart = true;
}

void forgetDevice() {
  portENTER_CRITICAL(&g_mux);
  g_saved[0] = '\0';
  portEXIT_CRITICAL(&g_mux);
  g_prefs.putString("name", "");
  g_connectingUntil = 0;
  g_restart = true;
}

Capture takeCapture() {
  Capture c;
  refreshFps();
  c.framesPerSec = g_running ? g_fps : 0;
  const uint16_t pk = g_peak;
  g_peak = 0;
  c.peakPermille = static_cast<int>(pk * 1000u / 32767u);
  return c;
}

void update() {
  if (g_restartAtMs != 0 && static_cast<int32_t>(millis() - g_restartAtMs) >= 0) {
    g_prefs.end();
    ESP.restart();
  }
  if (!g_running) return;
  refreshFps();
  static int shownDevices = 0;
  if (g_devCount < shownDevices) shownDevices = 0;  // the list was cleared by a restart
  while (shownDevices < deviceCount()) {
    const Device d = device(shownDevices++);
    dbg::printf("BT: found '%s' (%d dBm)\n", d.name, d.rssi);
  }
  const uint32_t now = millis();
  if (now - g_lastStatsMs < 2000) return;
  const uint32_t dtMs = now - g_lastStatsMs;
  g_lastStatsMs = now;
  const uint32_t zc = g_zeroCross;
  dbg::printf("BT: state=%d in=%u fps peak=%d/1000 zc=%u/s under=%u drop=%u heap=%u\n", static_cast<int>(status()),
              (unsigned)g_fps, static_cast<int>(g_peak * 1000u / 32767u),
              (unsigned)((zc - g_lastZc) * 1000u / (dtMs ? dtMs : 1)),
              (unsigned)g_underruns, (unsigned)g_dropped, (unsigned)ESP.getFreeHeap());
  g_lastZc = zc;
  g_peak = 0;
}

}  // namespace btaudio
