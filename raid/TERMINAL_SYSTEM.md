# Q2Raid Terminal System

Status: canonical design for the terminal structural and presentation pass. No terminal is an encounter-specific C++ branch.

## Player promise

The terminal is a reusable diegetic computer rather than a one-click trigger. It can present navigation, lore, camera feeds, external-code entry, switchable systems, and short hacking puzzles through the same physical shell.

The first terminal appears immediately before the first door in a safe space. It is deliberately short and forgiving. Its job is to teach cursor movement, key selection, pressed-key feedback, submit, and cancel before those actions are used under combat pressure.

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

The terminal is redrawn every client frame. The approved decorative presentation uses one shared `975 x 1024` source canvas mapped once into the runtime terminal rectangle.

The finished stack is:

1. The player's unchanged real game view.
2. Dark masking outside the terminal footprint.
3. **Engine-drawn primitive chassis/backing.** This code-rendered layer is always present and remains the safe terminal backing/fallback.
4. **`terminal_chassis.png`.** A separate decorative chassis layer drawn over the engine backing; it does not replace the engine backing.
5. **`terminal_screen.png`.** CRT/glass/screen treatment.
6. **`terminal_controls_clean.png`.** Static keyboard/keypad/control artwork.
7. Runtime screen text, menu, puzzle, or camera annotations.
8. Runtime key labels and per-key dim/pressed overlays.
9. Cursor and focus/feedback effects.

The two chassis concepts are cumulative, not alternatives:

```text
engine-drawn backing/chassis
        +
terminal_chassis.png
        +
terminal_screen.png
        +
terminal_controls_clean.png
        +
live terminal UI
```

Operating a terminal does not teleport the player, change solidity, or restore a saved position afterward. Camera feeds remain deferred until a separately specified safe rendering capability exists.

## Approved runtime art and publication policy

The approved terminal runtime images are local test/distribution assets. Their binary bytes are not required to be published in the public source repository. The public repository carries the renderer, manifest, documented runtime paths, hashes, and fallback contract.

| Runtime path | Canvas | SHA-256 |
| --- | --- | --- |
| `raid/ui/terminal_grunge/terminal_chassis.png` | `975 x 1024` RGBA | `45fcd7354b3253014b37de6ac437c65d11ba787d5cb712c434ee13958240c589` |
| `raid/ui/terminal_grunge/terminal_screen.png` | `975 x 1024` RGBA | `9da32277c3ce24f404168e68aa999cfee3912cacb30a761204a48b80e2d1e457` |
| `raid/ui/terminal_grunge/terminal_controls_clean.png` | `975 x 1024` RGBA | `88e8074c17be65403b846c5bed15f294752f7566dda388dcd8e8a8fbc4161dca` |

The runtime must register/preflight every decorative image independently before drawing. A missing, invalid, or unsupported layer leaves the primitive engine backing in place, draws any remaining valid decorative layers, and prints one useful warning. Presentation failure must never crash, blank the terminal, or retain movement/input ownership.

The older public `terminal_controls.png` and historical `terminal_controls_puzzle_v2.png` are not active presentation paths. They may remain as historical repository material until a separate bounded asset cleanup; the active renderer uses `terminal_controls_clean.png`.

The first implementation placed Raid Hat rank at stat index 64 in a fixed 64-entry array and coupled terminal presentation to Raid Hat ownership. The replacement uses explicitly named `STAT_RAID_UI_*` aliases in valid coop-only CTF slots. Raid Hat state is updated independently and is never used as a Raid UI sentinel.

## Keyboard contract

The keyboard artwork and input map are separate assets. Interactive keys retain stable semantic IDs, normalized hit rectangles, optional character/value, label anchors, pressed/dim regions, and roles such as character, clear, submit, cancel, navigation, or action.

Runtime hit testing remains normalized and aligned to the same terminal destination rectangle used by all decorative layers. This permits exact cursor mapping and pressed feedback without baking puzzle letters into the art.

Current approved presentation fragment:

```json
{
  "canvas": [975, 1024],
  "layers": {
    "engine_backing": "engine_rendered_chassis",
    "chassis": "terminal_chassis.png",
    "screen_fx": "terminal_screen.png",
    "controls": "terminal_controls_clean.png"
  }
}
```

Production geometry and hashes are recorded in `ui/terminal_grunge/terminal_layers.json`. The historical `1224 x 1285` source layout is no longer the active runtime canvas.

## Terminal shell modes

The reusable terminal shell remains intended to support menu/navigation pages, external-code entry, safe future camera applications, and short pressure puzzles without new terminal subclasses.

Encounter consequences remain outside terminal C++: terminal actions are semantic events, and encounter JSON decides doors, alarms, phase effects, hazards, rewards, or no action.

## Busy-game permutations

Existing accepted terminal-content concepts remain valid: packet reconstruction, signal routing, bus balancing, frequency lock, memory buffer, camera verification, trace isolation, utility/status pages, and deferred novelty applications. These are content/data permutations of the same terminal shell rather than separate engine primitives.

### Packet reconstruction

The first implementation candidate reconstructs a corrupted callsign/command word from a restricted key set. It proves dynamic labels, hit regions, pressed feedback, input buffer, and submit behavior.

The runtime source currently contains a hardcoded `ALPHA` onboarding proof. Its intended data is mirrored in `raid/terminals/entrance_terminal.json`; hardcoded content remains scaffolding for a later bounded terminal-content ownership pass. This visual presentation work unit does not change that boundary.

The Director-first activation proof is `raid/encounters/terminal_only_test.json`: `trigger_raid_interaction` emits `interact`, then JSON issues the validated `open_terminal` operation. `trigger_raid_terminal` remains only a deprecated compatibility alias.

## First-door onboarding instance

Recommended first proof:

- Terminal sits beside the first locked door.
- Activation holds the terminal presentation without relocating player solidity.
- Screen says `ACCESS TOKEN CORRUPTED`.
- Only the letters required for `ALPHA` are illuminated and labelled.
- Player selects them in order; used keys briefly dim/depress.
- Submit completes the tutorial and emits terminal completion.
- Encounter JSON owns the resulting door/progression consequence.
- Wrong order clears the buffer with obvious feedback but no punishment.
- Use/cancel exits cleanly; reopening behavior follows terminal ownership policy.

Later terminals may swap the onboarding content for other callsigns, codes, menu trees, cameras, or puzzle types without changing the entity class.

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

## Acceptance gates

- Layer failure cannot crash.
- Engine-drawn backing remains available when every decorative PNG is missing.
- PNG chassis is a separate layer over the engine backing, never its replacement.
- All approved decorative layers share one `975 x 1024` destination mapping.
- Every key is hit-tested from normalized geometry and visibly responds.
- Font labels remain independent from keyboard artwork.
- Menu, code, and puzzle data are JSON-selected when the terminal-content loader is connected.
- Terminal actions are semantic events; encounter consequences remain JSON.
- Complete, cancel, death, disconnect, wipe, and map change restore player state through the same idempotent cleanup.
- The first-door tutorial is understandable without combat pressure.
