const stores = new Map()

export function createRateLimit(options = {}) {
    const {
        windowMs = 60 * 1000,
        max = 20,
        name = "default"
    } = options

    if (!stores.has(name)) {
        stores.set(name, new Map())
    }

    const store = stores.get(name)

    setInterval(() => {
        const now = Date.now()
        for (const [key, entry] of store) {
            if (now - entry.start > windowMs) {
                store.delete(key)
            }
        }
    }, windowMs * 2).unref()

    return function rateLimitMiddleware(req, res, next) {
        const key = req.ip || req.connection?.remoteAddress || "unknown"
        const now = Date.now()
        let entry = store.get(key)

        if (!entry || now - entry.start > windowMs) {
            entry = { count: 0, start: now }
            store.set(key, entry)
        }

        entry.count++

        const remaining = Math.max(0, max - entry.count)
        const reset = entry.start + windowMs

        res.set("X-RateLimit-Limit", String(max))
        res.set("X-RateLimit-Remaining", String(remaining))
        res.set("X-RateLimit-Reset", String(Math.ceil(reset / 1000)))

        if (entry.count > max) {
            const retryAfter = Math.ceil((reset - now) / 1000)
            res.set("Retry-After", String(retryAfter))
            return res.status(429).json({
                success: false,
                message: `too many requests, try again in ${retryAfter}s`
            })
        }

        next()
    }
}

export function clearRateLimitStores() {
    stores.clear()
}
