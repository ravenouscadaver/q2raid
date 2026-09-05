# Q2Raid Design Notes

## First-door terminal onboarding

- Place the first functional terminal immediately before the first locked door,
  outside combat pressure.
- Use it to teach cursor movement, keyboard interaction, pressed/dim feedback,
  submit, and cancel before terminals appear during encounters.
- Make the first task short and forgiving: reconstruct a simple callsign such as
  `ALPHA`; success emits a semantic action and encounter JSON opens the door.
- This establishes the mod's thesis immediately: the machinery is recognisably
  Quake II, but the expected play vocabulary is not ordinary Quake II.
- Full terminal shell, puzzle permutations, art contract, and camera composition
  are defined in `TERMINAL_SYSTEM.md`.

## Powered emissive fixtures

- Pair a dull permanent base face with a slightly offset toggleable emissive
  `func_wall` overlay and its actual light sources.
- Existing Director `fire_target` can prove synchronized shutdown immediately; later a
  generic `set_powered` group can coordinate visibility, light, sound, and VFX.
- Do not require an overlap trigger merely to discover ordinary authored
  fixtures. See `POWERED_SURFACES.md`.

## Modular BSP kit generator

- Author simple reusable tile brushes in a source `.map`, enclosed by bounds
  volumes and marked with generator-only connector textures/metadata.
- An offline constraint solver assembles an editable `.map` blockout, preserving
  its seed and stripping generator markers.
- Generated geometry is an inspiration/cleanup substrate for JACK, not runtime
  procedural generation. See `MODULAR_BSP_GENERATOR.md`.

## Shield-drone laser-field proof encounter

- Build a traversal room blocked by a named laser field.
- Attach a `raid_hat` to the flying laser-firing support monster (exact stock classname to be confirmed from source/wiki).
- Treat it as a **Shield Drone**: killing the hatted monster emits `monster_killed` to JSON.
- JSON disables the laser field and advances the encounter presentation.
- Purpose: prove the missing enemy-owned immunity/shield-gate component needed for the opening encounter emulation.

## Held-open circuit bypass

- Present a malfunctioning powered door with its access panel visibly torn away and broken wiring exposed.
- Implement the interaction with a small invisible hold volume at normal use distance; the mapper-facing fiction is repairing the circuit, not standing on a floor plate.
- While a player maintains the interaction, force the existing third-person unarmed presentation and suppress weapon use. Releasing, displacement, downing, death, or wipe must immediately end the interaction and restore normal presentation.
- Fire intermittent sparks from the exposed panel and show a development message such as `DOOR CIRCUIT TEMPORARILY REPAIRED` as immediate feedback. Final presentation should communicate the same state without relying on debug text.
- Keep the door open only while the interaction remains valid, creating a committed operator role while teammates transport an objective through it.
- Pair it with a Volatile power core whose socket cannot normally be reached within its 15-second carry window. Holding the bypass opens the shorter viable route.
- Combine the transport pressure with Shield Drones, laser fields, Volatile consequences, and monster-door population so the opening encounter demands coordination rather than becoming an uncontested carry exercise.
- Generalize the logic as a JSON-controlled hold interaction so it can later represent jammed shutters, exposed relays, coolant valves, manual pumps, or other sustained repairs.

## Power-core dunk presentation

- Preserve the current satisfying deposit sound as the baseline.
- On successful dunk, briefly force a third-person jump/impact presentation that lands into a short crouch, then returns cleanly to first person.
- Keep input lockout short and deterministic; never leave the player stranded in third person.
- Sound direction: layered *twinkle -> mechanical confirmation -> restrained impact*, rewarding but not intrusive or slot-machine excessive.
- Presentation should be JSON-selectable so ordinary deposits can remain lightweight and major deposits can receive the full animation/sound treatment.

## Gib-gobbling escalation monster

- Prototype a monster or `raid_hat` behaviour that detects nearby physical gibs, moves to them, and consumes them.
- Each consumed gib can grant configurable health, armour, size, damage, speed, a stack counter, or unlock a stronger attack.
- Its threat is legible and interruptible: players see it feeding and can kill, stagger, or displace it before it snowballs.
- Candidate encounter role: an anti-farming pressure unit deployed when a room accumulates excessive remains.
- Gib consumption must distinguish disposable gore from objectives, corpses required by another mechanic, and reset-owned entities.
- Director-facing signals could include `gib_consumed`, `feed_threshold`, and `fully_fed`; JSON decides the consequences.

