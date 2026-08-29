# Q2Raid Terminal System

Status: canonical design for the terminal structural pass. No terminal is an
encounter-specific C++ branch.

## Player promise

The terminal is a reusable diegetic computer rather than a one-click trigger.
It can present navigation, lore, camera feeds, external-code entry, switchable
systems, and short hacking puzzles through the same physical shell.

The first terminal appears immediately before the first door in a safe space.
It is deliberately short and forgiving. Its job is to teach cursor movement,
key selection, pressed-key feedback, submit, and cancel before those actions are
used under combat pressure. It also declares the mod's premise immediately:
this is visibly Quake II machinery, but the player should discard ordinary
Quake II expectations.

## Ownership split

| Layer | Responsibility |
| --- | --- |
| PMenu | Per-client menu ownership, open/close lifecycle, input capture and release |
| `raid_ui` | Active screen, cursor, hit testing, source gadget, completion/cancel and cleanup |
| `cg_raid_ui` | Graphical terminal composition from dedicated `STAT_RAID_UI_*` state |
| Map | Physical terminal, interaction volume and optional addressable systems |
| Terminal JSON | Pages, labels, key layout, puzzle type/data, success/failure event names, presentation manifest |
| Encounter JSON | Meaning of terminal events: doors, alarms, phases, hazards, rewards |
| Art manifest | Canvas, screen aperture, layer paths, key masks/rectangles, colours and pressed treatment |

## Composite renderer

The terminal is redrawn every client frame. It is not limited to one canvas
post. The intended stack is:

1. The player's unchanged real game view.
2. Dark masking outside the terminal footprint.
3. Chassis layer, opaque except for the CRT aperture and exterior alpha.
4. Screen treatment: scanlines, glass, tint, noise, vignette.
5. Runtime screen text, menu, puzzle, or camera annotations.
6. Controls/keyboard layer.
7. Runtime key labels and per-key dim/pressed overlays.
8. Cursor and focus/feedback effects.

Operating a terminal does not teleport the player, change solidity, or restore
a saved position afterward. Camera feeds may be added only through a separately
specified rendering capability; they are not implemented by moving the player's
physical entity.

## Asset health and crash rule

Current source inspection found:

- `terminal_screen.png`: valid alpha PNG.
- `terminal_controls_puzzle_v2.png`: valid alpha PNG.
- `terminal_chassis.png`: currently missing after the corrupt copy was removed.
- `terminal_master.png`: untracked and excluded from runtime packaging.
- Saved original `Grimy Military Terminal Interface.png`: valid authoritative
  source artwork.

The runtime must register/preflight every image before drawing. A missing,
invalid, or unsupported layer selects the rectangle/text fallback and prints
one useful warning. Presentation failure must never crash or retain movement
lock.

The first implementation placed Raid Hat rank at stat index 64 in a fixed
64-entry array and coupled terminal presentation to Raid Hat ownership. The
replacement uses explicitly named `STAT_RAID_UI_*` aliases in valid coop-only
CTF slots. Raid Hat state is updated independently and is never used as a Raid
UI sentinel. Future additions must not append beyond the KEX protocol limit.

Large source art remains archival. Runtime layers should be exported at one
documented supported resolution while retaining one shared normalized canvas.

## Keyboard contract

The keyboard artwork and input map are separate assets.

Each interactive key has:

- stable semantic `id`;
- normalized hit rectangle or alpha-mask bounds;
- optional character/value;
- label anchor in the top-left of its transparent key-local square;
- label colour/style;
- pressed/dim overlay region;
- role such as character, digit, back, submit, cancel, navigation, or action.

The art surgery should export each key as an alpha-isolated square at its exact
canvas position, or export a mask atlas with equivalent per-key bounds. Runtime
hit testing uses the manifest, never guessed keyboard rows. This permits exact
cursor mapping and intermittent dimming without baking puzzle letters into the
art.

Illustrative manifest fragment:

```json
{
  "canvas": [1224, 1285],
  "runtime_canvas": [612, 643],
  "screen_polygon": [[210,160], [1018,160], [1050,195], [1050,682], [1015,720], [210,720], [180,685], [180,195]],
  "layers": {
    "chassis": "terminal_chassis.png",
    "screen_fx": "terminal_screen.png",
    "controls": "terminal_controls_puzzle_v2.png"
  },
  "keys": [
    {"id":"alpha_1", "rect":[0.078,0.678,0.038,0.039], "label_anchor":[0.084,0.684], "role":"character"},
    {"id":"submit", "rect":[0.894,0.678,0.039,0.098], "role":"submit"}
  ]
}
```

Coordinates above illustrate the schema only; final rectangles come from the
pixel-accurate key surgery and must not be treated as production values.

## Terminal shell modes

### Menu

Fallout-like primitive navigation without copying Fallout's word-search hack:

- nested pages;
- lore/log entries;
- system status;
- camera selection;
- toggle/action entries;
- locked entries with readable requirements;
- back and exit.

A toggle emits a semantic event such as `terminal_action` with an action ID.
Encounter JSON decides whether that opens a door, disables a field, marks a
state, or does nothing.

### External code

