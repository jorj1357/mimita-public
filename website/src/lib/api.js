import {
    logRequestStart,
    logRequestSuccess,
    logRequestError,
    logNetwork
} from "./api-log.js"

function getCsrfToken() {
    const match = document.cookie.match(/(?:^|;\s*)csrf_token=([^;]*)/)
    return match ? decodeURIComponent(match[1]) : null
}

function addCsrfHeader(method, headers) {
    if (["GET", "HEAD", "OPTIONS"].includes(method)) return
    const token = getCsrfToken()
    if (token) {
        headers["X-CSRF-Token"] = token
    }
}

export async function apiRequest(path, options = {}) {
    const method = (options.method || "GET").toUpperCase()
    const startTime = performance.now()

    const body = options.body || null
    logRequestStart(method, path, body ? safeParseBody(body) : null)

    const headers = {
        "Content-Type": "application/json",
        ...options.headers
    }
    addCsrfHeader(method, headers)

    try {
        const response = await fetch(path, {
            credentials: "include",
            ...options,
            headers
        })

        const responseClone = response.clone()
        const rawText = await responseClone.text()
        const data = safeParseResponse(rawText)
        const endTime = performance.now()
        const duration = Math.round(endTime - startTime)

        const payloadSize = body ? new Blob([body]).size : 0
        const responseSize = rawText.length

        logNetwork(method, path, startTime, endTime, payloadSize, responseSize, response.status)

        if (!response.ok) {
            const errorData = data || { message: `HTTP ${response.status}` }
            logRequestError(method, path, response.status, duration, errorData)
            throw new ApiError(errorData.message || "request failed", response.status, errorData)
        }

        logRequestSuccess(method, path, duration, data)
        return data
    }
    catch (error) {
        const endTime = performance.now()
        const duration = Math.round(endTime - startTime)

        if (!(error instanceof ApiError)) {
            logRequestError(method, path, 0, duration, { message: error.message || "network error" })
            logNetwork(method, path, startTime, endTime, 0, 0, 0)
            throw new ApiError(error.message || "unable to reach server", 0, null)
        }

        throw error
    }
}

export async function apiRequestRaw(path, options = {}) {
    const method = (options.method || "GET").toUpperCase()
    const startTime = performance.now()

    const body = options.body || null
    logRequestStart(method, path, body ? safeParseBody(body) : null)

    const headers = { ...options.headers }
    addCsrfHeader(method, headers)

    try {
        const response = await fetch(path, {
            credentials: "include",
            ...options,
            headers
        })

        const endTime = performance.now()
        const duration = Math.round(endTime - startTime)

        const clone = response.clone()
        const rawText = await clone.text().catch(() => "")
        const data = safeParseResponse(rawText)
        const responseSize = rawText.length
        const payloadSize = body ? new Blob([body]).size : 0

        logNetwork(method, path, startTime, endTime, payloadSize, responseSize, response.status)

        if (!response.ok) {
            logRequestError(method, path, response.status, duration, data || { message: `HTTP ${response.status}` })
        }
        else {
            logRequestSuccess(method, path, duration, data || "(no body)")
        }

        return { response, data }
    }
    catch (error) {
        const endTime = performance.now()
        const duration = Math.round(endTime - startTime)
        logRequestError(method, path, 0, duration, { message: error.message || "network error" })
        logNetwork(method, path, startTime, endTime, body ? new Blob([body]).size : 0, 0, 0)
        throw error
    }
}

class ApiError extends Error {
    constructor(message, status, data) {
        super(message)
        this.name = "ApiError"
        this.status = status
        this.data = data
    }
}

function safeParseBody(body) {
    if (!body) return null
    try {
        return JSON.parse(body)
    }
    catch {
        return body
    }
}

function safeParseResponse(text) {
    if (!text) return null
    try {
        return JSON.parse(text)
    }
    catch {
        return null
    }
}

export { ApiError }
