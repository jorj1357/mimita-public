# Graphics Resolution and Fullscreen

## Purpose

Define the available window and fullscreen modes, including Counter-Strike-style
stretched game resolution.

## Audience

This specification is for the graphics settings, window-management, and
rendering owners.

## Scope

This specification covers the game image and display mode, including whether
the GUI is included in stretched output. GUI element layout is defined by
[the GUI specification](../gui/guiv2.md).

## Does not define

This specification does not define GUI element layout. It does define whether
the completed GUI image is included in the stretched frame for modes 5 and 6.

## Fullscreen modes

The `fullscreen` setting uses these values:

| Value | Mode | Behavior |
| --- | --- | --- |
| `0` | Windowed | Run in a normal window. |
| `1` | Exclusive fullscreen | Use exclusive fullscreen at the selected game resolution. |
| `2` | Borderless fullscreen | Fill the display with a borderless window, without the stretched-resolution behavior below. |
| `3` | Exclusive fullscreen stretched | Use exclusive fullscreen and stretch the game image to fill the display. |
| `4` | Borderless fullscreen stretched | Fill the display with a borderless window and stretch the game image to fill it. |
| `5` | Exclusive fullscreen stretched, full frame | Render the entire game frame, including GUI, at 1024x768 and stretch it to fill the display. |
| `6` | Borderless fullscreen stretched, full frame | Render the entire game frame, including GUI, at 1024x768 and stretch it to fill the display. |

## Stretched fullscreen

In modes `3` and `4`, render the game image at the selected game resolution and
scale it to fill the display, even when their aspect ratios differ. For
example, render the game at 4:3 on a 16:9 monitor, then stretch the image
horizontally to fill the 16:9 display. Characters and other objects therefore
appear wider, like stretched-resolution play in Counter-Strike.

Modes `3` and `4` render the game world at the selected game resolution and
stretch that world image to fill the display. GUI and HUD elements, including
the 2D crosshair, are drawn afterward at the display resolution and keep their
normal display aspect ratio.

Modes `5` and `6` always render the complete game frame at exactly 1024x768,
regardless of the selected resolution setting. The world, GUI, HUD, and 2D
crosshair are composed together at that resolution, then the whole image is
stretched to fill the display. GUI and HUD elements therefore appear wider on a
16:9 display, just like the game world. Mode `5` uses exclusive fullscreen;
mode `6` uses borderless fullscreen.

All stretched modes fill the display without adding black bars or cropping the
game image. Modes `3` and `4` differ only in exclusive versus borderless
fullscreen. Modes `5` and `6` use the same full-frame stretch behavior and
differ only in exclusive versus borderless fullscreen.
