# Caligo Surface Ruins — Playable Vertical Slice

## Status

Proposed implementation design. This design turns the existing Caligo Surface
Ruins level into the first environment used to validate first-person combat,
movement, lighting, materials, and environmental readability. The current level
definition already contains a 500-metre square floor, a spawn, solid gameplay
volumes, and a render/collision mesh reference. [src: file: assets/levels/caligo_surface_ruins.json:1-22]

## Outcome

The first deliverable is not a full open world or a downloaded ready-made map.
It is a dense 120 x 120 metre playable research-campus courtyard built around
three routes:

1. A central sand-choked concourse for the baseline combat encounter.
2. A raised, collapsed research wing for vertical movement and long sightlines.
3. A lower transformer service route for close-range combat and a readable
   electrical hazard.

The surrounding 500 x 500 metre Caligo level remains a distant skyline and
storm boundary. That allows the game to test one repeatable gameplay space
without making the player traverse an unfinished world. [src: file: assets/levels/caligo_surface_ruins.json:9-21]

## Authoring Boundary

The engine imports external-buffer glTF models, uncompressed TGA textures, text
materials, and text levels; it compiles them to `.aemesh`, `.aetex`, `.aemat`,
and `.aelevel` respectively. [src: file: docs/systems/asset_pipeline.md:24-34]
Every third-party model should therefore be normalized to a glTF with external
buffers, while texture maps are converted to uncompressed TGA and assigned in
an explicit `.mat` file. `.mat` supports albedo, normal, ORM, and emissive map
references. [src: file: docs/systems/asset_pipeline.md:95-111]

Do not hand-edit compiled assets. Sources and manifest entries are the reviewable
inputs; the importer owns `assets/compiled/`. [src: file: docs/systems/asset_pipeline.md:21-22]

## Free Asset Policy

Use only assets with an asset-specific recorded source and a permissive licence.
The first approved sources are:

- **Poly Haven** for realistic rock, sand, concrete, steel, glass, debris, and
  HDRI reference material. Its assets are CC0 and may be used commercially.
  [src: url: https://polyhaven.com/license]
- **ambientCG** for tileable PBR surfaces and terrain masks. Its public site
  states that its assets are CC0 and usable commercially without attribution.
  [src: url: https://ambientcg.com/]
- **Quaternius Ultimate Modular Sci-Fi Pack** only for temporary modular
  proportions, doors, crates, and interior layout. The pack is marked CC0, but
  its stylised look is not the final Caligo art direction. [src: url: https://quaternius.com/packs/ultimatemodularscifi.html]

Do **not** import the Khronos sample Sponza package as a production map. Its
own repository currently associates the packaged version with CryEngine's
limited licence, and an open licensing report raises a use-restriction concern.
[src: url: https://github.com/KhronosGroup/glTF-Sample-Assets/blob/main/Models/Sponza/README.md]
[src: url: https://github.com/KhronosGroup/glTF-Sample-Assets/issues/172]

For every accepted download, add a short provenance record in
`docs/third-party/` before it is referenced by the manifest. The existing M4A1
record establishes the desired source-and-licence format. [src: file: docs/third-party/m4a1-cc0.md:1-10]

## Art Direction and Material Set

Caligo should read as sun-bleached and electrically damaged rather than as a
generic blue sci-fi facility. The initial material library is deliberately
small:

- sand and wind-carved rock;
- dust-coated concrete and ceramic panels;
- oxidised structural steel;
- cracked safety glass;
- vitrified ground with a cyan emissive fracture map;
- painted corporate paneling with faded amber identifiers.

Each material begins with a low-frequency base texture and a high-frequency
detail texture. Until the renderer has texture layering, artists should bake
the composite into one PBR set per material instead of relying on a runtime
shader feature that does not yet exist. The current material contract supports
only the named PBR map references above. [src: file: docs/systems/asset_pipeline.md:101-111]

## Environment Construction

1. **Greybox first.** Extend the JSON level specification with the courtyard,
   cover, traversal ramps, and collision. The JSON specification can be
   round-tripped to `.lvl` with `tools/levelgen/spec_to_lvl.py`, which keeps
   collision and level placement reviewable without Blender. [src: file: docs/systems/asset_pipeline.md:138-147]
2. **Replace a route at a time.** Start with the central concourse: ground,
   six modular walls, two cover clusters, one landmark tower shell, and one
   lighting state. Preserve its collider while its visual mesh changes.
3. **Add one hero interior.** The Magnetodynamic service room is the first
   fully textured space. It validates interior scale, first-person sightlines,
   PBR readability, and emissive electrical danger before the other two
   buildings receive full interiors.
4. **Use shells for scale.** The remaining buildings are exterior shells with
   blocked doors or shallow lobby spaces until gameplay needs them. Their LOD
   and streaming design should follow only after the playable route is visually
   and mechanically successful.

## Definition of Done for the First Slice

- The player spawns into the central concourse and can complete a loop through
  all three routes without getting caught on collision.
- The environment has at least five reusable PBR materials and each has an
  authored source, manifest record, and provenance record.
- A high-contrast electrical hazard is visible before it damages the player.
- The hero interior has a distinct visual identity from the exterior even when
  seen through a doorway.
- The level compiles through the asset importer and retains an automated
  JSON-to-`.lvl` round-trip test. [src: file: docs/systems/asset_pipeline.md:83-93]
- A display-equipped machine captures first-person screenshots and a fixed
  traversal replay before and after each material or collision milestone.
