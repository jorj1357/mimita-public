// 09 06 2026, 14 43
/* purpose
* Format persisted integer totals without rounding decimal strings.
* Convert authoritative playtime ticks at 60 ticks per second.
* Share display rules between profiles and leaderboards.
* DOES NOT award progression, sort rankings, or write account data.
*/

export function formatPersistentStat(value, playtime = false) {
    if (typeof value === "number" && !Number.isSafeInteger(value)) return "—"
    if (typeof value !== "number" && typeof value !== "string" && typeof value !== "bigint") return "—"
    if (!/^\d+$/.test(String(value))) return "—"
    const total = BigInt(value)
    if (!playtime) return total.toLocaleString("en-US")
    const seconds = total / 60n
    if (seconds < 60n) return `${seconds}s`
    const minutes = seconds / 60n
    const hours = minutes / 60n
    const days = hours / 24n
    if (days > 0n) return `${days.toLocaleString("en-US")}d ${hours % 24n}h ${minutes % 60n}m`
    if (hours > 0n) return `${hours}h ${minutes % 60n}m`
    return `${minutes}m`
}