The keypad accepts digits acquired elsewhere in the level. JSON supplies code,
length, attempt rules, and event IDs. The terminal displays entered digits and
clear/submit feedback. The code is encounter data, not C++.

### Camera

Camera-feed applications are deferred until the exposed KEX interface provides
or the project specifies a safe rendered-feed mechanism. A terminal must not
simulate a feed by moving the player's origin, view angles, or solidity.

## Busy-game permutations

All are short pressure tasks assembled from the same keyboard, screen, cursor,
and event vocabulary.

### Packet reconstruction

A corrupted intercepted phrase supplies only a limited set of key labels. The
player selects letters in the correct order to reconstruct callsigns or command
words—for example `ALPHA`, `BRAVO`, or `DELTA`. Letters may be shuffled,
duplicated, or divided into packets. Wrong input can clear a packet, add trace,
or emit a recoverable failure event.

This is the first implementation candidate because it directly proves dynamic
labels, per-key hit regions, key dimming, input buffer, submit, and JSON data.

The local source currently contains a hardcoded `ALPHA` onboarding proof so the
input/render path can be exercised before the loader exists. Its canonical data
is mirrored in `raid/terminals/entrance_terminal.json`; hardcoded letters and
rectangles are scaffolding to be removed when manifest and terminal JSON loading
are connected.

The Director-first activation proof is `raid/encounters/terminal_only_test.json`:
`trigger_raid_interaction` emits `interact`, then JSON issues the validated
`open_terminal` operation. `trigger_raid_terminal` is retained only as a
deprecated compatibility alias.

### Signal routing

A small grid contains source, sink, and rotatable junctions. Selecting a node
rotates or toggles it. Complete continuity routes the signal. JSON controls
grid, allowed pieces, move budget, and consequences. It reads as machinery,
not a vocabulary quiz.

### Bus balancing

Several circuits draw different loads. Keys toggle breakers while a live meter
shows total draw and unstable branches. The player must land inside a target
range without energising a forbidden pair. Useful for reactor, door, beam, and
shield fiction.

### Frequency lock

Three coarse controls alter phase, frequency, and amplitude of a displayed
wave. The player aligns it with a noisy reference trace. Difficulty comes from
tolerance and interference, not twitch precision.

### Memory buffer

The terminal flashes a short sequence across keys or screen nodes, then asks
for reproduction. JSON controls sequence length, symbols, speed, and whether
teammates see complementary information elsewhere.

### Camera verification

The terminal cycles live viewpoints. The player identifies a glyph, position,
open path, or changed object and submits the matching key/code. The camera is
information, not merely decoration.

### Trace isolation

Several channels pulse asynchronously; one carries a repeated anomaly. The
player samples or mutes channels to isolate it before a trace meter fills. This
creates pressure without simulating Hollywood source-code typing.

### Utility and novelty applications

The shell can also expose non-hacking pages such as encounter kill tracking,
best clear time, deaths, recovered lore, camera archives, or local system
diagnostics. These are presentation/data pages and should not require new
terminal subclasses.

`TRINITY – FAST FARTER FAST 4!` is retained as a hidden terminal application or
cartridge Easter egg. It is a separate import task, not part of the terminal
structural pass; the terminal shell only needs a generic application/page hook
capable of hosting it later.

## First-door onboarding instance

Recommended first proof:

- Terminal sits beside the first locked door.
- Activation moves to a close fixed camera or holds the existing view.
- Screen says `ACCESS TOKEN CORRUPTED`.
- Only the letters required for `ALPHA` are illuminated and labelled.
- Player selects them in order; used keys briefly dim/depress.
- Submit completes the tutorial and emits `terminal_complete` plus
  `terminal_action: entrance_unlock`.
- Encounter JSON opens the door.
- Wrong order clears the buffer with obvious feedback but no punishment.
- Use/cancel exits cleanly; reopening preserves or resets progress according to
  terminal JSON.

Later terminals can swap `ALPHA` for `BRAVO`, `DELTA`, scrambled multiword
phrases, codes found in the environment, menu trees, cameras, or other puzzle
types without changing the entity class.

## JSON sketch

```json
{
  "id": "entrance_terminal",
  "presentation": "terminal_grunge",
  "start_page": "boot",
  "pages": {
    "boot": {
      "type": "packet_reconstruction",
      "prompt": "ACCESS TOKEN CORRUPTED",
      "answer": "ALPHA",
      "available": ["A", "L", "P", "H", "A"],
      "shuffle": true,
      "on_success": "entrance_unlock",
      "on_failure": "clear_input"
    }
  }
}
```

Encounter JSON listens for the semantic action:

```json
{
  "source": "entrance_terminal",
  "signal": "terminal_action",
  "do": [
    {
      "op": "fire_target",
      "target": "entrance_door"
    }
  ]
}
```

## Acceptance gates

- Layer failure cannot crash.
- Screen aperture can reveal the moved real POV.
- Every key is hit-tested from the manifest and visibly responds.
- Font labels remain independent from the keyboard art.
- Menu, code, and puzzle data are JSON-selected.
- Terminal actions are semantic events; encounter consequences remain JSON.
- Complete, cancel, death, disconnect, wipe, and map change all restore player
  state through the same idempotent cleanup.
- The first-door tutorial is understandable without combat pressure.
