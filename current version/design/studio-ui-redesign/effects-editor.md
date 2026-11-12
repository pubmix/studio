# A small, understandable effects rack

Proposal only. The companion [SVG](effects-editor.svg) is a code-native 1280×720 layout study, not firmware or a device screenshot. Typography is illustrative; production uses its bitmap fonts. No firmware changes or flash were performed for this research.

## Recommended interaction

Keep three clearly numbered cards in signal order, with labels always visible. Tap a card to select it; show only three large actions: **Edit**, **Move**, **Replace**. An empty card says **Add effect**. Keep chain-wide **Effect amount** and **Mute effects** together below the cards. Keep transport visible for auditioning.

1. **Add:** tap the empty card, choose an available effect from large named tiles, then return to the rack. Explain each sound briefly: Delay repeats; Reverb adds space; Chorus thickens; Flange sweeps. Show unavailable choices with a reason, not a mysterious gray tile.
2. **Edit:** open a focused page with Presets / Customize. Initially expose two or three large parameter tiles, with units and sensible bounds. Delay: Time and Feedback; Reverb: Room and Damping; Flange: Rate, Depth, Offset; Chorus: Voices. A parameter tile opens a large slider plus −/+ buttons and a value. Tapping the slider or −/+ works without dragging; the physical encoder remains an alternative.
3. **Move:** tap Move, then tap a destination slot, with a visible prospective order and explicit Apply/Cancel. Only valid destinations are enabled. Optional drag can be added later in a dedicated arrange mode, not as a hidden prerequisite. Snap to slots; keep the audio graph unchanged until commit. Include Undo after a successful change.
4. **Replace:** select a compatible effect, audition after confirmation, and retain an undo snapshot. Offer Remove only for optional slots when supported by the engine. Never silently move an exclusive effect away from another track: explain which track will lose it and require a clear Move here action.
5. **Mute:** retain current dub behavior: stop feeding the chain while existing tails decay. Label help as “New sound stops entering the effects; echoes fade out.” This is different from muting the whole track.

## Current capability versus proposed work

| Capability | Current code | Proposed change |
|---|---|---|
| Three slots | One primary plus up to two added effects | Preserve three, do not promise unlimited rack |
| Primary type | Delay / Flange / Freeverb / Chorus, cycled by physical control | Explicit Replace menu |
| Added slots | Flange / Chorus only; no duplicates | Display those constraints in Add |
| Delay / Freeverb | Shared exclusive instances between tracks | Warn before transferring ownership |
| Select slot, add, wet amount, chain bypass | Existing display commands | Reuse current behavior |
| Per-effect parameters | Engine setters; physical controls cycle presets | New touchscreen commands, acknowledgements and saved state required |
| Reorder or remove | Not exposed by current display protocol | Requires protocol, audio graph and project-format work; cannot ship as a cosmetic drag operation |
| Per-slot bypass / wet | Not current chain semantics | Defer; do not show nonfunctional toggles |
| Full wet/dry crossfade | Current dry stays full and wet is added | Call existing control **Effect amount**, not Wet/dry; true crossfade is separate DSP work |
| Modular cables | Not an existing patching engine | Defer to an advanced mode after rack editing is stable |

The current graph also restricts additional slots to Flange/Chorus, so a first implementation of Move should only exchange compatible optional slots. Keep the primary position fixed and explain why. Full reordering is a later engine change. The SVG's Move action represents that future capability; it is not already implemented.

## Physical sizing and layout

Active screen: 5.5×2.5 inches = 139.7×63.5mm, 1280×720 pixels. Approximate densities are 9.16px/mm horizontally and 11.34px/mm vertically. Thus a 9mm target needs roughly 83×102px; use at least 104px height for new controls, with 16–24px gutters. Do not apply square-pixel phone point sizes mechanically to this panel.

| Region | Bounds (x,y,width,height) | Purpose |
|---|---|---|
| Back / Done | (16,0,174,104), (1090,0,174,104) | Navigation |
| Three effect cards | x32/452/872, y174, 376×144 | Select / Add |
| Three selected actions | x32/452/872, y342, 376×104 | Edit / Move / Replace |
| Effect amount | (32,476,796,104) | Chain send amount |
| Mute effects | (872,476,376,104) | Tail-preserving bypass |
| Transport | y618–719 | Persistent audition controls |

Card labels 28–34px; secondary explanation 23px. Verify final bitmap widths, contrast, reachability and touch accuracy on the actual panel. Use text plus selection outline, not color alone. Preserve ongoing playback when navigating effect pages. Parameter changes should update only their tile; rearrangement should publish one stable frame after acknowledgement, not redraw the whole screen continuously.

## Scope and sequence

**First:** Add/Replace explanations, clearer effect amount, primary parameter pages using existing DSP capabilities. **Next:** saved effect presets, acknowledged state, undo and compatible slot moves. **Later:** advanced patching using named ports and tap source → tap destination, with drag as an optional shortcut; reject feedback loops and unsupported connections explicitly. Do not put a cable canvas in the beginner's default view.

Apple recommends alternatives to drag-and-drop. W3C likewise recommends a single-pointer alternative that does not require dragging; applying that principle here motivates Move → destination and tap/−/+ parameter editing. These are interaction recommendations, not a claim that this embedded device is WCAG-certified. [Apple drag and drop](https://developer.apple.com/design/human-interface-guidelines/drag-and-drop), [W3C dragging movements](https://www.w3.org/WAI/WCAG22/Understanding/dragging-movements).
