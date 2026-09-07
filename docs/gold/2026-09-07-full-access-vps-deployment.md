// 2026-09-07T17:30:26Z
/* purpose
* preserve gold behavior for Full access Codex VPS diagnosis and recovery
* record the exact deployment proof and human-visible result
* make the successful workflow reproducible without storing credentials
* this file does NOT contain SSH keys, passwords, tokens, or secrets
* this file does NOT authorize future deployment without the VPS procedure
* this file does NOT replace source, migration, or human acceptance evidence
*/

# Gold behavior: Full-access Codex VPS recovery

- Timestamp: `2026-09-07T17:30:26Z`
- App: OpenAI Codex desktop app on Windows
- Model: GPT-5.6
- Environment: Full access mode
- Result: after Full access mode was enabled, GPT could run `ssh mimita-vps`, inspect VPS PM2/PostgreSQL logs, identify the missing database column, deploy the forward migration, restart the API, and verify login recovery in one pass.
- Exact deployment commit: `6c2b08218571b54e8a3f834e8c7722d684893a01`
- Exact proof changelog: `C:\mimita-priv-v8\docs\changelog\2026-09-07\20260907_162530_vip-style-revision-migration.md`
- Proof: migration versions reached `1, 5, 6, 7`; PostgreSQL was healthy; `mimita-api` was online; unauthenticated `/api/auth/me` returned `401` instead of `500`; the user could sign in after retesting.

## Why this is gold behavior

Before Full access mode, the interactive human CMD session had SSH access but the Codex shell did not. After the environment changed, GPT performed the log checks itself, made the local forward migration, deployed it through Git, and listed the exact changelog path so the proof could be opened directly.
