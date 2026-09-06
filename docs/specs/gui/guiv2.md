9 2 2026

- IF IT IS A GUI ELEMENT IT IS JSON HOT RELOADALBE  
  - No hardcoding gui elements EVER EVER EVER EVER EVER   
- We fixed a ui issue where each letter was drawn INDIVIDUALLY , causing horrible performance on low power devices  
  - 

# **MiMITA GUI Architecture Full Specification**

**Original goal:** 07-10-2026 / 07-19-2026  
**Updated:** 09-06-2026  
**Status:** Core GUI architecture specification

---

# **1\. Absolute GUI Rules**

MiMITA should have **one shared GUI architecture** used throughout the game.

The most important rule is:

> **IF IT IS A GUI ELEMENT, ITS PRESENTATION MUST BE JSON-DEFINED AND JSON HOT RELOADABLE.**

Do not hardcode feature-specific GUI presentation.

This applies to:

```
main menu
HUD
inventory
chat
notifications
server browser
settings
replays
avatar editor
Bomb Tag UI
damage numbers
names above players
typing indicators
debug UI
world-space UI
future GUI
```

Feature/gameplay code supplies **data and named actions**.

JSON decides how that data looks.

Example:

```
Bomb Tag code supplies:

bomb_holder_name = "Jorj1357"
bomb_ticks_remaining = 489
local_player_has_bomb = false
```

JSON decides:

```
text format
position
anchor
size
font
color
alpha
Z layer
effects
visibility
layout
animation
```

Feature code must NOT do:

```
drawText("Jorj1357 HAS THE BOMB", 500, 30);
```

The desired flow is:

```
GAME STATE
     ↓
DATA BINDINGS
     ↓
JSON GUI DEFINITION
     ↓
SHARED GUI ENGINE
     ↓
VISIBLE GUI
```

JSON may reference named actions:

```
open_play_menu
join_server
close_notification
equip_slot
send_chat
```

but JSON does not contain actual gameplay logic.

C++ owns what an action does.

JSON owns how the control invoking it is presented.

A GUI feature that visually works but cannot be edited through JSON is considered **broken**.

Adding new hardcoded feature-specific GUI presentation is a **regression**.

---

# **2\. Minimal Primitive System**

All MiMITA GUI should ultimately reduce to the smallest feasible set of shared primitives.

The core should be approximately:

```
Text
Image
Shape
Container
Input/Interaction
```

Do not create dozens of unrelated fundamental widget renderers when composition can produce the same behavior.

Higher-level concepts should be compositions.

Example:

```
Button
└── Container / Shape
    ├── Text
    └── Input behavior
```

An image button:

```
ImageButton
└── Container / Shape
    ├── Image
    ├── optional Text
    └── Input behavior
```

A notification:

```
Notification
└── Container
    ├── Shape
    ├── Image
    ├── Title Text
    ├── Body Text
    └── Button/Input
```

A server-browser entry:

```
ServerCard
└── Container
    ├── Image
    ├── Server Name
    ├── Player Count
    ├── Ping
    ├── Tags
    └── Join Button
```

Do not create:

```
BombTagTimerRenderer
NotificationRenderer
ServerCardRenderer
DamageNumberRenderer
```

when shared primitives can represent them.

## **Reusable Components**

JSON should support reusable definitions/templates.

Example concept:

```
default_button
default_scrollbar
default_notification
server_card
inventory_slot
```

Define once:

```
default_scrollbar
```

and reuse it everywhere.

A specific use may override properties:

```
default_scrollbar
    ↓
override width
override color
```

without duplicating the entire definition.

Prefer:

```
define once
reuse
override only differences
```

over copy-pasting GUI definitions.

---

# **3\. JSON, Data Binding & Named Actions**

JSON controls **100% of GUI presentation**.

Dynamic game state should enter GUI through generic data bindings.

Example conceptual JSON:

```
{
  "type": "Text",
  "text": "{bomb_holder_name} has the bomb!!!! {bomb_seconds} until it explodes!!!"
}
```

Game code supplies:

```
bomb_holder_name
bomb_seconds
```

Game code should not assemble and draw the final visual itself.

## **Bindings**

The GUI system should support reusable bindings for values such as:

```
username
health
ammo
weapon_name
server_name
player_count
ping
bomb_holder_name
bomb_ticks_remaining
notification_text
chat_message
```

Bindings represent data.

They do not define presentation.

## **Conditions**

The GUI system should support a small generic conditional system where useful.

Conceptually:

```
visible_when = local_player_has_bomb
```

or:

```
visible_when = NOT local_player_has_bomb
```

This prevents feature code from needing custom rendering branches for every UI state.

Do not turn JSON into a second programming language.

Conditions should remain small, understandable, inspectable, and deterministic.

