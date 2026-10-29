# Gold reference: Computer Use and MCP for Blender

- Status: `PASS_WITH_HUMAN_REVIEW` — the Blender add-on is enabled and reports its local connection, while the active Codex chat still needs to load the MCP tool list.
- Audience: people asking Codex to inspect or edit an open Blender scene.
- Purpose: explain the visible computer-control skill and the separate scene-editing integration, using the MiMITA sizing 6 setup as a worked example.

## This reference does not

- Claim that the Blender scene was inspected through MCP or that any floor geometry was created.
- Replace the upstream installation instructions or grant permission to edit a `.blend` file.
- Guarantee that visual clicks always hit the intended control; inspect the screen after each action.

## Two ways Codex can work with Blender

| Part | In plain terms | Useful for |
| --- | --- | --- |
| Computer Use skill | Codex gets screenshots and can click, type, and press keys in the visible Blender window. It is like watching and guiding a person through menus. | Opening Preferences, enabling an add-on, opening the `N` sidebar, navigating panels, and visually checking what Blender shows. |
| MCP for Blender | A small server connects Codex to an add-on running inside Blender. The add-on can read scene data and perform requested actions in Blender. | Listing objects and collections, measuring bounds, making objects or materials, arranging a scene, and returning viewport views. |

The Blender add-on listens on `localhost` port `9876`. The Codex MCP server starts with `uvx mcp-for-blender` and talks to that add-on. The add-on is the Blender-side receiver; the server is the bridge Codex calls. The repository is a community project, not an official Blender Foundation product.

The Codex plugin package in `integrations/codex` is an optional presentation layer. Upstream says it can add an inline viewport, object/material/collection mentions, and asset thumbnail pickers. The plain MCP server is sufficient for scene tools; the optional plugin does not replace the Blender add-on.

## What the repository's tools do

Names below are from the checked-out `src/blender_mcp/server.py`. They describe what the repository offers; a Codex chat only gets them after that client loads the server.

- `get_addon_status`: checks Blender and add-on versions and the connection.
- `get_scene_info`: reports object names, positions, sizes, visibility, parent/child hierarchy, and optional mesh or rig details. Collection membership can be queried with Blender Python through `execute_blender_code`.
- `look`: captures the viewport or selected views so Codex can visually review the scene.
- `execute_blender_code`: runs Blender Python (`bpy`, `bmesh`, and `mathutils`) in the open scene. This is the main way to make precise edits such as creating collections, duplicating a floor group, positioning text, and assigning parents.
- `search_assets` and `import_asset`: find and bring in outside models or materials. Licences vary by source.
- `generate_3d`: requests one generated model and imports it. Generation may take time and can use a paid provider or quota; get the user's go-ahead before using it.
- `open_viewport` and related viewport tools: show or refresh the connected view in clients that support the plugin's inline app.

For this installation, `BLENDER_MCP_SAFE_MODE=1` is set. The server checks scripts before sending them to Blender and blocks direct file/network/process access and other risky Python patterns. Normal Blender work, including object edits and Blender save/import/export operators, remains possible. This reduces risk; it is not an undo button. `DISABLE_TELEMETRY=true` opts out of the project's usage telemetry, and the connection stays on localhost.

## What happened in the visible setup

1. Computer Use opened Blender Preferences using Blender's `F3` command search for “Preferences.”
2. The add-on was disabled and enabled again so Blender would reload the installed file.
3. Computer Use returned to the 3D View, pressed `N`, and opened the **MCP for Blender** sidebar tab.
4. The panel displayed **Connected on port 9876**. This confirms the Blender-side add-on is running; it does not, by itself, add tools to a chat that started before the Codex MCP configuration was loaded.
5. The Codex `blender` MCP server is configured globally. Fully quit and relaunch Codex to load the updated configuration; if the active chat still has no Blender tools, open a new chat and verify the `blender` server is enabled in Codex MCP settings.

Computer Use is a visible interface skill, not a hidden scene API. It follows the current screen, then uses a key or click and checks the result. In this session, a coordinate landed on a scene object while attempting to focus the viewport: Blender's active selection changed to `Cube.7076` and the title gained an unsaved marker. No mesh geometry was changed and the file was not saved. Treat a changed selection or unsaved marker as a cue to inspect and restore the prior state before saving.

## A safe plan for 100 labeled floors

After the MCP tools appear in a chat:

1. Ask for a read-only inventory of the existing floor pieces, `Floor 1`/`Floor 2`/`Floor 3` labels, collections, transforms, and object bounds.
2. Identify one complete floor-and-label pair as the template. Confirm which objects belong together before duplicating anything.
3. Measure the floor thickness, the vertical step to the next level, and the sideways label gap. Use the existing label placement as the reference instead of guessing.
4. Save a working copy of the `.blend` file before changing the scene.
5. Put each level's floor pieces, text label, and a clearly named parent/anchor into its own collection, for example `Floor_001` through `Floor_100`. Parent the floor pieces and matching text to that level's anchor so they move together. Collections provide organization; the anchor provides a movable attachment point.
6. Duplicate the template using the measured repeat distance, update each label to `Floor N`, and check the first, middle, and last levels in the viewport for overlap and spacing before saving.

Suggested first request after restarting Codex:

> Use the Blender scene tools to inspect the active scene only. List the likely floor geometry, existing floor text, collections, and their positions and sizes. Do not edit or save anything. Then suggest the smallest complete floor-and-label template and report the measured gap and repeat distance you would use.

This gives the user a concrete review point before any bulk duplication.

## Sources

- Upstream project and capability list: <https://github.com/ahujasid/mcp-for-blender>
- Upstream Codex integration setup: <https://github.com/ahujasid/mcp-for-blender#codex>
- Local tool owners: `devscripts/blender-ai/src/blender_mcp/server.py` and `devscripts/blender-ai/addon.py`
- Computer Use workflow: Codex `computer-use` skill and its screenshot/action guidance
