# Q2Raid Modified-Unit Provenance Matrix

**Seed audit:** 2026-09-05  
**Canonical branch:** `director-scaffold`  
**Audit basis HEAD:** `faae89f0baad042a348338660f8d2143e407120e`

## Classification vocabulary

- `CANONICAL CURRENT` — authoritative current source location; not automatically runtime proof.
- `PROTECTED` — runtime-passing behavior; do not alter absent a new defect or unavoidable shared-code requirement.
- `CURRENT / INCOMPLETE` — authoritative source with known architectural/runtime work remaining.
- `CURRENT / EXPERIMENTAL` — intentional implementation with design provenance, not yet a hardened slice dependency.
- `PROOF FIXTURE` — test/example data, not production schema authority.
- `GENERATED DERIVATIVE` — edit the canonical source instead.
- `SUPERSEDED / ARCHIVE` — historical evidence only.
- `RECOVERABLE OFF-CANONICAL` — intentional historical implementation; recover reviewed hunks only.
- `CONFLICTING` — current ownership/data representations disagree or duplicate each other.
- `ABSENT / ACCEPTED DESIGN` — architecture accepted but implementation not recovered in GitHub.

## Build / registration

| Unit | Classification | Rule |
|---|---|---|
| `.github/workflows/windows-build.yml` | CANONICAL CURRENT | Push to `director-scaffold` triggers CI. |
| `rerelease/game.vcxproj` | CANONICAL CURRENT | Current runtime-unit registry. |
| `rerelease/game.vcxproj.filters` | CANONICAL CURRENT | IDE grouping only. |

## FGD

| Unit | Classification | Rule |
|---|---|---|
| `fgd/q2raid.fgd` | **CANONICAL CURRENT** | Sole hand-maintained mapper contract. |
| `fgd/jack_merged/q2raid_merged.fgd` | GENERATED DERIVATIVE | Distribution snapshot; not authority. |
| `fgd/jack_merged/INSTALL.txt` | GENERATED DERIVATIVE | Operational helper only. |
| `raid/archive/fgd/raid_legacy_standalone.fgd` | SUPERSEDED / ARCHIVE | Evidence only. |

## Encounter / terminal JSON

| Unit | Classification | Note |
|---|---|---|
| `raid/encounters/carnage_report_test.json` | PROOF FIXTURE / HISTORICAL-CONFLICT | Its `mechanic_complete` state is not Carnage accounting authority. |
| `raid/encounters/core_phase_progression_test.json` | PROOF FIXTURE / PROTECTED-ADJACENT | Core path is PASS / FINISHED. |
| `raid/encounters/director_bridge_test.json` | PROOF FIXTURE | Basic exact `source + signal` bridge evidence. |
| `raid/encounters/party_bridge_test.json` | PROOF FIXTURE | Test data only. |
| `raid/encounters/phase3_bridge_test.json` | PROOF FIXTURE | Historical/current integration proof. |
| `raid/encounters/phase4_systems_test.json` | PROOF FIXTURE | Reset/relic/status proof; not persistence proof. |
| `raid/encounters/phase7_encounter_test.json` | PROOF FIXTURE | Broader systems test. |
| `raid/encounters/status_bridge_test.json` | PROOF FIXTURE | Status machinery proof. |
| `raid/encounters/terminal_only_test.json` | PROOF FIXTURE | Terminal still requires runtime closure. |
| `raid/terminals/entrance_terminal.json` | **CANONICAL DATA CANDIDATE / CONFLICTING** | Duplicates hard-coded onboarding puzzle data. |
| `raid/ui/terminal_grunge/terminal_layers.json` | CURRENT / OUTDATED | Predates approved three-PNG rollback stack. |

## Engine-family integration files

| Unit | Classification | Q2Raid ownership / rule |
|---|---|---|
| `rerelease/bg_local.h` | CANONICAL CURRENT / PROTECTED-ADJACENT | Raid stat/config slots. |
| `rerelease/cg_main.cpp` | **PROTECTED-ADJACENT** | `+raid_grenade` / `-raid_grenade`. |
| `rerelease/cg_screen.cpp` | CANONICAL CURRENT | Q2Raid cgame UI draw entry. |
| `rerelease/ctf/p_ctf_menu.cpp` | CURRENT / INCOMPLETE | Terminal menu/input infrastructure. |
| `rerelease/ctf/p_ctf_menu.h` | CURRENT / INCOMPLETE | Terminal menu declarations. |
| `rerelease/g_ai.cpp` | CANONICAL CURRENT / PROVENANCE-SMEARED | No canonical Q2Raid event publisher. |
| `rerelease/g_cmds.cpp` | CANONICAL CURRENT / PROTECTED-ADJACENT | Raid dev commands + grenade dispatch. |
| `rerelease/g_combat.cpp` | CURRENT / INCOMPLETE | Downed fatal-damage interception. |
| `rerelease/g_local.h` | CANONICAL CURRENT | Shared high-impact integration fields. |
| `rerelease/g_main.cpp` | CANONICAL CURRENT | Director/subsystem lifecycle. |
| `rerelease/g_monster.cpp` | **PROTECTED-ADJACENT** | Raid Hat kill hook. |
| `rerelease/g_save.cpp` | CURRENT / INCOMPLETE | Current Director level-save hook only. |
| `rerelease/g_spawn.cpp` | **CANONICAL HIGH-RISK INTERFACE REGISTRY** | Classnames + semantic mapper aliases. |
| `rerelease/g_svcmds.cpp` | CANONICAL CURRENT | Raid server/dev commands. |
| `rerelease/g_trigger.cpp` | CANONICAL CURRENT | Generic trigger -> `activate` fact. |
| `rerelease/p_client.cpp` | CANONICAL CURRENT / MIXED EVIDENCE | Grenade protected; downed untested; movement optional/default-off. |
| `rerelease/p_hud.cpp` | **PROTECTED** | Raid Hat name/health/shield presentation. |
| `rerelease/p_view.cpp` | CANONICAL CURRENT / SHARED | Raid view/presentation effects. |
| `rerelease/p_weapon.cpp` | **PROTECTED-ADJACENT** | Protected grenade/carry interaction. |
| `rerelease/q_vec3.h` | CANONICAL CURRENT / UTILITY | No mapper/Director string authority. |

