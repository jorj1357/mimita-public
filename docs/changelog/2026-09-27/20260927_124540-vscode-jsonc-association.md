# Make VS Code treat commented config files as JSONC

Time: 2026-09-27 12:45:40 EST  
Result: PASS_WITH_HUMAN_REVIEW

## Outcome

The workspace now explicitly associates the movement config with VS Code's
`jsonc` language mode. Comments remain in `.json` files and runtime paths are
unchanged, but VS Code should no longer report every `//` line as a JSON error.

## Change

Updated `.vscode/settings.json` with explicit associations for:

- `**/config/**/*.json`
- `**/config/movement/*.json`
- `**/config/movement/movement-source.json`

## Validation

- `git diff --check -- .vscode/settings.json`: passed.

## Human step

Reload the VS Code window. The bottom-right language indicator should change
from `JSON` to `JSON with Comments` for `movement-source.json`. If it still
shows `JSON`, click that indicator, choose `JSON with Comments`, and reload the
workspace; the explicit file association will then persist.
