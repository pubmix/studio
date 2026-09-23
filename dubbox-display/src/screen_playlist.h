#pragma once

// Timeline view: four track lanes with real waveforms, clips with crop handles, a snip tool,
// beat markers, clips that can be moved along the timeline, a drum-pattern lane (place a
// pattern at the playhead, move it, stretch it to repeat, delete it), undo for all of these
// edits, and the playhead.
namespace playlist {

constexpr int kLaneTop = 106;
constexpr int kLaneH = 100;
constexpr int kLaneStride = 106;
constexpr int kWaveLeft = 96;
constexpr int kWaveRight = 1250;
// The drum-pattern lane sits under the four track lanes.
constexpr int kPatLaneTop = kLaneTop + 4 * kLaneStride;
constexpr int kPatLaneH = 106;
constexpr int kLanesBottom = kPatLaneTop + kPatLaneH;

// Builds the off-screen lane art. Call once after the display is up.
void init();
// Redraws this screen's body onto the visible layer.
void enter();
// Call every loop. Consumes link updates; only draws to the visible layer when `visible`.
void update(bool visible);
// Returns the track whose audio-picker was requested (its "+" button tapped), or -1.
//
// Pressing on a lane: a quick tap moves the playhead there, dragging scrubs, and pressing
// and holding still on a clip for about a third of a second picks that clip up (the whole
// track if it was never snipped) so dragging moves it along the timeline.
int touchDown(int x, int y);
void touchMove(int x, int y);
void touchUp();

// The SNIP and UNDO buttons in the strip under the header. Returns true if (x, y) hit one.
// SNIP toggles snip mode; while it is on, pressing or dragging over a lane shows a cut line
// and letting go splits the clip there (dragging down from the button into a lane works
// too). UNDO reverses the most recent snip (joins the pieces) or clip move.
bool stripButtonDown(int x, int y);

}  // namespace playlist
