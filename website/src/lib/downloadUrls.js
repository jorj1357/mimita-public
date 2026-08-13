// 08 12 2026, 21 12
/* purpose
* Single source of truth for public game download URLs.
* Owns the launcher EXE URL and the portable ZIP URL, both pointed at the
* latest GitHub release so the site can never hand out stale artifacts.
* Uses the same stable latest-release pattern as the server's /api/download/latest.
* DOES NOT track downloads, run analytics, or serve files.
* DOES NOT know release versions, dates, or hashes.
*/

const RELEASE_BASE = "https://github.com/jorj1357/mimita-public/releases/latest/download"

export const LAUNCHER_DOWNLOAD_URL = `${RELEASE_BASE}/MimitaLauncher.exe`
export const PORTABLE_ZIP_DOWNLOAD_URL = `${RELEASE_BASE}/mimita-game.zip`
