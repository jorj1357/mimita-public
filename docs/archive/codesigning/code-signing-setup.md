# Code signing setup (SignPath.io free OSS program)

This is the walkthrough for getting `MimitaLauncher.exe` (and `mimita.exe`)
code-signed so Defender/SmartScreen stop flagging them — **free for open
source** via the SignPath Foundation. The CI workflow that does the signing
already exists (`.github/workflows/sign-release.yml`); this doc covers the
one-time account + GitHub-secrets setup.

## 1. Create a SignPath.io account

1. Go to https://signpath.io and sign up (free).
2. Verify your email.

## 2. Join the SignPath Foundation free OSS program

1. In SignPath, open **Organizations** → request access to the
   **SignPath Foundation** organization (this is the org that signs open-source
   projects for free with a trusted certificate).
2. Follow the SignPath Foundation onboarding (they usually ask for your repo
   URL, license, and a link to your code-signing policy — the policy is already
   published in this repo at `docs/code-signing-policy.md`).
3. Wait for approval (typically quick for OSS).

## 3. Create the project

Inside the **SignPath Foundation** organization:

1. **Projects** → create a project for this repo:
   - Name: `MiMITA`
   - Repository URL: `https://github.com/jorj1357/mimita-public`
   - License: MIT
   - Code-signing policy URL: the GitHub URL of `docs/code-signing-policy.md`
2. Note the **Project slug** (shown in the project settings; looks like
   `mimita-public`).

## 4. Create the signing policy

1. **Signing Policies** → create one (or use the Foundation's default OSS
   signing policy if offered).
2. Note the **Signing policy slug**.

## 5. Create artifact configurations (one per binary)

1. **Artifact Configurations** → create **two**:
   - Game: name `mimita.exe`, `*` → set product metadata:
     `ProductName = MiMITA`, `ProductVersion = <game version from version.txt>`.
   - Launcher: name `MimitaLauncher.exe`, `*` → set product metadata:
     `ProductName = MiMITA`, `ProductVersion = <LAUNCHER_VERSION>`.
2. Note both **artifact configuration slugs**
   (`SIGNPATH_GAME_ARTIFACT_CONFIG_SLUG`, `SIGNPATH_LAUNCHER_ARTIFACT_CONFIG_SLUG`).

## 6. Create an API token

1. **Settings → API Tokens** → create a token with the **submit signing
   request** permission for the project above.
2. Copy the token (shown once).

## 7. Add the GitHub secrets

In the repo at https://github.com/jorj1357/mimita-public/settings/secrets/actions,
add these repository secrets (exact names matter — the workflow reads them):

```
SIGNPATH_API_TOKEN                   = <API token from step 6>
SIGNPATH_ORG_ID                      = <SignPath Foundation org ID>
SIGNPATH_PROJECT_SLUG                = <project slug from step 3>
SIGNPATH_SIGNING_POLICY_SLUG         = <signing policy slug from step 4>
SIGNPATH_GAME_ARTIFACT_CONFIG_SLUG   = <game artifact config slug from step 5>
SIGNPATH_LAUNCHER_ARTIFACT_CONFIG_SLUG = <launcher artifact config slug from step 5>
```

## 8. Trigger a signed release

1. Bump `version.txt` (and `config/version.json` + `python devscripts/generate-version.py`).
2. Commit and push the version bump.
3. Create a tag and push it:

```powershell
git tag v2.0.2
git push origin v2.0.2
```

The `sign-release.yml` workflow then:

- builds `mimita.exe` + `MimitaLauncher.exe` on a Windows runner,
- uploads them as artifacts and submits **signing requests** to SignPath.io,
- waits for **Approver approval** (the SignPath Foundation approves OSS builds),
- downloads the signed EXEs, rebuilds `mimita-game.zip` + `launcher_info.json`
  with the signed hashes,
- creates/updates the GitHub release with the signed assets.

## 9. Verify

After the workflow finishes:

```powershell
signtool verify /pa /all "C:\mimita-priv-v8\MimitaLauncher.exe"
```

A signed launcher shows a valid Authenticode signature from the SignPath
Foundation certificate, and Defender/SmartScreen stop flagging it.

---

### Alternative: buy a certificate and sign locally

If you prefer a paid cert (OV ~$200-250/yr from DigiCert/Sectigo) instead of
the free SignPath program:

1. Buy the cert, install it into your personal certificate store.
2. Sign with signtool (already on this machine):

```powershell
signtool sign /fd SHA256 /a /tr http://timestamp.digicert.com /td SHA256 ^
  MimitaLauncher.exe
signtool verify /pa /all MimitaLauncher.exe
```

You'd repeat this after every launcher rebuild and before uploading.