## **Named Actions**

Interactive elements may invoke named actions.

Example:

```
{
  "action": "open_play_menu"
}
```

or:

```
{
  "action": "join_server"
}
```

JSON identifies the action.

C++ implements it.

Do not put arbitrary executable gameplay logic inside GUI JSON.

---

# **4\. Element Tree, Layout, Parenting & Z-Layers**

GUI elements should form a nested element tree.

Example:

```
MainMenu
├── Background
├── Logo
└── MenuButtons
    ├── Play
    ├── Avatar
    ├── Sign In
    ├── Settings
    ├── Replays
    └── Exit
```

Children should be able to inherit or derive appropriate transforms/layout from their parent.

Moving a parent should not require manually editing every child's absolute coordinates.

## **Parenting**

Containers should support:

```
children
parent-relative position
parent-relative scale where appropriate
clipping
visibility
opacity
Z ordering
layout
```

## **Z Layers**

Elements require explicit ordering.

Conceptually:

```
background
↓
normal UI
↓
HUD
↓
popup
↓
modal
↓
debug/inspection
```

Exact layers should be JSON/config-driven rather than scattered hardcoded ordering.

## **Layout Containers**

Long-term, support generic layout modes such as:

```
free
horizontal
vertical
grid
wrap
overlay
```

These are layout behaviors, not separate rendering systems.

Example server browser:

```
ScrollContainer
└── ServerList
    layout = vertical
    ├── ServerCard
    ├── ServerCard
    ├── ServerCard
    └── ...
```

The layout system should handle child positioning rather than requiring hundreds of manually calculated coordinates.

## **Text Overflow**

Default text overflow behavior:

```
wrap
```

Other JSON-selectable modes should include:

```
wrap
clip
ellipsis
shrink-to-fit
scroll
intentional overflow
```

MiMITA's weird/brutalist visual style must not be prevented by the layout system.

The system provides behaviors.

JSON chooses them.

---

# **5\. Anchors, Scaling & Resolution Independence**

GUI must work across substantially different resolutions and aspect ratios.

Examples include:

```
640×480
1280×720
1920×1080
2560×1440
3840×2160
ultrawide
mobile displays
other future platforms
```

Do not design the architecture around one resolution.

Use:

```
anchors
normalized coordinates
relative sizing
optional pixel/unit offsets
parent-relative layout
```

Example concept:

```
anchor = bottom_right
position = normalized(1.0, 1.0)
offset = (-20, -20)
```

instead of assuming:

```
x = 1880
y = 1040
```

## **Anchors**

Support concepts such as:

```
top_left
top_center
top_right

center_left
center
center_right

bottom_left
bottom_center
bottom_right
```

and custom normalized anchors where useful.

## **User Scaling**

UI scale and text scale must remain configurable for accessibility.

A user increasing UI scale should not require completely separate menu implementations.

Layouts do NOT need to be pixel-identical across every device.

The requirement is:

> Preserve the intended information, usability, hierarchy, and interaction across supported display configurations.

---

# **6\. Screen-Space & World-Space GUI**

The same GUI architecture should support both:

```
SCREEN SPACE
WORLD SPACE
```

Reuse primitives wherever feasible.

For example, `Text` should not require entirely separate feature-specific implementations for:

```
menu label
damage number
Bomb Tag timer
player name
typing indicator
```

Instead the element has an appropriate space/transform mode.

Conceptually:

```
Text
space = screen
```

versus:

```
Text
space = world
```

## **World-Space Examples**

World-space GUI includes:

```
damage numbers
names above players
typing indicators
Bomb Tag timer
interaction prompts
floating status indicators
world-space images
world-space shapes
future world-space buttons
```

JSON should control appropriate world-space behavior such as:

```
world position / offset
scale
rotation
face camera
maximum visible distance
fade distance
depth behavior
occlusion behavior
visible-through-walls behavior
Z/depth policy
```

A damage number should therefore be conceptually:

```
GAME:
damage = 35
hit_position = X,Y,Z

        ↓

JSON:
Text
text = "{damage}"
space = world
position = "{hit_position}"
color = ...
scale = ...
lifetime = ...
modifier = ...
```

The gameplay damage system should not decide font, color, text size, or animation.

## **2D and 3D**

The architecture should be capable of representing both conventional 2D GUI and appropriate 3D/world presentation.

Do not create an unrelated second GUI architecture solely because an element exists in the world.

---

# **7\. Tick-Based Animation, Effects & Interaction**

GUI animation must be based on the MiMITA client/UI tick timeline rather than rendered FPS.

Default target:

```
60 client/UI ticks per second
```

Therefore:

```
30 FPS device
60 FPS device
144 FPS device
300 FPS device
```

all progress through the same underlying GUI animation timing.

