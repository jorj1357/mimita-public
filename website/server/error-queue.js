// 07 29 2026, 19 30
/* purpose
* Maintain an in-memory ring buffer of server-side errors and notable events.
* Capture signup failures, 5xx responses, game crash reports, and unexpected errors.
* Expose the buffer for the admin dashboard error-log widget.
* DOES NOT persist across restarts.
* DOES NOT replace the feedback system or structured logging.
*/

const MAX_ENTRIES = 200
const queue = []
let nextId = 1

export function pushError(entry) {
    queue.push({
        id: nextId++,
        timestamp: new Date().toISOString(),
        category: entry.category || "general",
        level: entry.level || "error",
        message: entry.message || "",
        method: entry.method || "",
        path: entry.path || "",
        userId: entry.userId || null,
        detail: entry.detail || ""
    })
    if (queue.length > MAX_ENTRIES) queue.shift()
}

export function getErrors(limit) {
    const n = Math.min(Math.max(1, limit || 50), MAX_ENTRIES)
    return queue.slice(-n).reverse()
}

export function getErrorCount() {
    return queue.length
}