## Shadow-seeking enemy behaviour

- Explore a reusable `raid_hat` AI preference that scores navigation destinations by darkness and attempts to remain outside brightly lit player sightlines.
- Strong candidate: the tentacled dog-like enemy, used as a lurking ambusher that retreats or relocates through shadow.
- Audit engine-accessible light sampling first; do not infer darkness only from mapper labels if reliable world-light information already exists.
- Allow mapper-authored fallback shadow anchors for deterministic encounter composition and engines/maps where useful light sampling is unavailable.
- Distinguish a soft preference from a hard rule: combat, leash, path safety, and valid navigation must remain authoritative.
- Bright flashes or activated room lighting could temporarily flush these enemies from cover and become encounter counterplay.

## Limited downed and revival state

- Cooperative PvE may grant a marine one downed state before full death and matter reconstruction.
- A downed player can crawl slowly toward cover but cannot fight normally; avoid an extended helpless-state punishment.
- Teammates can perform a committed revive interaction within a configurable window.
- Successful revival keeps the fireteam together but returns the marine in a vulnerable, configurable condition.
- Expiry, severe finishing damage, environmental hazards, or a full-team down advances to actual death/wipe handling.
- Matter reconstruction remains station-based and is the lore-consistent fallback; marines do not possess unlimited personal resurrection devices.
- Encounters can disable downing, modify the timer, limit revives, or apply revive consequences through JSON.
- Preserve clean accounting across downed, revived, dead, spectator, wipe, and reconstruction states.

### Prototype damage gate

- A normally fatal hit that would leave the marine between `0` and `-24` enters the downed state instead of death.
- A hit that crosses directly to `-25` or below causes normal death.
- Any valid damage taken while already downed causes normal death, even if it is only one point.
- Gib/disintegration-class outcomes always cause immediate death regardless of the numerical threshold.
- Internally, keep a separate `downed` flag and safe engine health value so stock `health <= 0` checks do not prematurely classify the player as dead or trigger a team wipe; the HUD may still present zero health.
- Make the `25`-damage overkill allowance configurable after the prototype proves reliable.

## Optional crouch long-jump

- Add an opt-in Half-Life-style movement permutation: a deliberate crouch-and-jump input while grounded and already moving launches the player in a long jump.
- Derive the horizontal launch vector from the player's current planar movement direction, not merely their view angle. Require a configurable minimum input/velocity so it cannot launch from rest.
- Keep stock Quake II movement unchanged by default. Expose the feature and its forward speed, vertical impulse, and optional cooldown as mapper/server configuration.
- Require a fresh jump press and a valid grounded/coyote-grounded state; merely holding crouch in the air must never retrigger it.
- Consume the jump cleanly so it cannot accidentally compound with the optional double jump unless an encounter explicitly permits that combination.
- Preserve ordinary player collision and knockback during launch and landing. Teammates impatiently crowding a traversal lane can therefore bump one another off course as emergent cooperative chaos.
- Provide clear launch/landing feedback and deterministic server-authoritative behaviour so clients agree on the arc.
- Initially allow direct player/server enablement for testing; later present it as **Strogg leg actuators** granted through the general implant/status system.

## Strogg implants as a player-facing mechanic family

- Treat **Strogg Implant** as a reusable lore layer rather than the permanent name of one narrowly defined buff.
- Individual implants may grant long-jump actuators, safe passage through disintegration fields, movement changes, environmental tolerances, sensory changes, weapon interactions, or other encounter permissions.
- Let JSON define the implant's mechanical capabilities, duration, acquisition, loss rules, HUD label, and presentation while the DLL supplies reusable hooks.
- Prefer specific player-facing names where clarity matters, with `Strogg implant` retained as the shared category/source. This avoids making unrelated mechanics feel like arbitrary game permissions without forcing every implant to behave identically.

## Raid Hat HUD presentation status

- Current monster name and red health-bar presentation is accepted as implemented: scale, readability, and targeting behaviour are suitable.
- Segmented Destiny-style bars and the earlier text-formatted bar remain optional presentation polish, not blockers for the Raid Hat system.

## Observer-Locked Raid Hat controls

