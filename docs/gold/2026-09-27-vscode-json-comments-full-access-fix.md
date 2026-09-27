// 2026-09-27T12:50:00-04:00
/* purpose
* preserve the confirmed full-access workflow that fixed persistent JSON comment errors
* record the screenshot evidence and the exact editor setting that solved it
* make the result reproducible for future commented MiMITA config files
* this file does NOT redefine the JSON runtime parser contract
* this file does NOT claim every external JSON validator accepts comments
* this file does NOT store credentials or private environment data
*/

# Gold behavior: Full-access VS Code JSON comments fix

- Date: `2026-09-27`
- Repository: `C:\mimita-v9`
- File: `config/movement/movement-source.json`
- Problem duration: comments had appeared as errors for months and were being
  ignored instead of fixed.
- Tool interaction: GPT-5.6 Luna, medium reasoning
- Environment: Codex desktop with Full access
- Screenshot evidence: `C:\Users\guita\AppData\Local\Temp\codex-clipboard-55c00606-b501-450e-845a-a20d1d661efc.png`

## Before

VS Code opened `movement-source.json` in the strict `JSON` language mode.
Every `//` and `/* ... */` comment was underlined as an error, and the file
showed a large error count. The comments were valid for MiMITA's runtime config
reader, but the editor was applying the standard JSON grammar.

## Fix

Full access allowed the workspace editor settings to be created and updated at:

`.vscode/settings.json`

The workspace now associates the config tree, movement configs, and the exact
source preset with the JSONC language mode:

```json
{
  "files.associations": {
    "**/config/**/*.json": "jsonc",
    "**/config/movement/*.json": "jsonc",
    "**/config/movement/movement-source.json": "jsonc"
  }
}
```

JSONC means JSON with Comments. It allows `//` line comments and `/* ... */`
block comments while retaining normal JSON structure and string handling.

## After

After reloading the VS Code window, the bottom-right language indicator changed
from `JSON` to `JSON with Comments`. The red comment errors disappeared. This
was the desired visible result.

## Why this is gold behavior

The successful fix identified the true owner of the symptom instead of deleting
useful documentation from the movement preset. MiMITA's C++ movement loader
already used comment-enabled parsing; the missing piece was the editor's
workspace language association. Full access made it possible to enter the
workspace settings and correct the editor behavior directly.

## Reproduction checklist

1. Open a commented config file under `C:\mimita-v9\config`.
2. Confirm the bottom-right language mode says `JSON with Comments`.
3. If VS Code still shows `JSON`, run `Developer: Reload Window`.
4. If necessary, click the language indicator and select `JSON with Comments`.
5. Confirm comments are no longer red-underlined and the config remains at its
   original `.json` path.

## Boundary

Standard JSON tools such as Python's plain `json.load()` still reject comments
because comments are not part of the official JSON grammar. MiMITA runtime
loaders and repository JSONC-aware tools must be used for these authored config
files.