## Q2Raid-specific runtime units

| Unit | Classification | Rule |
|---|---|---|
| `rerelease/cg_raid_ui.cpp` | **CURRENT / INCOMPLETE** | Post-audit patch removes fixed `MECHANIC PROGRESS`; terminal still uses old image stack until next work unit. |
| `rerelease/cg_raid_ui.h` | CANONICAL CURRENT | Glue only. |
| `rerelease/raid_bots.cpp` | CURRENT / EXPERIMENTAL | Generic helper-bot capability. |
| `rerelease/raid_bots.h` | CURRENT / EXPERIMENTAL | Bot declarations. |
| `rerelease/raid_director.cpp` | **CURRENT / INCOMPLETE** | Base bridge/ops/status/reset exist; accepted persistent/checkpoint/live split absent. |
| `rerelease/raid_director.h` | CURRENT / INCOMPLETE | Do not create a parallel event bus. |
| `rerelease/raid_downed.cpp` | **CURRENT / COMPILED / NOT RUNTIME TESTED** | Do not patch corpse again before test. |
| `rerelease/raid_downed.h` | CURRENT / COMPILED / NOT RUNTIME TESTED | Same evidence boundary. |
| `rerelease/raid_grenade.cpp` | **PROTECTED — FINAL PASS / LOCKED** | Owning source matched runtime evidence. |
| `rerelease/raid_grenade.h` | **PROTECTED — FINAL PASS / LOCKED** | Do not touch absent new defect. |
| `rerelease/raid_hats.cpp` | **PROTECTED PRESENTATION + CURRENT BEHAVIOR** | Name/health/shield PASS / COMPLETE. |
| `rerelease/raid_hats.h` | PROTECTED-ADJACENT | Preserve protected interface. |
| `rerelease/raid_items.cpp` | **CANONICAL CURRENT; CORE PATH PROTECTED** | Core path PASS / FINISHED. |
| `rerelease/raid_items.h` | CANONICAL CURRENT / CORE-PROTECTED-ADJACENT | Preserve core contract. |
| `rerelease/raid_monsters.cpp` | CANONICAL CURRENT | Door-local events; no generic AI `monster_arrived`. |
| `rerelease/raid_monsters.h` | CANONICAL CURRENT | Do not replace wholesale during event recovery. |
| `rerelease/raid_reconstruction.cpp` | CURRENT / EXPERIMENTAL-TO-INCOMPLETE | Intentional matter-reconstruction flow. |
| `rerelease/raid_reconstruction.h` | CURRENT / EXPERIMENTAL-TO-INCOMPLETE | Same owner. |
| `rerelease/raid_terminal.cpp` | **CANONICAL CURRENT / TERMINAL INCOMPLETE** | Generic interaction + deprecated wrapper. |
| `rerelease/raid_terminal.h` | CURRENT / INCOMPLETE | Do not resurrect historical parallel headers. |
| `rerelease/raid_thirdperson.cpp` | CANONICAL CURRENT / SHARED | High regression radius. |
| `rerelease/raid_thirdperson.h` | CANONICAL CURRENT / SHARED | Same. |
| `rerelease/raid_ui.cpp` | **CURRENT / INCOMPLETE** | Post-audit patch removes state-derived mechanic accounting and adds per-player unsolved-exit re-entry cooldown. Terminal content ownership and generic Carnage rows remain future work. |
| `rerelease/raid_ui.h` | CANONICAL CURRENT | Shared UI API. |
| `rerelease/raid_ui_shared.h` | **CURRENT / CONFLICTING** | Hard-coded onboarding puzzle duplicates terminal JSON. |

## Off-canonical recovery candidates

| Feature | Candidate | Classification | Recovery rule |
|---|---|---|---|
| Qualified event routing | source `9dbe2a00...` + numeric fix `e795d0ce...`; recovered gameplay lineage `bf70733e...` | **RECOVERABLE OFF-CANONICAL** | Reviewed event-specific hunks only. |
| WU3 documentation descendant | `9a8b2a73...` | RECOVERABLE OFF-CANONICAL / DOC-ONLY DELTA | Same gameplay source plus audit docs. |
| Persistent/checkpoint/live Director | none found in recoverable GitHub source | **ABSENT / ACCEPTED DESIGN** | Implement deliberately when Director work resumes. |
| Approved terminal graphics | exact approved three-PNG set | **APPROVED / STAGED FOR NEXT WORK UNIT** | Exact bytes/paths only; no regeneration/renaming. |

## Superseded / non-authoritative implementation names

- `raid_player_pose.*`
- `raid_terminal_shared.h`
- `integration` as canonical branch
- `pics/raid/...`
- `terminal_controls_puzzle_v2.png` as active runtime layer
- same-SHA recovery aliases treated as independent implementations

This matrix is the default ownership lookup before touching a Q2Raid-modified unit.
