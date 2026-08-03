// 08 02 2026, 23 30
/* purpose
* Pure helpers for the banner UI: countdown formatting and local-only dismissal.
* Countdown is computed client-side from an absolute expires_at timestamp so the
* page never polls the server every second.
* DOES NOT make network requests.
* DOES NOT touch banner state on the server.
*/

const SESSION_KEY_PREFIX = "banner_dismissed_"

export function formatCountdown(expiresAt, now = Date.now()) {
    const expiry = new Date(expiresAt).getTime()
    if (!Number.isFinite(expiry)) return ""
    const remainingMs = Math.max(0, expiry - now)
    const totalSeconds = Math.floor(remainingMs / 1000)
    const hours = Math.floor(totalSeconds / 3600)
    const minutes = Math.floor((totalSeconds % 3600) / 60)
    const seconds = totalSeconds % 60
    const pad = n => String(n).padStart(2, "0")
    return `${pad(hours)}:${pad(minutes)}:${pad(seconds)}`
}

export function isBannerDismissed(bannerId) {
    if (!bannerId) return false
    try {
        return window.sessionStorage.getItem(SESSION_KEY_PREFIX + bannerId) === "1"
    }
    catch {
        return false
    }
}

export function dismissBanner(bannerId) {
    if (!bannerId) return
    try {
        window.sessionStorage.setItem(SESSION_KEY_PREFIX + bannerId, "1")
    }
    catch {
        // storage unavailable: dismissal simply won't persist
    }
}
