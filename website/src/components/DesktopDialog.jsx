import { useState, useEffect } from "react"
import { apiRequest } from "../lib/api"

export default function DesktopDialog({ onClose, onDismiss }) {
    const [status, setStatus] = useState("detecting")
    const [countdown, setCountdown] = useState(15)
    const [exchangeToken, setExchangeToken] = useState(null)

    useEffect(() => {
        apiRequest("/api/auth/token-exchange", { method: "POST" })
            .then((data) => {
                if (data.success) {
                    setExchangeToken(data.exchange_token)
                    setStatus("ready")
                    tryOpen(data.exchange_token)
                } else {
                    setStatus("failed")
                }
            })
            .catch(() => setStatus("failed"))
    }, [])

    useEffect(() => {
        if (status !== "ready" && status !== "opened") return
        if (countdown <= 0) {
            setStatus("timeout")
            return
        }
        const timer = setTimeout(() => setCountdown((c) => c - 1), 1000)
        return () => clearTimeout(timer)
    }, [countdown, status])

    function tryOpen(token) {
        const url = `mimita://login?token=${encodeURIComponent(token)}`
        const iframe = document.createElement("iframe")
        iframe.style.display = "none"
        iframe.src = url
        document.body.appendChild(iframe)
        setTimeout(() => {
            document.body.removeChild(iframe)
            setStatus("opened")
        }, 500)
        setTimeout(() => {
            if (status === "opened") {
                window.location.href = url
            }
        }, 1000)
    }

    function handleOpen() {
        if (exchangeToken) {
            setStatus("opening")
            tryOpen(exchangeToken)
        }
    }

    function handleDismiss() {
        if (onDismiss) onDismiss()
        if (onClose) onClose()
    }

    if (status === "detecting") {
        return null
    }

    return (
        <div className="desktopDialogOverlay">
            <div className="desktopDialog">
                <div className="desktopDialogIcon">
                    <svg width="64" height="64" viewBox="0 0 64 64" fill="none">
                        <rect width="64" height="64" rx="16" fill="#4A90D9"/>
                        <text x="32" y="42" textAnchor="middle" fill="white" fontSize="28" fontWeight="bold">M</text>
                    </svg>
                </div>
                <h2>Open Mimita?</h2>
                <p className="desktopDialogDesc">
                    Launch the game to continue with your account on this device.
                </p>
                <div className="desktopDialogActions">
                    <button
                        className="desktopDialogOpen"
                        onClick={handleOpen}
                        disabled={status === "opening"}
                    >
                        {status === "opening" ? "Opening..." : "Open Mimita"}
                    </button>
                    <button
                        className="desktopDialogSkip"
                        onClick={handleDismiss}
                    >
                        {countdown > 0 ? `Continue on Website (${countdown}s)` : "Continue on Website"}
                    </button>
                </div>
                {status === "timeout" && (
                    <p className="desktopDialogNote">
                        Don&apos;t have the game installed?{" "}
                        <a href="/download">Download Mimita</a>
                    </p>
                )}
            </div>
        </div>
    )
}  