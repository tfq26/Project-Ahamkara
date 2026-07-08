# Rendering Map

Status: Seed

Use this map before changing debug rendering, renderer backends, materials,
meshes, maps, text, or PBR/shadow paths.

## Canonical Docs

- [Renderer backend](../../systems/renderer_backend.md)
- [Architecture](../../systems/architecture.md)
- [Render backend report](../../reports/subagents/render-backend-abstraction.md)
- [Debug UI GPU profiling report](../../reports/subagents/debug-ui-gpu-profiling.md)

## Main Areas

- `engine/render/`
- `engine/render/include/ae/render/`
- `engine/render/src/`
- `engine/render/shaders/`
- `client/src/debug_render_runtime.cpp`
- `client/src/debug_scene_bridge.cpp`
- `tests/src/asset_pipeline_tests.cpp`

## Agent Checks

- Full render verification may require local `debug` preset and a windowed run.
- Asset format changes should be cross-checked with the asset pipeline map.
- Keep backend boundaries clean; avoid pushing visual-only details into generic
  renderer interfaces without a reason.

## Related

- [Asset pipeline map](asset-pipeline-map.md)
- [Known good commands](../memory/known-good-commands.md)
