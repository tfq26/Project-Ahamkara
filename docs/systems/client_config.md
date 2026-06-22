# Client Configuration

The Ahamkara client reads an optional `ahamkara.cfg` file on startup.
If the file is missing every setting falls back to the sensible default
shown below — the client starts with no extra ceremony.

## Config file location

Place `ahamkara.cfg` in the working directory from which you launch the
client. A sample is provided at `client/config/ahamkara.cfg`.

## Format

```
# Lines starting with '#' are comments.
# Blank lines are ignored.

key = value
```

Whitespace around the key and value is trimmed. Unknown keys are logged
as a warning and skipped. Invalid values fall back to the default.

## Available keys

| Key                 | Type    | Default       | Description                           |
|---------------------|---------|---------------|---------------------------------------|
| `window_width`      | int     | `1280`        | Client window width in pixels         |
| `window_height`     | int     | `720`         | Client window height in pixels        |
| `fullscreen`        | bool    | `false`       | Start in fullscreen (`true` / `false` or `1` / `0`) |
| `mouse_sensitivity` | float   | `1.0`         | Mouse look multiplier                 |
| `server_ip`         | string  | `127.0.0.1`   | Default server IPv4 address           |
| `audio_enabled`     | bool    | `true`        | Master audio kill switch               |
| `audio_master_volume` | float | `1.0`         | Master output volume (0.0–1.0)        |
| `audio_sfx_volume`  | float   | `1.0`         | SFX bus volume (0.0–1.0)              |
| `audio_weapon_volume`| float  | `1.0`         | Weapon bus volume (0.0–1.0)           |
| `audio_ui_volume`   | float   | `1.0`         | UI bus volume (0.0–1.0)               |
| `audio_music_volume`| float   | `1.0`         | Music bus volume (0.0–1.0)            |
| `audio_ambient_volume`| float | `1.0`         | Ambient bus volume (0.0–1.0)          |

## Overriding the server IP

A server IP supplied on the command line always takes precedence over
the config entry:

```sh
# Uses server_ip from ahamkara.cfg (or 127.0.0.1 by default)
./ahamkara_client

# Overrides the config
./ahamkara_client 192.168.1.50
```

The `--sandbox` and `--window` flags do not use the server IP.

## Example

A typical `ahamkara.cfg` for development:

```ini
window_width     = 1920
window_height    = 1080
fullscreen       = false
mouse_sensitivity = 2.5
server_ip = 127.0.0.1

# Audio (optional, defaults shown)
audio_enabled       = true
audio_master_volume = 1.0
audio_sfx_volume    = 1.0
audio_weapon_volume = 1.0
audio_ui_volume     = 1.0
audio_music_volume  = 1.0
audio_ambient_volume = 1.0
```
