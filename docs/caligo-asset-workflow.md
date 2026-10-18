# Caligo asset workflow

Run the Caligo generator from the repository root:

```sh
./tools/blender/run_caligo.sh --out_dir assets/models
```

The launcher probes Blender before invoking Python. This matters on macOS:
some Blender installations crash during native startup, before `bpy` or the
generation script can execute. When that happens, the launcher automatically
uses the deterministic glTF blockout path in `build_caligo.py` and exits with
the same asset set. It does not modify or replace the Blender installation.

On a healthy Blender installation, the launcher runs:

```sh
blender --background --factory-startup --threads 1 \
  --python tools/blender/build_caligo.py -- --out_dir assets/models
```

After generation, compile the assets with:

```sh
./build/debug/tools/ahamkara_asset_importer --manifest assets/manifest.assets
```

The supplied Blender 5.1.2 crash report shows a SIGSEGV in Blender's bundled
`libIlmThread` during startup. The probe is therefore expected to fail on that
installation; use a current Blender build on the Windows workstation for the
full `.blend` authoring path.
