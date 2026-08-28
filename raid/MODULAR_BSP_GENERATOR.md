# Modular BSP Kit Generator

Status: offline authoring-tool concept; explicitly outside the game DLL.

## Goal

Feed a constrained set of mapper-authored `.map` tiles into a generator and
produce a valid, deliberately rough Quake II `.map` composition. The generated
monstrosity is inspiration and blockout material: the mapper opens it in JACK,
keeps the interesting accidents, and cleans it into intentional architecture.

The generator never runs during gameplay and never makes the Director own level
geometry.

## Source kit

One source `.map` contains reusable tiles. Each tile has:

- stable tile ID and category;
- rectangular bounds and local origin;
- rotation/mirroring permissions;
- weighted rarity;
- connector sockets on its boundary;
- compatibility tags for each connector;
- optional semantic anchors for doors, lights, encounters, cameras, and props;
- optional vertical level/elevation rules.

## Mapper marking scheme

Preferred proof format:

- Enclose each tile in a dedicated bounds trigger/volume.
- The volume's minimum corner defines the tile-local origin; dimensions define
  its occupied grid footprint.
- Give the volume a tile ID/type and generator-only metadata.
- Place thin connector marker brushes on permitted exits.
- Use generator-only tool textures such as:
  - `raidgen/socket_corridor`
  - `raidgen/socket_door`
  - `raidgen/socket_power`
  - `raidgen/socket_vertical_up`
  - `raidgen/socket_vertical_down`
  - `raidgen/blocked`
- Connector brushes carry direction, socket class, and optional partner tags.
- Generated marker brushes are stripped or converted to `skip`/`nodraw` before
  the output map is compiled.

`func_wall` can group the visible tile geometry during the first proof, but
generator identity should live in explicit metadata rather than relying on the
runtime behaviour of `func_wall`.

## Constraint model

Each boundary socket defines what may touch it. A tile may be placed when:

- footprints do not overlap;
- connector directions oppose one another;
- socket types are compatible;
- elevation aligns;
- required/forbidden neighbour tags pass;
- global quotas and reachability constraints remain satisfiable.

The initial solver may use weighted backtracking. Wave Function Collapse becomes
useful when the kit is large enough that adjacency propagation saves work. The
output should store its seed so an interesting failure can be regenerated.

## Useful global constraints

- one reachable entrance and exit;
- all critical-path sockets connected;
- optional loops and dead ends;
- minimum/maximum room counts by category;
- no impossible vertical transitions;
- reserved encounter footprints;
- distance bands between entrance, objective, and exit;
- optional symmetry/asymmetry bias;
- lighting/power-network continuity tags;
- route width suitable for Quake II monsters and cooperative players.

## Output

The tool writes a normal editable `.map` containing:

- transformed copies of chosen tile brushes/entities;
- stable generated targetnames;
- a worldspawn comment containing seed and kit version;
- optional debug connector/bounds layer;
- a generation report listing rejected sockets, forced choices, unreachable
  regions, and quota results.

It does not emit a BSP directly. JACK remains the cleanup/art-direction stage,
and the established QBSP/VIS/LIGHT pipeline remains authoritative.

## Relationship to Q2Raid JSON

The generator may place and name physical primitives or output a starter
encounter manifest, but it does not invent encounter rules. Generated anchors
can become explicit references in later JSON:

```json
{
  "anchors": {
    "encounter_room": "gen_room_07",
    "terminal": "gen_terminal_02",
    "power_group": "gen_power_03"
  }
}
```

The mapper then authors or edits the actual state machine.

## First proof

1. Build six to ten simple orthogonal tiles in one `.map`.
2. Mark bounds and north/east/south/west corridor sockets.
3. Parse brushes/entities and normalize each tile to its bounds minimum.
4. Assemble a small rectangular grid with weighted backtracking.
5. Transform brush planes and entity origins correctly under 90-degree
   rotations.
6. Strip generator markers and write an editable output `.map`.
7. Open in JACK, inspect seams, compile manually, and record which accidents are
   actually inspiring.

Only after that proof should the tool attempt multi-cell footprints, vertical
connectors, mirroring, encounter anchors, or full WFC propagation.
