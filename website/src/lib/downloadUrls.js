// 08 13 2026, 17 59
/* purpose
* Single source of truth for the public game download URL.
* Owns the single zip download URL pointed at the v2.0.5 release artifact
* (mimita-game-v2.0.5.zip) so the site hands out the exact requested build.
* Uses a versioned release URL so the link stays stable even after newer
* releases appear on GitHub.
* DOES NOT track downloads, run analytics, or serve files.
* DOES NOT know release dates or hashes.
*/

const RELEASE_BASE = "https://github.com/jorj1357/mimita-public/releases/download/v2.0.5"

export const GAME_ZIP_DOWNLOAD_URL = `${RELEASE_BASE}/mimita-game-v2.0.5.zip`
