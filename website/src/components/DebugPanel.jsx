import { useEffect, useState, useRef } from "react"

export default function DebugPanel() {
    const [health, setHealth] = useState(null)
    const [error, setError] = useState("")
    const [recentLogs, setRecentLogs] = useState([])
    const MAX_LOGS = 50
    const logRef = useRef([])

    useEffect(() => {
        fetchHealth()
        startCapture()
    }, [])

    async function fetchHealth() {
        try {
            const response = await fetch("/api/debug/health")
            const data = await response.json()
            if (data.success) {
                setHealth(data.checks)
            }
        }
        catch {
            setError("health check failed")
        }
    }

    function startCapture() {
        const originalLog = console.log
        const originalError = console.error
        const originalWarn = console.warn

        console.log = function (...args) {
            originalLog.apply(console, args)
            addLog("log", args)
        }
        console.error = function (...args) {
            originalError.apply(console, args)
            addLog("error", args)
        }
        console.warn = function (...args) {
            originalWarn.apply(console, args)
            addLog("warn", args)
        }
    }

    function addLog(type, args) {
        const text = args.map(a =>
            typeof a === "object" ? JSON.stringify(a, null, 2) : String(a)
        ).join(" ")

        logRef.current = [
            { type, text, time: new Date().toLocaleTimeString() },
            ...logRef.current
        ].slice(0, MAX_LOGS)

        setRecentLogs([...logRef.current])
    }

    function statusClass(status) {
        if (status === "ok") return "debugStatusOk"
        if (status === "error") return "debugStatusError"
        return "debugStatusUnknown"
    }

    return (
        <div className="debugPanel">
            <h3 className="debugTitle">dev debug panel</h3>

            <div className="debugSection">
                <h4>backend health</h4>
                {health ? (
                    <div className="debugHealthGrid">
                        <div className={`debugStatusBadge ${statusClass(health.server?.status)}`}>
                            server: {health.server?.status || "unknown"}
                            {health.server?.uptime && ` (${Math.round(health.server.uptime)}s)`}
                        </div>
                        <div className={`debugStatusBadge ${statusClass(health.database?.status)}`}>
                            database: {health.database?.status || "unknown"}
                        </div>
                        <div className="debugStatusBadge debugStatusInfo">
                            node: {health.environment?.node || "?"}
                        </div>
                        <div className="debugStatusBadge debugStatusInfo">
                            env: {health.environment?.env || "?"}
                        </div>
                        {health.database?.config && (
                            <div className="debugDbConfig">
                                <span>host: {health.database.config.host}</span>
                                <span>port: {health.database.config.port}</span>
                                <span>db: {health.database.config.database}</span>
                                <span>user: {health.database.config.user}</span>
                                <span>pw set: {health.database.config.hasPassword ? "yes" : "no"}</span>
                                <span>tables ({health.database.config.expectedTables.length}): {health.database.config.expectedTables.join(", ")}</span>
                            </div>
                        )}
                    </div>
                ) : (
                    <p className="debugLoading">{error || "loading health..."}</p>
                )}
            </div>

            <div className="debugSection">
                <h4>recent console activity</h4>
                <div className="debugLogContainer">
                    {recentLogs.length === 0 ? (
                        <p className="debugEmpty">waiting for activity...</p>
                    ) : (
                        recentLogs.slice(0, 20).map((entry, i) => (
                            <div key={i} className={`debugLogLine debugLog${entry.type}`}>
                                <span className="debugLogTime">{entry.time}</span>
                                <span className="debugLogText">{entry.text}</span>
                            </div>
                        ))
                    )}
                </div>
            </div>
        </div>
    )
}
