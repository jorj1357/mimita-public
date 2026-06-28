import { Router } from "express"
import { pool, getDbConfig } from "./db.js"

const router = Router()

const production = process.env.NODE_ENV === "production"

router.get("/health", async (req, res) => {
    let dbOk = false
    try {
        await pool.query("SELECT 1")
        dbOk = true
    }
    catch {}
    res.json({
        success: true,
        environment: production ? "production" : "development",
        uptime: process.uptime(),
        database: dbOk ? "connected" : "disconnected",
        timestamp: new Date().toISOString()
    })
})

router.use((req, res, next) => {
    if (production) {
        return res.status(404).json({
            success: false,
            message: "not found"
        })
    }
    next()
})

const ERROR_CATALOG = {
    auth: {
        signup: {
            expected: [
                "username too short (< 3 chars)",
                "invalid email format",
                "password too weak (8+ chars, 1 uppercase, 1 symbol)",
                "username already exists (409)",
                "email already registered (409)"
            ],
            unexpected: [
                "database connection failure",
                "email send failure (catch in safelySend)"
            ],
            debug: [
                "check request payload in network tab",
                "check [DB ERROR] log for constraint violations",
                "verify SMTP_HOST if mail fails"
            ]
        },
        signin: {
            expected: [
                "invalid username/email or password (401)",
                "user not found"
            ],
            unexpected: [
                "database down"
            ],
            debug: [
                "verify credentials in database: SELECT * FROM users WHERE username_key = ...",
                "check password_hash format (should be scrypt:salt:hex)"
            ]
        },
        session: {
            expected: [
                "session expired (401)",
                "no session cookie (401)",
                "user deleted (401)"
            ],
            unexpected: [
                "token_hash collision (should not happen)"
            ],
            debug: [
                "check browser cookies for mimita_session",
                "query sessions table: SELECT * FROM sessions WHERE user_id = ...",
                "verify expires_at > NOW()",
                "check revoked_at IS NULL"
            ]
        }
    },
    admin: {
        login: {
            expected: [
                "invalid admin credentials (401)"
            ],
            unexpected: [
                "admin_sessions table missing (run migrations)"
            ],
            debug: [
                "check ADMIN_SECRET env var",
                "verify hardcoded credentials in server/admin.js"
            ]
        },
        dashboard: {
            expected: [],
            unexpected: [
                "analytics_metrics table empty",
                "database connection failure"
            ],
            debug: [
                "run refresh to populate metrics",
                "check analytics_metrics table: SELECT * FROM analytics_metrics"
            ]
        }
    },
    feedback: {
        submit: {
            expected: [
                "empty feedback (no presets, no text)"
            ],
            unexpected: [
                "database insert failure"
            ],
            debug: [
                "verify feedback table exists",
                "check selected_presets format (should be JSON array)"
            ]
        }
    },
    newsletter: {
        signup: {
            expected: [
                "invalid email format (400)",
                "email already subscribed (200 with alreadySubscribed flag)"
            ],
            unexpected: [
                "email send failure (catch in safelySend)"
            ],
            debug: [
                "query newsletter table: SELECT * FROM newsletter WHERE email = ..."
            ]
        }
    },
    analytics: {
        tracking: {
            expected: [],
            unexpected: [
                "analytics_events table missing",
                "event_name is null"
            ],
            debug: [
                "check analytics_events table: SELECT event_name, COUNT(*) FROM analytics_events GROUP BY event_name",
                "verify trackEvent() is being called with correct event_name"
            ]
        }
    },
    database: {
        connection: {
            expected: [
                "ECONNREFUSED - PostgreSQL not running",
                "28P01 - invalid password",
                "42P01 - table not found"
            ],
            unexpected: [
                "disk full",
                "connection pool exhausted"
            ],
            debug: [
                "check .env file has correct DB_* values",
                "run: psql -U mimita_user -d mimita_db -h localhost",
                "run: npm run migrate",
                "verify PostgreSQL service is running"
            ]
        }
    }
}

router.get("/error-catalog", (req, res) => {
    res.json({ success: true, catalog: ERROR_CATALOG })
})

router.get("/health", async (req, res) => {
    const checks = {
        server: { status: "ok", uptime: process.uptime() },
        database: { status: "unknown" },
        environment: {
            node: process.version,
            platform: process.platform,
            env: process.env.NODE_ENV || "development"
        }
    }

    try {
        const dbResult = await pool.query("SELECT 1 AS ping")
        checks.database = {
            status: "ok",
            ping: "connected",
            config: getDbConfig()
        }
    }
    catch (error) {
        checks.database = {
            status: "error",
            message: error.message,
            code: error.code,
            config: getDbConfig()
        }
    }

    res.json({ success: true, checks })
})

router.get("/routes", (req, res) => {
    const routes = collectRoutes(req.app)
    res.json({ success: true, routes })
})

function collectRoutes(app) {
    const list = []
    function extract(layer, basePath) {
        if (layer.route) {
            const methods = Object.keys(layer.route.methods).join(",").toUpperCase()
            list.push({
                method: methods,
                path: basePath + layer.route.path
            })
        }
        else if (layer.name === "router" && layer.handle.stack) {
            const routerPath = layer.regexp.source
                .replace("\\/?(?=\\/|$)", "")
                .replace(/\\\//g, "/")
                .replace(/\\/g, "")
                .replace(/\^/g, "")
                .replace(/\(\?:\[\^\\\/\]\+\?\)/g, ":param")
                .replace(/\?\(\[\^\\\/\]\*\\\/\)\?/g, "")
                .replace(/\$\/i/g, "")
            layer.handle.stack.forEach(l => extract(l, routerPath))
        }
    }
    app._router.stack.forEach(l => extract(l, ""))
    return list
}

export default router
export { ERROR_CATALOG }
