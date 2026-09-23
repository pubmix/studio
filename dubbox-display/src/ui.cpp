#include "ui.h"
#include <Arduino.h>
#include <string.h>
#include "screen_keyboard.h"
#include "screen_load.h"
#include "screen_menu.h"
#include "screen_settings.h"
#include "screen_picker.h"
#include "screen_mixer.h"
#include "screen_pattern.h"
#include "screen_wifi.h"
#include "wifi_upload.h"
#include "screen_stems.h"
#include "screen_playlist.h"
#include "teensy_link.h"
#include "touch_gt911.h"
#include "ui_common.h"
#include "screen_mirror.h"

namespace ui {
namespace {

Screen current = Screen::Menu;

enum class Owner { None, Header, Transport, Slider, Volume, Body };
Owner touchOwner = Owner::None;
bool touching = false;
uint32_t lastTouchMs = 0;

bool wasPlayingDrawn = false;
int wasUploadPercentDrawn = -1;
int wasPreviewDrawn = 0;  // drum loop / record state the transport was last drawn for

int drumTransportState() {
  const teensylink::Drums& d = teensylink::state().drums;
  return (d.previewOn ? 1 : 0) | (d.recording ? 2 : 0) | (d.countIn ? 4 : 0) | (d.clickOn ? 8 : 0) |
         (d.padMode == 1 ? 16 : 0);
}
uint32_t lastClockMs = 0;
uint32_t lastDemoMs = 0;
uint32_t lastSeekSendMs = 0;
int lastTouchX = 0;
int lastTouchY = 0;

// Name shown in the header; set as soon as a project is created/opened (the
// Teensy confirms it a moment later) and cleared on returning to the menu.
char shownProject[24] = {0};
Screen pickerReturn = Screen::Playlist;

void goTo(Screen s);

void openPicker(int track) {
  pickerReturn = current;
  picker::setTrack(track);
  goTo(Screen::FilePicker);
}

void sliderSeek(int x, bool final) {
  uint32_t now = millis();
  uint32_t ms = sliderMsForX(x);
  if (final || now - lastSeekSendMs >= 60) {
    lastSeekSendMs = now;
    seekToMs(ms);
  } else {
    g.posMs = ms;
    g.posHoldUntil = now + 450;
  }
  drawSlider();
  drawClock();
}

bool inProject(Screen s) {
  return s == Screen::Mixer || s == Screen::Playlist || s == Screen::Pattern;
}

void drawHeaderFor(Screen s) {
  const char* project = shownProject[0] ? shownProject : "PROJECT";
  switch (s) {
    case Screen::Menu:
      drawHeader("MENU", -1);
      break;
    case Screen::NewProject:
      drawHeader("NEW PROJECT", -1, "MENU");
      break;
    case Screen::LoadProject:
      drawHeader("LOAD PROJECT", -1, "MENU");
      break;
    case Screen::Mixer:
      drawHeader(project, kTabMixer);
      break;
    case Screen::Playlist:
      drawHeader(project, kTabPlaylist);
      break;
    case Screen::Pattern:
      drawHeader(project, kTabPattern);
      break;
    case Screen::Stems:
      drawHeader("STEM SPLITTER", -1, "MENU");
      break;
    case Screen::Wifi:
      drawHeader("WIFI UPLOAD", -1, "MENU");
      break;
    case Screen::Settings:
      drawHeader("SETTINGS", -1, "MENU");
      break;
    case Screen::FilePicker: {
      char title[24];
      snprintf(title, sizeof(title), "ADD TO TRACK %d", picker::track() + 1);
      drawHeader(title, -1, "BACK");
      break;
    }
  }
}

void goTo(Screen s) {
  if (touchOwner == Owner::Body) {
    if (current == Screen::Mixer) mixer::touchUp();
    if (current == Screen::Playlist) playlist::touchUp();
    if (current == Screen::Pattern) pattern::touchUp();
  }
  if (current == Screen::Pattern && s != Screen::Pattern) pattern::leave();
  if (s == Screen::Menu) {
    // Being on the menu means no project is open: stop playback and close it.
    shownProject[0] = '\0';
    if (g.linked) teensylink::sendCloseProject();
  }
  current = s;
  setCanvas(kVisible);
  fillRect(0, 0, kW - 1, kH - 1, rgb565(0x000000));
  drawHeaderFor(s);
  switch (s) {
    case Screen::Menu:
      menu::enter();
      break;
    case Screen::NewProject:
      keyboard::enter();
      break;
    case Screen::LoadProject:
      loadscreen::enter();
      break;
    case Screen::Mixer:
      mixer::enter();
      drawTransport();
      break;
    case Screen::Playlist:
      drawTransport();  // first, so the SNIP button drawn by playlist::enter() stays on top
      playlist::enter();
      break;
    case Screen::Pattern:
      drawTransport(true);
      pattern::enter();
      break;
    case Screen::FilePicker:
      picker::enter(picker::track());
      break;
    case Screen::Stems:
      stemscreen::enter();
      break;
    case Screen::Wifi:
      wifiscreen::enter();
      break;
    case Screen::Settings:
      settings::enter();
      break;
  }
  wasPlayingDrawn = g.playing;
  wasPreviewDrawn = drumTransportState();
}

void startProject(const char* name, bool isNew) {
  strncpy(shownProject, name, sizeof(shownProject) - 1);
  shownProject[sizeof(shownProject) - 1] = '\0';
  if (isNew) {
    teensylink::sendNewProject(name);
  } else {
    teensylink::sendOpenProject(name);
  }
  goTo(Screen::Mixer);
}

bool nameTaken(const char* name) {
  const teensylink::ProjectInfo& p = teensylink::state().project;
  for (int i = 0; i < p.listCount; ++i) {
    if (strcasecmp(p.list[i], name) == 0) return true;
  }
  return false;
}

void onTouchDown(int x, int y) {
  mirror::touch("down", x, y);
  HeaderHit hh = headerHit(x, y);
  if (hh == HeaderHit::Volume) {
    touchOwner = Owner::Volume;
    setVolumeFromX(x, false);
    return;
  }
  if (hh != HeaderHit::None) {
    touchOwner = Owner::Header;
    switch (hh) {
      case HeaderHit::TabMixer:
        if (current != Screen::Mixer) goTo(Screen::Mixer);
        break;
      case HeaderHit::TabPlaylist:
        if (current != Screen::Playlist) goTo(Screen::Playlist);
        break;
      case HeaderHit::TabPattern:
        if (current != Screen::Pattern) goTo(Screen::Pattern);
        break;
      case HeaderHit::Menu:
        goTo(Screen::Menu);
        break;
      case HeaderHit::Back:
        goTo(pickerReturn);
        break;
      default:
        break;
    }
    return;
  }
  if (current == Screen::Playlist && playlist::stripButtonDown(x, y)) {
    touchOwner = Owner::Body;  // later moves/release drive the snip cut preview
    return;
  }
  if (inProject(current) && inTransportArea(x, y)) {
    touchOwner = Owner::Transport;
    TransportHit th = transportHit(x, y);
    if (th == TransportHit::Slider) {
      touchOwner = Owner::Slider;
      sliderSeek(x, false);
    } else if (th == TransportHit::PlayPattern) {
      // Loop the current pattern by itself. The Teensy only previews while the song is paused,
      // so pause the song first if it is playing (the commands are handled in order).
      if (g.linked) {
        bool on = teensylink::state().drums.previewOn;
        if (!on && g.playing) teensylink::sendTogglePlay();
        teensylink::setPreview(!on, g.pattern);
        drawTransport(true);
      }
    } else if (th == TransportHit::Record) {
      // A one-bar count-in, then records into the looping pattern. Pressing again stops recording
      // (or cancels the count-in); the loop keeps playing until PATTERN / STOP.
      if (g.linked) {
        const teensylink::Drums& d = teensylink::state().drums;
        if (d.recording || d.countIn) {
          teensylink::setPreview(true, g.pattern, false);
        } else {
          if (g.playing) teensylink::sendTogglePlay();  // the loop only runs while the song is paused
          teensylink::setPreview(true, g.pattern, true);
        }
        drawTransport(true);
      }
    } else if (th == TransportHit::ClickToggle) {
      if (g.linked) {
        teensylink::setClickOn(!teensylink::state().drums.clickOn);
        drawTransport(true);
      }
    } else if (th == TransportHit::PadMode) {
      if (g.linked) {
        teensylink::setPadMode(teensylink::state().drums.padMode == 1 ? 0 : 1);
        drawTransport(true);
      }
    } else if (th == TransportHit::Play) {
      if (g.linked) {
        teensylink::sendTogglePlay();
      } else {
        g.playing = !g.playing;
        lastDemoMs = millis();
      }
    } else if (th == TransportHit::Rewind) {
      if (g.linked) {
        teensylink::sendRewind();
      } else {
        g.posMs = 0;
      }
    }
    return;
  }
  touchOwner = Owner::Body;
  switch (current) {
    case Screen::Menu: {
      Screen next = menu::touchDown(x, y);
      if (next != Screen::Menu) goTo(next);
      break;
    }
    case Screen::NewProject: {
      keyboard::Action a = keyboard::touchDown(x, y);
      if (a == keyboard::Action::Cancel) {
        goTo(Screen::Menu);
      } else if (a == keyboard::Action::Create) {
        const char* name = keyboard::name();
        if (name[0] == '\0') {
          keyboard::showMessage("ENTER A NAME FIRST");
        } else if (!g.linked) {
          keyboard::showMessage("TEENSY NOT CONNECTED");
        } else if (nameTaken(name)) {
          keyboard::showMessage("A PROJECT WITH THAT NAME ALREADY EXISTS");
        } else {
          startProject(name, true);
        }
      }
      break;
    }
    case Screen::LoadProject: {
      const char* name = loadscreen::touchDown(x, y);
      if (name != nullptr) startProject(name, false);
      break;
    }
    case Screen::Mixer: {
      int t = mixer::touchDown(x, y);
      if (t >= 0) openPicker(t);
      break;
    }
    case Screen::Playlist: {
      int t = playlist::touchDown(x, y);
      if (t >= 0) openPicker(t);
      break;
    }
    case Screen::Pattern:
      pattern::touchDown(x, y);
      break;
    case Screen::Stems:
      stemscreen::touchDown(x, y);
      break;
    case Screen::Wifi:
      wifiscreen::touchDown(x, y);
      break;
    case Screen::Settings:
      if (settings::touchDown(x, y)) {
        playlist::init(); // Rebuild both cached waveform layers in the new palette.
        goTo(Screen::Settings);
      }
      break;
    case Screen::FilePicker: {
      picker::Action a = picker::touchDown(x, y);
      if (a == picker::Action::Picked) {
        teensylink::sendAssignTrack(picker::track(), picker::chosen());
        goTo(pickerReturn);
      } else if (a == picker::Action::Remove) {
        teensylink::sendClearTrack(picker::track());
        goTo(pickerReturn);
      }
      break;
    }
  }
}

void onTouchMove(int x, int y) {
  static uint32_t lastTrace = 0;
  if (millis() - lastTrace >= 50) { mirror::touch("move", x, y); lastTrace = millis(); }
  if (touchOwner == Owner::Volume) {
    setVolumeFromX(x, false);
    return;
  }
  if (touchOwner == Owner::Slider) {
    sliderSeek(x, false);
    return;
  }
  if (touchOwner != Owner::Body) return;
  if (current == Screen::Mixer) mixer::touchMove(x, y);
  if (current == Screen::Playlist) playlist::touchMove(x, y);
  if (current == Screen::Pattern) pattern::touchMove(x, y);
}

void onTouchUp() {
  mirror::touch("up", lastTouchX, lastTouchY);
  if (touchOwner == Owner::Volume) setVolumeFromX(lastTouchX, true);
  if (touchOwner == Owner::Slider) sliderSeek(lastTouchX, true);
  if (touchOwner == Owner::Body) {
    if (current == Screen::Mixer) mixer::touchUp();
    if (current == Screen::Playlist) playlist::touchUp();
    if (current == Screen::Pattern) pattern::touchUp();
  }
  touchOwner = Owner::None;
}

void updateLink(uint32_t now) {
  teensylink::poll();
  bool nowLinked = teensylink::connected();
  if (nowLinked != g.linked) {
    g.linked = nowLinked;
    Serial.printf("link %s\n", g.linked ? "UP" : "DOWN");
    drawLinkTag();
    if (g.linked) {
      teensylink::sendHello();
      // The menu means "no project open" — make the Teensy agree after any reset.
      if (current == Screen::Menu || current == Screen::Settings) teensylink::sendCloseProject();
      if (current == Screen::LoadProject) loadscreen::enter();
    } else if (current == Screen::LoadProject) {
      loadscreen::refresh();
    }
  }
  if (g.linked) {
    const teensylink::State& st = teensylink::state();
    g.playing = st.playing;
    if (now >= g.posHoldUntil) g.posMs = st.posMs;
    if (teensylink::takeVolumeChanged() && now >= g.volHoldUntil) {
      g.volPermille = st.volumePermille;
      drawVolume();
    }
    g.songMs = st.songMs > 0 ? st.songMs : 8000;
  } else if (g.playing && now - lastDemoMs >= 40) {
    g.posMs += now - lastDemoMs;
    lastDemoMs = now;
    if (g.posMs >= g.songMs) g.posMs = 0;
  }

  if (teensylink::takeProjectChanged()) {
    const teensylink::ProjectInfo& p = teensylink::state().project;
    if (p.open && inProject(current) && strcmp(shownProject, p.name) != 0) {
      strncpy(shownProject, p.name, sizeof(shownProject) - 1);
      drawHeaderFor(current);
    }
  }
  if (teensylink::takeListChanged() && current == Screen::LoadProject) loadscreen::refresh();
  if (teensylink::takeFilesChanged() && current == Screen::FilePicker) picker::refresh();
}

}  // namespace

void begin() {
  theme::begin();
  uint32_t t0 = millis();
  playlist::init();
  Serial.printf("ui art built in %lu ms\n", (unsigned long)(millis() - t0));
  goTo(Screen::Menu);
}

void update() {
  const auto upload = wifiup::progress();
  const bool saving = upload.stage == wifiup::Stage::Starting || upload.stage == wifiup::Stage::Sending;
  g.uploadPercent = saving ? (upload.size ? static_cast<int>(100ULL * upload.sent / upload.size) : 0) : -1;
  uint32_t now = millis();
  updateLink(now);

  static uint32_t lastStatsMs = 0;
  if (now - lastStatsMs >= 5000) {
    lastStatsMs = now;
    uint32_t lines = 0, maxGap = 0;
    teensylink::takeLinkStats(lines, maxGap);
    Serial.printf("linkstats: %lu state lines / 5s, max gap %lu ms, uptime %lu s\n",
                  (unsigned long)lines, (unsigned long)maxGap, (unsigned long)(now / 1000));
  }

  TouchPoint pts[5];
  int n = touchRead(pts, 5);
  if (current == Screen::Pattern) {
    int xs[5], ys[5];
    for (int i = 0; i < n && i < 5; ++i) {
      xs[i] = (kPanelH - 1) - (int)pts[i].y;  // panel (px, py) to landscape, as for the first finger
      ys[i] = (int)pts[i].x;
    }
    pattern::touchPoints(xs, ys, n < 5 ? n : 5);
  }
  if (n > 0) {
    // Panel (px, py) to landscape: lx = 1279 - py, ly = px.
    int lx = (kPanelH - 1) - (int)pts[0].y;
    int ly = (int)pts[0].x;
    lastTouchMs = now;
    lastTouchX = lx;
    lastTouchY = ly;
    if (!touching) {
      touching = true;
      onTouchDown(lx, ly);
    } else {
      onTouchMove(lx, ly);
    }
  } else if (touching && now - lastTouchMs > 120) {
    touching = false;
    onTouchUp();
  }

  playlist::update(current == Screen::Playlist);
  mixer::update(current == Screen::Mixer);
  pattern::update(current == Screen::Pattern);
  if (current == Screen::Wifi) wifiscreen::update();
  if (current == Screen::Stems) stemscreen::update();

  if (inProject(current)) {
    int previewNow = drumTransportState();
    bool splitPlay = current == Screen::Pattern;
    if (g.playing != wasPlayingDrawn || g.uploadPercent != wasUploadPercentDrawn || (splitPlay && previewNow != wasPreviewDrawn)) {
      wasUploadPercentDrawn = g.uploadPercent;
      wasPlayingDrawn = g.playing;
      wasPreviewDrawn = previewNow;
      drawTransport(splitPlay);
    }
    drawSlider();
    if (now - lastClockMs >= 100) {
      lastClockMs = now;
      drawClock();
    }
  }
  mirror::frame(static_cast<int>(current));
}

}  // namespace ui
