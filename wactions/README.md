# WActions
This script will apply a keymap for the corresponding buttons of tablet,
defined by a configuration file.

## Dependencies
- `xdotool` (to perform action)
- `xprop` (to check for window class)

## Configuration
WActions will look for configuration file in `$HOME/.config/wactions/config` by default.

Configuration is in INI format.

### Sections
- `[default]`
- `[layer_<number>]` (for example `[layer_1]`)
- `[window_class]` (for example `[krita]`)
- `[window_class/layer_<number>]`

`[default]` section will act as a fallback for all layers. For example,
if `button_2` is not set on `[layer_1]` but it is set under `[default]`,
it will use the default keymap.

Same thing goes for window class. For a fallback keymap, use `[window_class]` section,
and for other layers, use `[window_class/layer_<number>]`.

<br>
To get window class:

```bash
$ xprop WM_CLASS # Then click on the window
WM_CLASS(STRING) = "krita", "krita"
```
The first item should be the window class.

### Buttons
- `button_<number>`  from 1 to 8. Example: `button_1`
- `ring_cw` when ring is rotated clockwise
- `ring_ccw` when ring is rotated counterclockwise

### Mapping
This script uses `xdotool` to perform actions. Basically, all key rules apply here.

For example, `button_1 = ctrl+z`. This maps the first button to Ctrl+Z. Check `man xdotool` (under "KEYBOARD COMMANDS" section).

You can also map keys to run a custom script. Add `run:` prefix to the command you want to run.

Example: `button_2 = run:xdotool click 1`. This maps the second button to mouse click.

### Install
Run
```
# make install
```

### Example
An example configuration file will be installed to `/etc/xdg/wactions/config`.
Copy that to `$HOME/.config/wactions/config`.
