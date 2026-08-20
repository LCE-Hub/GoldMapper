![Banner](https://raw.githubusercontent.com/LCE-Hub/GoldMapper/refs/heads/main/banner.png)
<div align="center">
  <h1>GoldMapper</h1>
  <p><strong>Input remapper for Minecraft Legacy Console Edition</strong></p>
</div>

GoldMapper injects into the game process to remap keyboard, mouse, and gamepad inputs in real time. Designed for use with the Emerald Launcher.

## How the hell does it work?

GoldMapper consists of two binaries:

| Binary | Role |
|--------|------|
| **`GoldMapperLauncher.exe`** | Creates the target process suspended, injects the DLL, then resumes the game |
| **`GoldMapperLib.dll`** | Hooks into the game's input system (WndProc + XInput) and applies remappings |

---

## Usage

```
GoldMapperLauncher.exe <config.json> <game.exe> [game args...]
```

**Example:**

```
GoldMapperLauncher.exe my_mappings.json Minecraft.Client.exe -name GentooUserBtw
```

---

## Configuration

Use the Emerald Launcher input configurator, or create a JSON file with your desired remappings:

```json
{
  "mappings": [
    { "from": "KEY_Z", "to": "KEY_W" },
    { "from": "PAD_A", "to": "KEY_F11" },
    { "from": "KEY_LEFT_CTRL", "to": "PAD_RB" }
  ]
}
```

### Supported Input Names

**Keyboard** (`KEY_` prefix):

`KEY_A` through `KEY_Z`, `KEY_0` through `KEY_9`, `KEY_F1` through `KEY_F12`, `KEY_SPACE`, `KEY_RETURN`, `KEY_ESCAPE`, `KEY_TAB`, `KEY_BACKSPACE`, `KEY_DELETE`, `KEY_INSERT`, `KEY_HOME`, `KEY_END`, `KEY_PAGEUP`, `KEY_PAGEDOWN`, `KEY_UP`, `KEY_DOWN`, `KEY_LEFT`, `KEY_RIGHT`, `KEY_PRINTSCREEN`, `KEY_PAUSE`, `KEY_CAPSLOCK`, `KEY_NUMLOCK`, `KEY_SCROLLLOCK`, `KEY_LEFT_SHIFT`, `KEY_RIGHT_SHIFT`, `KEY_LEFT_CTRL`, `KEY_RIGHT_CTRL`, `KEY_LEFT_ALT`, `KEY_RIGHT_ALT`, `KEY_LWIN`, `KEY_RWIN`, `KEY_APPS`, `KEY_NUMPAD0` through `KEY_NUMPAD9`, `KEY_MULTIPLY`, `KEY_ADD`, `KEY_SUBTRACT`, `KEY_DECIMAL`, `KEY_DIVIDE`, `KEY_SEMICOLON`, `KEY_EQUALS`, `KEY_COMMA`, `KEY_MINUS`, `KEY_PERIOD`, `KEY_SLASH`, `KEY_GRAVE`, `KEY_LBRACKET`, `KEY_BACKSLASH`, `KEY_RBRACKET`, `KEY_APOSTROPHE`

**Gamepad** (`PAD_` prefix, XInput):

`PAD_A`, `PAD_B`, `PAD_X`, `PAD_Y`, `PAD_LB`, `PAD_RB`, `PAD_BACK`, `PAD_START`, `PAD_LTHUMB`, `PAD_RTHUMB`, `PAD_DPAD_UP`, `PAD_DPAD_DOWN`, `PAD_DPAD_LEFT`, `PAD_DPAD_RIGHT`

For the `to` side, prefix with a pad number to target a specific controller: `PAD1_A`, `PAD2_RB`, etc. Without a number (`PAD_A`), the mapping applies to all controllers.

**Mouse** (`MOUSE_` prefix):

`MOUSE_LEFT`, `MOUSE_RIGHT`, `MOUSE_MIDDLE`

### Cross-Device Mapping

You can map between any input devices:

| From | To | Behavior |
|------|-----|----------|
| `KEY_*` | `KEY_*` | Remaps keyboard keys |
| `PAD_*` | `PAD_*` | Remaps gamepad buttons |
| `MOUSE_*` | `MOUSE_*` | Remaps mouse buttons |
| `PAD_*` | `KEY_*` | Gamepad button triggers a keyboard key |
| `KEY_*` | `PAD_*` | Keyboard key triggers a gamepad button |
| `MOUSE_*` | `PAD_*` | Mouse button triggers a gamepad button |

Gamepad output always uses XInput.

## License
GoldMapper is licensed under the [LGPL-2.1](LICENSE) license.
