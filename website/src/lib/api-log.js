const ENABLED = true

const SEPARATOR = "=".repeat(56)
const SUB_SEPARATOR = "-".repeat(40)

function timestamp() {
    return new Date().toISOString()
}

function truncate(str, max = 200) {
    if (!str) return ""
    const s = typeof str === "string" ? str : JSON.stringify(str)
    if (s.length <= max) return s
    return s.slice(0, max) + "..."
}

function sanitize(payload) {
    if (!payload) return "(empty)"
    try {
        const obj = typeof payload === "string" ? JSON.parse(payload) : payload
        const cleaned = { ...obj }
        if (cleaned.password) cleaned.password = "***"
        if (cleaned.newPassword) cleaned.newPassword = "***"
        if (cleaned.confirmNewPassword) cleaned.confirmNewPassword = "***"
        if (cleaned.oldPassword) cleaned.oldPassword = "***"
        if (cleaned.password_hash) cleaned.password_hash = "***"
        if (cleaned.token) cleaned.token = "***"
        if (cleaned.token_hash) cleaned.token_hash = "***"
        if (cleaned.code) cleaned.code = "***"
        return JSON.stringify(cleaned)
    }
    catch {
        return truncate(payload)
    }
}

export function logRequestStart(method, path, payload) {
    if (!ENABLED) return
    console.groupCollapsed(
        `%c[API REQUEST] %c${method} ${path}`,
        "color:#3b82f6;font-weight:bold",
        "color:#93c5fd"
    )
    console.log(`timestamp: ${timestamp()}`)
    console.log(`method:    ${method}`)
    console.log(`endpoint:  ${path}`)
    if (payload) {
        console.log(`payload:   ${sanitize(payload)}`)
        console.log(`size:      ${new Blob([JSON.stringify(payload)]).size} bytes`)
    }
    console.groupEnd()
}

export function logRequestSuccess(method, path, duration, responseData) {
    if (!ENABLED) return
    console.groupCollapsed(
        `%c[API SUCCESS] %c${method} ${path}`,
        "color:#22c55e;font-weight:bold",
        "color:#86efac"
    )
    console.log(`timestamp: ${timestamp()}`)
    console.log(`duration:  ${duration}ms`)
    console.log(`response:  ${sanitize(responseData)}`)
    console.groupEnd()
}

export function logRequestError(method, path, status, duration, errorData) {
    if (!ENABLED) return
    console.groupCollapsed(
        `%c[API ERROR] %c${method} ${path}`,
        "color:#ef4444;font-weight:bold",
        "color:#fca5a5"
    )
    console.log(`timestamp: ${timestamp()}`)
    console.log(`status:    ${status}`)
    console.log(`duration:  ${duration}ms`)
    if (errorData) {
        console.log(`message:   ${errorData.message || "(no message)"}`)
        console.log(`raw:       ${sanitize(errorData)}`)
    }

    const causes = inferErrorCauses(method, path, status)
    if (causes.length > 0) {
        console.log(SUB_SEPARATOR)
        console.log("possible causes:")
        causes.forEach(c => console.log(`  - ${c}`))
    }

    console.groupEnd()
}

function inferErrorCauses(method, path, status) {
    const causes = []

    if (status === 400) causes.push("invalid input data")
    if (status === 401) {
        causes.push("invalid credentials")
        causes.push("session expired")
        causes.push("missing auth cookie")
    }
    if (status === 403) causes.push("insufficient permissions")
    if (status === 404) causes.push("resource not found")
    if (status === 409) causes.push("duplicate resource")
    if (status === 429) causes.push("rate limited")
    if (status >= 500) {
        causes.push("server error")
        causes.push("database error")
        causes.push("unhandled exception")
    }

    if (path.includes("auth")) {
        if (status === 401) causes.push("wrong password", "user not found")
        if (status === 409) causes.push("username already taken", "email already registered")
    }
    if (path.includes("newsletter") && status === 409) causes.push("email already subscribed")
    if (path.includes("admin/login") && status === 401) causes.push("wrong admin credentials")
    if (path.includes("dashboard") && status === 401) causes.push("admin session expired")

    return causes
}

export function logDbQuery(query, duration, rowCount, error) {
    if (!ENABLED) return
    const label = error ? "[DB ERROR]" : "[DB QUERY]"
    const color = error ? "#ef4444" : "#a855f7"
    const shortQuery = truncate(query, 120)

    if (error) {
        console.groupCollapsed(
            `%c${label}`,
            `color:${color};font-weight:bold`
        )
        console.log(`query:     ${shortQuery}`)
        console.log(`duration:  ${duration}ms`)
        console.log(`message:   ${error.message}`)
        console.log(`code:      ${error.code || "unknown"}`)
        console.log(SUB_SEPARATOR)
        console.log("possible causes:")
        if (error.code === "23505") console.log("  - duplicate key violation")
        if (error.code === "23503") console.log("  - foreign key violation")
        if (error.code === "42P01") console.log("  - table does not exist (run migrations)")
        if (error.code === "28P01") console.log("  - invalid database password")
        if (error.code === "ECONNREFUSED") {
            console.log("  - database not running")
            console.log("  - wrong host/port")
            console.log("  - firewall blocking connection")
        }
        console.log("recommended fix:")
        if (error.code === "23505") console.log("  - check unique constraints, use ON CONFLICT")
        if (error.code === "42P01") console.log("  - run: npm run migrate")
        if (error.code === "ECONNREFUSED") console.log("  - start PostgreSQL, check .env")
        console.groupEnd()
    }
    else {
        console.groupCollapsed(
            `%c${label}`,
            `color:${color};font-weight:bold`
        )
        console.log(`query:     ${shortQuery}`)
        console.log(`duration:  ${duration}ms`)
        console.log(`rows:      ${rowCount || 0}`)
        console.groupEnd()
    }
}

export function logNetwork(method, path, startTime, endTime, payloadSize, responseSize, status) {
    if (!ENABLED) return
    const duration = endTime - startTime
    console.groupCollapsed(
        `%c[NETWORK] %c${method} ${path}`,
        "color:#f59e0b;font-weight:bold",
        "color:#fde68a"
    )
    console.log(`start:     ${new Date(startTime).toISOString()}`)
    console.log(`end:       ${new Date(endTime).toISOString()}`)
    console.log(`duration:  ${duration}ms`)
    console.log(`sent:      ${payloadSize} bytes`)
    console.log(`received:  ${responseSize} bytes`)
    console.log(`status:    ${status}`)
    console.groupEnd()
}

export function logAuthEvent(event, data) {
    if (!ENABLED) return
    console.groupCollapsed(
        `%c[AUTH] %c${event}`,
        "color:#ec4899;font-weight:bold",
        "color:#f9a8d4"
    )
    console.log(`timestamp: ${timestamp()}`)
    if (data) console.log(`data:      ${sanitize(data)}`)
    console.groupEnd()
}

export function logAnalytics(event, data) {
    if (!ENABLED) return
    console.groupCollapsed(
        `%c[ANALYTICS] %c${event}`,
        "color:#14b8a6;font-weight:bold",
        "color:#5eead4"
    )
    console.log(`timestamp: ${timestamp()}`)
    if (data) console.log(`data:      ${sanitize(data)}`)
    console.groupEnd()
}

export function logStartup(message, ok) {
    if (!ENABLED) return
    const color = ok ? "#22c55e" : "#ef4444"
    const prefix = ok ? "[OK]" : "[FAIL]"
    console.log(
        `%c${prefix} %c${message}`,
        `color:${color};font-weight:bold`,
        "color:white"
    )
}
