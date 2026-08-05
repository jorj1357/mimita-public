// 08 04 2026, 22 45
/* purpose
* Validates and builds safe post-sign-in redirect destinations.
* Rejects absolute URLs, protocol-relative paths, backslashes, colons, and control characters.
* Owns the single convention for preserving the original page across the sign-in bounce.
* DOES NOT navigate, read the location, or touch the DOM.
* DOES NOT perform authentication or grant access.
* DOES NOT store redirect values in cookies, storage, or on the server.
*/

export function safeReturnTo(value) {
    if (!value) return null
    let decoded
    try {
        decoded = decodeURIComponent(value)
    }
    catch {
        return null
    }
    if (!decoded.startsWith("/")) return null
    if (decoded.startsWith("//")) return null
    if (decoded.includes("\\") || decoded.includes(":")) return null
    if (decoded.includes("\n") || decoded.includes("\r")) return null
    return decoded
}

export function buildSigninPath(returnTo) {
    const safe = safeReturnTo(returnTo)
    if (!safe) return "/signin"
    return `/signin?returnTo=${encodeURIComponent(safe)}`
}

export function readReturnTo(search) {
    const params = new URLSearchParams(search || "")
    return safeReturnTo(params.get("returnTo"))
}

export function redirectTarget(search, fallback = "/") {
    return readReturnTo(search) || fallback
}
