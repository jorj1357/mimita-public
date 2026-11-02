const unavailableCodes = new Set([
    "ECONNREFUSED",
    "ECONNRESET",
    "ETIMEDOUT",
    "EHOSTUNREACH",
    "ENETUNREACH",
    "ENOTFOUND",
    "57P01",
    "57P02",
    "57P03"
])

export function isDatabaseUnavailable(error, depth = 0) {
    if (!error || depth > 4) return false
    if (unavailableCodes.has(error.code)) return true
    if (isDatabaseUnavailable(error.cause, depth + 1)) return true
    return Array.isArray(error.errors) &&
        error.errors.some((nested) => isDatabaseUnavailable(nested, depth + 1))
}