Rendering may visually sample that timeline at different frequencies.

Game/UI behavior should not run faster merely because FPS is higher.

## **Reusable Modifiers**

Effects should be generic modifiers rather than unique widget types.

Examples:

```
sway
rainbow
pulse
glitch
hue shift
rotation
scale oscillation
position oscillation
fade
mouse avoidance
```

Modifiers should be stackable.

Example:

```
PLAY BUTTON
├── sway
├── rainbow
└── pulse
```

Do not create:

```
RainbowButton
SwayButton
RainbowSwayButton
GlitchRainbowSwayButton
```

The primitive remains a button/composition.

Effects modify properties.

## **Post-Effects**

The architecture should eventually permit configurable GUI/world presentation effects such as:

```
hue shifting
screen distortion
GUI sway
reduced motion variants
other post effects
```

These should be configurable and capable of being reduced/disabled where appropriate.

## **Element States**

Interactive elements should support generic states such as:

```
normal
hover
pressed
selected
focused
disabled
```

Each state's presentation should be JSON-defined.

Do not hardcode:

```
hover = brighten button by 20%
```

inside feature code.

## **Input**

Long-term GUI interaction should support the abstract MiMITA input/accessibility layer:

```
mouse
keyboard
controller
touch
navigation without mouse
```

Possible generic interactions include:

```
click
right click
hover
leave
hold
double click
focus
activate
controller navigation
touch
```

These map to named actions rather than embedding gameplay logic in the GUI.

---

# **8\. JSON Hot Reload**

Hot reload is a core requirement, not an optional development convenience.

Desired workflow:

```
MiMITA running on monitor 1
JSON open on monitor 2

change:
position
color
size
text
effect
layout
etc.

Ctrl+S
↓
MiMITA updates
```

No:

```
rebuild
restart
leave server
rejoin
```

should be required for normal GUI presentation edits.

Hot reload must support:

```
property changes
adding elements
removing elements
changing hierarchy
changing templates/components
changing modifiers
changing layout
```

## **Safe Reload Pipeline**

Desired architecture:

```
JSON file changes
        ↓
file watcher detects change
        ↓
parse candidate
        ↓
validate candidate
        ↓
prepare update
        ↓
queue update
        ↓
next safe client/UI tick
        ↓
atomically replace affected valid GUI state
```

Do not modify active GUI halfway through rendering.

## **Broken JSON**

A malformed edit must NOT destroy the currently working GUI.

Example:

```
GOOD GUI VERSION A
        ↓
user saves malformed JSON VERSION B
        ↓
parser/validation fails
        ↓
VERSION B REJECTED
        ↓
VERSION A REMAINS ACTIVE
        ↓
useful error logged/displayed
        ↓
user fixes JSON
        ↓
VERSION C validates
        ↓
atomically switch A → C
```

Never:

```
bad JSON
↓
entire GUI disappears
```

or:

```
bad JSON
↓
MiMITA crashes
```

The last known-good configuration remains active until a replacement validates successfully.

---

# **9\. Performance, Accessibility & Regression Protection**

GUI must be designed for very low performance cost, including low-power devices.

A previous MiMITA regression rendered individual letters separately and caused severe GUI performance problems.

That behavior must not return.

## **Text Performance Invariant**

One logical text element remains one logical text element.

Per-character/per-range formatting must NOT require creating a separate independent GUI object for every character.

For example:

```
"MiMITA IS COOL"
```

may contain internal style ranges:

```
MiMITA = red
IS = bold
COOL = rainbow modifier
```

without becoming:

```
M object
i object
M object
I object
T object
A object
...
```

Prefer:

```
batched text
persistent buffers
cached layout
minimal allocations
only rebuild changed data
```

Do not:

```
parse every JSON file every frame
recreate unchanged GUI trees every frame
allocate every label every frame
remeasure unchanged text unnecessarily
split every character into independent render work
```

## **No-Hardcoding Regression**

Automated review/checking should inspect new code for GUI presentation bypasses.

Suspicious examples include:

```
drawText("PLAY", 300, 500);
```

or feature-specific code directly defining:

```
coordinates
colors
font sizes
visible strings
GUI alpha
button dimensions
GUI animation values
```

The canonical GUI renderer/parser/layout implementation itself obviously requires implementation constants and rendering code.

The rule applies to **feature-specific presentation bypassing the JSON system**.

Trace ownership before declaring a violation.

## **Accessibility**

All GUI development must follow:

```
C:\mimita-priv-v8\docs\architecture\accessibility\accessibility.md
```

Every new GUI/input/presentation feature should answer:

1. Can it be used without a mouse?  
2. Can important information be understood without relying only on color, sound, or motion?  
3. Can scale, visibility, intensity, or quality be configured?  
4. Can behavior be tested through commands or deterministic tests?  
5. Does it degrade safely on lower-capability hardware?

