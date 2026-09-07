2026-09-07T16:25:30Z

Branch: 8292026stash
Deployment commit: 6c2b08218571b54e8a3f834e8c7722d684893a01
Purpose: Add and deploy a forward database migration for the missing VIP name-style revision column that caused authentication and profile HTTP 500 errors.

Pre-existing changes: unrelated replay/config/documentation modifications and prior regression/changelog files were present in the worktree. Only the three migration/test files listed below were staged and committed for this task; no unrelated work was included.

Exact implementation changes:
1. Added `website/server/migrations/007_vip_style_revision.sql`:
   - New content: `ALTER TABLE vip_name_styles ADD COLUMN IF NOT EXISTS style_revision INT NOT NULL DEFAULT 1;`
   - Purpose: safely upgrade databases where the historical bootstrap migration was already recorded before `style_revision` was added to its source.
2. Updated `website/server/db.js`:
   - Old migration sequence: `[1, 5, 6]`.
   - New migration sequence: `[1, 5, 6, 7]`.
   - Added migration filename selection for `7_vip_style_revision.sql`.
3. Updated `website/server/progression.test.js`:
   - Old expected migration ledger: `[1, 5]`.
   - New expected ledger: `[1, 5, 6, 7]`.

Local validation:
- `node --test server/progression.test.js server/vip-config.test.js server/vip-entitlements.test.js`: 31 passed, 0 failed.
- Commit and push succeeded on branch `8292026stash`.

Deployment evidence:
- Preserved VPS untracked files: `website/db-check-temp.mjs` and `website/src/pages/ProfilePage.jsx.pre-reward-20260901-150551`.
- VPS fast-forwarded from `bbaf43d09ad0f2fb5bcb29d6115171f463c9490e` to `6c2b08218571b54e8a3f834e8c7722d684893a01`.
- `npm run migrate` completed successfully.
- VPS migration ledger query returned versions `1, 5, 6, 7`.
- `mimita-api` was restarted and reported online.
- PostgreSQL was active and accepting connections.
- Fresh unauthenticated `GET /api/auth/me` returned HTTP 401, which is the expected controlled response instead of HTTP 500.
- Recent PM2 logs showed normal unauthenticated 401 responses and no new `style_revision` errors after migration/restart. Historical pre-migration errors remain in the rotated/log tail and are not evidence of a current failure.

Remaining human validation: refresh the local tunneled site, sign in with the intended test account, verify `/api/auth/me` succeeds in the browser, then retest VIP checkout/configuration. If sign-in still fails, capture only the new timestamped PM2 error rather than the historical log tail.
