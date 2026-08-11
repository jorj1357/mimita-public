<!-- 08 11 2026, 15 58 -->
<!-- purpose
* Privacy policy for the MiMITA game, launcher and website.
* Describes what data is collected, how it is collected, and how users can opt
* out or request deletion, as required by SignPath Foundation's code of conduct.
* Does NOT cover third-party privacy policies for services MiMITA links to.
* Does NOT grant or imply any ownership of user-generated content.
-->

# MiMITA Privacy Policy

Effective date: 2026-08-11

This policy applies to the MiMITA game (`mimita.exe`), the MiMITA launcher
(`MimitaLauncher.exe`), and the website at https://mimita.fun.

## Data the game collects

MiMITA collects **anonymous gameplay analytics only after you explicitly
consent** through the first-launch popup. Analytics can be enabled or disabled
at any time from:

- the first-launch popup (`Continue` vs `Disable`),
- the in-game settings menu (Analytics Enabled),
- the terminal commands `analytics_status` and `analytics_disable`.

When enabled, the game records aggregate, non-identifying information such as
session length, onboarding steps, maps played, weapons used, movement patterns,
feature usage, crashes, disconnects, and retention. This helps us understand
and improve the game. Analytics events are uploaded in batches via HTTPS to
`https://mimita.fun/api/game/analytics/events`.

**We do not collect:** passwords, chat contents, auth tokens, email addresses,
names, or other sensitive free-form information through gameplay analytics.
The analytics subsystem explicitly excludes reading such data (see
`src/analytics/AnalyticsDocs.md`).

## Data the launcher collects

The launcher downloads and verifies game updates from the public GitHub
repository (`jorj1357/mimita-public`) using SHA-256 checksums. It does not
upload personal information. Version and download tracking on the website
records only coarse, anonymous data (platform and that a download occurred).

## Data the website collects

The website uses infrastructure services including hosting, email, newsletters,
payments (for VIP features), and optional third-party analytics tools. Please
refer to the website's own privacy disclosures on https://mimita.fun/terms for
those details.

## Data deletion

You can request deletion of analytics data from the in-game settings menu
("Request Data Deletion") or with the terminal command `analytics_request_delete`.
This sends a deletion request to
`https://mimita.fun/api/game/analytics/deletion-request`. Account-related data
on the website can be managed from your account page.

## Network transfers

In accordance with the SignPath Foundation code of conduct, **this program will
not transfer any information to other networked systems unless specifically
requested by the user or the person installing or operating it.** All network
activity in the game is user-initiated: playing online matches, signing in,
checking for updates, and (after explicit consent) submitting analytics.

## Third-party services

Using MiMITA may involve the following third parties, each with their own
privacy policies:

- GitHub (release downloads, source repository)
- SignPath.io / SignPath Foundation (code signing)
- The MiMITA website API and ICE coordinator (matchmaking, STUN/TURN)
- Payment processors used for VIP purchases

## Contact

For privacy questions or data requests, contact the MiMITA team via the
contact page at https://mimita.fun.