Important state must not be communicated through color alone.

Support over time:

```
UI scaling
font scaling
contrast controls
color accessibility
reduced motion
reduced flashes
reduced effects
keyboard navigation
controller navigation
touch
remapping
subtitles/text equivalents
low-performance presentation modes
```

Accessibility should be built through the same configurable architecture rather than separate hardcoded accessibility screens wherever possible.

---

# **10\. Tests, Acceptance Criteria & Definition of Done**

The GUI architecture is successful when radically different interfaces can be constructed without feature-specific drawing implementations.

For example, using the same core system:

```
Main Menu
Server Browser
Notifications
Inventory
Chat
Bomb Tag HUD
Damage Numbers
Player Names
Typing Indicators
Settings
Replay UI
```

should all reduce to:

```
JSON
↓
bindings + named actions
↓
element tree
↓
shared primitives
↓
screen/world renderer
```

## **Required Automated Tests**

At minimum test:

### **Hot Reload**

```
change position → updates without restart
change text → updates
change color → updates
change hierarchy → updates
add element → appears
remove element → disappears
change modifier → updates
```

### **Invalid JSON**

```
valid A loaded
↓
invalid B saved
↓
B rejected
↓
A remains fully functional
↓
valid C saved
↓
C replaces A
```

### **Resolution**

Test representative layouts at:

```
640×480
1280×720
1920×1080
3840×2160
representative ultrawide
representative mobile/narrow layout
```

Verify usability rather than requiring identical pixel positions.

### **Performance**

Test large amounts of:

```
text
notifications
server-browser entries
world-space labels
damage numbers
animated modifiers
```

Verify logical text is not decomposed into independent per-character GUI objects.

### **World Space**

Verify the shared system can produce:

```
damage number
player name
typing indicator
Bomb Tag timer
```

without bespoke feature renderers.

### **Data Binding**

Change:

```
health
username
bomb timer
player count
```

and verify the GUI updates without feature code controlling presentation.

### **Actions**

Verify JSON-defined controls can invoke approved named actions while JSON itself contains no gameplay implementation.

### **Accessibility**

Verify:

```
UI scale
font scale
keyboard/controller navigation where implemented
reduced effects
visibility settings
non-color gameplay indicators
```

through the shared architecture.

## **Future Live Editor**

A visual drag-and-drop GUI editor is desirable later but is NOT required now.

The architecture should avoid decisions that prevent a future inspector/editor from doing:

```
click element
↓
identify JSON source
↓
show hierarchy
↓
show bindings
↓
show modifiers
↓
drag/resize/edit
↓
write updated JSON
```

For now:

> **JSON editing \+ reliable hot reload is the GUI editor.**

## **Permanent Invariants**

These are regressions if violated:

> **No feature-specific hardcoded GUI presentation.**

> **Every GUI presentation element must have a real JSON definition.**

> **Feature code supplies data and named actions; JSON supplies presentation.**

> **Screen-space and world-space UI use the shared architecture wherever feasible.**

> **GUI animations use the client/UI tick timeline, not FPS.**

> **Reusable visual effects are modifiers, not endless new widget classes.**

> **Broken hot reload input never destroys the last known-good GUI.**

> **A logical text element must not become one independently rendered GUI object per character.**

> **New GUI must respect MiMITA's accessibility and low-performance-device requirements.**

> **If implementing Feature A requires inventing another feature-specific GUI renderer, first determine why the shared GUI primitives cannot express it.**

The final desired architecture is:

```
                     GAME / APP STATE
                           │
                           ▼
                    DATA BINDINGS
                           │
             ┌─────────────┴─────────────┐
             │                           │
             ▼                           ▼
          GUI JSON                 NAMED ACTIONS
             │                           │
             ▼                           │
      PARSE + VALIDATE                   │
             │                           │
          HOT RELOAD                     │
             │                           │
      SAFE CLIENT TICK                   │
             │                           │
             ▼                           │
       GUI ELEMENT TREE                  │
             │                           │
      parenting / layout                 │
       anchors / Z-order                 │
             │                           │
       ┌─────┴─────┐                     │
       ▼           ▼                     │
    SCREEN       WORLD                   │
     SPACE        SPACE                   │
       │           │                     │
       └─────┬─────┘                     │
             ▼                           │
       CORE PRIMITIVES                   │
             │                           │
       Text / Image                      │
      Shape / Container                  │
       Input / Interaction ──────────────┘
             │
             ▼
      SHARED RENDERING
```

**One GUI architecture. JSON-defined. Hot reloadable. Resolution-independent. Tick-based. Accessible. Efficient. Composable. No feature-specific hardcoded presentation.**

