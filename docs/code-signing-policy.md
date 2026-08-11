<!-- 08 11 2026, 15 58 -->
<!-- purpose
* Code signing policy for the MiMITA open-source project.
* Meets SignPath Foundation's requirement that a code signing policy be
* specified on the project home page with team roles, the SignPath attribution
* line, and a privacy policy reference.
* Does NOT describe the technical SignPath.io configuration or CI details.
* Does NOT grant signing authority to non-team members.
-->

# Code signing policy

Free code signing provided by [SignPath.io](https://about.signpath.io),
certificate by [SignPath Foundation](https://signpath.org).

## Scope

This policy applies to the binaries published by the MiMITA project:

- `mimita.exe` — the MiMITA game.
- `MimitaLauncher.exe` — the launcher that installs and updates the game.
- `mimita-game.zip` — the game runtime archive downloaded by the launcher
  (its contents are covered by the project's open-source license; see
  `docs/third-party-licenses.md`).

## Team roles

| Role | Responsibility | Members |
|------|----------------|---------|
| Authors | May modify source code in version control without additional review | [Maintainers](https://github.com/jorj1357) |
| Reviewers | Must review every change proposed by non-committers (pull requests) | [Members](https://github.com/jorj1357) |
| Approvers | Must approve each code-signing request before a release can be signed | [Owners](https://github.com/jorj1357) |

Until the project grows a formal team, one maintainer (`jorj1357`) holds all
three roles. When additional members join, this table and the corresponding
GitHub permission groups will be updated before any further signed releases.

## Privacy policy

Our [privacy policy](privacy-policy.md) describes what data the game, launcher
and website collect, how you can disable data collection, and how to request
deletion. In accordance with the SignPath Foundation code of conduct, MiMITA
will not transfer information to other networked systems unless specifically
requested by the user or the person installing or operating it.

## How binaries are built and signed

- Every signed binary is a **valid, automated build resulting from the source
  code** in the public repository `jorj1357/mimita-public`. Source code
  includes build scripts and CI configuration, all of which are reviewed.
- Builds run on **GitHub-hosted runners** in GitHub Actions (a tag push
  workflow, `.github/workflows/sign-release.yml`).
- The unsigned build is uploaded as a GitHub Actions artifact and submitted to
  SignPath.io, which verifies origin and provenance.
- **Every signing request requires manual approval** by an Approver before the
  binary is signed.
- Product metadata is enforced via SignPath artifact configuration: all product
  name attributes are set to `MiMITA`, and product version attributes are
  consistent within each build (`mimita.exe` uses the game version from
  `version.txt`; `MimitaLauncher.exe` uses `LAUNCHER_VERSION`).
- The signed artifacts are `mimita.exe` and `MimitaLauncher.exe`. The
  `mimita-game.zip` archive downloaded by the launcher is an unsigned content
  package; its game audio is produced by the project (credits in
  `assets/sound/music/credits.json`) and its third-party components are
  covered by `docs/third-party-licenses.md`. Audio production files are kept
  out of version control per project policy but are distributed under the
  project's MIT license.

## What we do not sign

- Binaries not built from this repository.
- Binaries built on machines outside the audited CI flow.
- Software containing malware, potentially unwanted programs, or features
  designed to bypass antivirus or security software. MiMITA contains no
  antivirus-bypass features and no Defender exclusions.

## Violations

Violations of the SignPath Foundation [code of conduct](https://signpath.org/terms)
can be reported to support@signpath.io. The project team cooperates with any
investigation and may revoke or pause signing for the project.