- Preserve the proven default relationship: a watched monster freezes and an unobserved monster may move.
- Add a live targetable/JSON-controlled inversion switch. While inverted, the monster may move while watched and freezes when every player looks away, forcing the fireteam to deliberately play "look away."
- Expose separate observation timing controls: delay before movement is released after the relevant observation change, delay before freezing resumes, and an optional minimum active movement window.
- Evaluate observation across all living players: the current rule or its inverse must use the combined fireteam view state, not only the nearest player.
- Emit a clear event when inversion changes so JSON can synchronize lights, sound, messaging, and encounter state with the rule swap.

## Deferred custom-model production

Status: CONCEPT / deferred — not an active day goal.
Date: 2026-09-05.

- Possible custom model for Dr Oliver; role, appearance and required animation set remain undecided.
- Consider a couple of bespoke Raid Hat enemy models where their presentation materially matters to gameplay.
- The user has hard-surface modelling experience in Blender; rigging and animation are the main production bottleneck. This constraint currently favours passive implant abilities and feedback using available particles, beams and other existing presentation.
- Contractor lead to revisit: [Iammr R / iammrromeo on Fiverr](https://www.fiverr.com/iammrromeo/create-3d-game-character-and-game-mod-with-custom-animation-for-skyrim-unity-ue). The listing advertises low-poly assets, rigging, custom animation and game-mod work. This is an unverified lead, not a vetted hire or evidence of MD2 expertise; no contact or commission has been made.
- Proposed qualification gate, if this work is activated: review actual animation examples and agree source-file delivery, then commission a small Blender animation test before a complete character. Test fixed topology, named frame ranges, MD2 export and playback inside Q2Raid.
- Keep editable Blender source separate from the final exported MD2. Exporter/Blender compatibility and the complete conversion pipeline remain NOT TESTED; do not treat a listing or available exporter as proof.
- No contractor spend, asset production or implementation is authorized by recording this note.

## Dr Oliver — Dangling Surgical Platform — pre-variant snapshot

Status: CONCEPT / preserved design return point, not implementation-ready.
Date: 2026-09-05.
Purpose: preserve the current design before the user's next experimental variant. Future variants do not overwrite this snapshot.

### Visual reference and retrieval

- Reference: the latest dark laboratory image in the Dr Oliver model/animation design conversation, immediately before the request to preserve the current state.
- Exact generated image identifier: `exec-2236ccb0-0bc2-460c-a3f7-7b2ea726d03e.png`.
- Image description: full-body Oliver scanning a dark industrial laboratory with a flat red optical laser plane; blue chest core, dangling human torso, surgical arms above, large hooked-under pincer beneath, two massive supporting legs.
- The image was generated in ChatGPT and is not a repository asset. Its identifier is a retrieval aid, not a durable GitHub image URL. Retrieve that existing image from the conversation when exact visual continuity is needed; regeneration is not an identical restoration.
- Return-point phrase: **Dr Oliver — Dangling Surgical Platform — pre-variant snapshot**.
- Preserve both the image's composition and the written corrections below; image errors do not supersede user requirements.

### User-defined anatomy and equipment

- Heavily self-modified human scientist boss, nearly ten feet tall when powered. Exaggerated machinery proportions, but normal adult human skull size and mostly recognizable human face/mouth for speaking.
- Huge bowed Strogg leg structures attach to the large bulbous upper-back weapons/machinery mass. Legs must visibly bow outward, not merely stand apart or bend backward.
- Human pelvis may be absent. Torso is explicitly DANGLING between the leg structures, swaying independently rather than rigidly seated on a mechanical waist. Upper body remains hunched by unnatural bulk beneath stretched skin.
- Back machinery is not entirely contained by skin. Partial stretched/harvested skin coverage, exposed machine mass and leg attachment points should remain evident.
- Two rows of prominent metallic octagonal CYLINDRICAL implants flank the spine. Stegosaurus-like protruding prominence, not triangular plates or little flush square tiles. Vary lengths, diameters and surface appearance.
- Implant options are mixed: plain caps, occasional LEDs, exposed unused connectors, direct flesh connections, and auxiliary hoses plugged into the SIDES of cylinders. No LED-per-implant rule.
- Cables hang down and return upward into the abdomen/core connections. Vary thickness and texture, especially large ribbed hoses alongside braided and smooth lines.
- Large SQUARE BLUE power core seated INSIDE split ribcage, glowing outward through the ribs. Its BIG handle protrudes slightly DOWNWARD from stomach/abdomen. The narrow yellow tube and later narrow blue rectangular cell are incorrect substitutes. Exact existing project core appearance should be referenced before further visual refinement.
- Retain one human hand for terminal access and handling machinery/cores.
- Other hand has ocular finger capable of probing around cover; maintain a recognizable hand and finger, not a freestanding periscope.
- Additional ocular implants modify skull; chest/shoulder oculars remain possible.
- Lab coat shreds hang about shoulder with Dr Oliver name badge attached.
- Integrated combat implants include a personal missile salvo system, comparable in role to the Tank's but aesthetically distinct. A reveal view shows machinery causing the unnatural upper-back bulk.
- Additional back-platform surgical robot armatures carry syringe/injector tools, circular bone saws and clamps.
- One large arm mounts on the REAR of the weapons platform, hangs down and hooks back underneath, ending in a jaws-of-life-style pincer grip. It serves as a multipurpose foot/arm appendage.
- Flamethrower was floated then explicitly deferred; not part of this snapshot's equipment.

### Motion, states and planned studies

- Powered: legs extend to full height, covering ground rapidly in HUGE steps, with dangling torso/cable follow-through.
- Stunned, core destroyed, or selected other behaviour states: legs fold approximately in half, greatly lowering him while retaining a crouch-walk. This communicates state change and vulnerability. Do not shrink the leg segments.
- Study list: powered full body; rear implant/cable view; combat-hardware reveal; core-hit stagger; low stunned crouch-walk; replacement-core extraction; separate ocular-finger search vignette.
- Recharge: Oliver hunches over NORMAL HUMAN-SIZED equipment and extracts a replacement core to restore shield/power according to the eventual boss phase. Exact phase rules remain open.
- Search vignette: marines hiding behind cover, ocular finger probing around it, owner's menacing shadow behind.
- Current atmospheric direction: dark industrial laboratory, optics scanning surroundings, a FLAT RED LASER PLANE passing across the environment.

### Character and encounter intent

- He mostly addresses an unseen observer/new master in the darkness, calling for empowerment, curses against the marines, and rescue as he dies.
- Marines receive little direct acknowledgement; the user's example is asking for power to smite these insects.
- Do not harden assistant-suggested dialogue, claims that the observer ordered each surgery, or an exposition-heavy personality as approved lore.
- Shield recharge and core-related vulnerability are design concepts; timings, attacks, exact consequences and JSON phase conditions are not specified or runtime-tested.

### Known image deviations to correct

- Latest image still does not establish the required outward leg bow sufficiently.
- Installed power core is still too narrow/rectangular and not an accurate depiction of the existing square blue handled core.
- Implant variety, visibly unused connectors and side-entry hose connections need continued attention; do not revert to uniform lit tiles.
- Normal human head scale, substantial exposed back machinery and the precise dangling support arrangement remain explicit checks for subsequent variants.
- Generated slogans, background branding, extra staff and incidental composition details are not accepted lore merely because they appeared in images.

### Reconstruction prompt for this state

Create one dark cinematic full-body view of Dr Oliver, a nearly ten-foot Strogg-modified scientist with a normal-sized recognizable human head and a hunched torso dangling loosely between enormous outward-BOWED mechanical legs. Both legs mount high onto a huge bulbous upper-back weapons and surgical machinery platform, partly covered by stretched skin but visibly exposed. Two rows of large protruding octagonal cylindrical implants flank his spine; vary their size, caps and finish, with sparse LEDs, unused connectors and thick hoses visibly plugged into their sides. Varied corrugated, braided and smooth cables hang down and loop upward into his abdomen. A large SQUARE BLUE industrial power core glows inside his split ribcage; its big handle protrudes slightly downward from the abdomen. Preserve one human hand, a separate ocular-finger hand, shoulder lab-coat scraps and name badge. Add articulated injector, circular saw and clamp tools to the platform, plus a large rear-mounted arm dropping down and hooking underneath into a jaws-of-life pincer that can brace as an auxiliary foot. Show him searching a human-scale industrial laboratory with a flat red optical scanning plane visible in haze. Keep the body dangling, the leg bow unmistakable, and the human head human-sized. No flamethrower. This restores the design direction; it does not reproduce the archived image pixel-for-pixel.
