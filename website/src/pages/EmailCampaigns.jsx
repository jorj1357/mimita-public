import { useState, useEffect, useRef, useCallback } from "react"
import { apiRequest, apiRequestRaw } from "../lib/api"
import Avatar from "../components/Avatar"

function LivePreview({ html }) {
    return (
        <div
            style={{
                flex: 1,
                padding: "16px",
                background: "white",
                color: "#111",
                minHeight: "300px",
                overflowY: "auto",
                fontFamily: "Arial, sans-serif",
                fontSize: "14px",
                lineHeight: "1.5",
                borderRadius: "4px",
                border: "1px solid rgba(255,255,255,0.1)"
            }}
            dangerouslySetInnerHTML={{ __html: html }}
        />
    )
}

export default function EmailCampaigns() {
    const [users, setUsers] = useState([])
    const [totalUsers, setTotalUsers] = useState(0)
    const [selected, setSelected] = useState(new Set())
    const [search, setSearch] = useState("")
    const [subject, setSubject] = useState("")
    const [htmlBody, setHtmlBody] = useState(`<div style="max-width:600px;margin:0 auto;padding:20px;font-family:Arial,sans-serif">
  <h1 style="color:#333">Hello!</h1>
  <p>This is an email from Mimita.</p>
  <hr style="border:none;border-top:1px solid #eee;margin:20px 0" />
  <p style="color:#999;font-size:12px">You received this because you have an account at mimita.fun</p>
</div>`)
    const [templates, setTemplates] = useState([])
    const [templateName, setTemplateName] = useState("")
    const [templateDesc, setTemplateDesc] = useState("")
    const [status, setStatus] = useState("idle")
    const [progress, setProgress] = useState("")
    const [progressCount, setProgressCount] = useState({ current: 0, total: 0 })
    const [results, setResults] = useState(null)
    const [error, setError] = useState("")
    const [confirmOpen, setConfirmOpen] = useState(false)
    const [pastCampaigns, setPastCampaigns] = useState([])
    const [showPast, setShowPast] = useState(false)
    const [resizing, setResizing] = useState(false)
    const containerRef = useRef()

    useEffect(() => {
        loadUsers()
        loadTemplates()
        loadPastCampaigns()
    }, [])

    useEffect(() => {
        if (!resizing) return
        function onMouseMove(e) {
            if (!containerRef.current) return
            const rect = containerRef.current.getBoundingClientRect()
            const pct = ((e.clientX - rect.left) / rect.width) * 100
            containerRef.current.style.setProperty("--split", `${Math.max(20, Math.min(80, pct))}%`)
        }
        function onMouseUp() { setResizing(false) }
        document.addEventListener("mousemove", onMouseMove)
        document.addEventListener("mouseup", onMouseUp)
        return () => { document.removeEventListener("mousemove", onMouseMove); document.removeEventListener("mouseup", onMouseUp) }
    }, [resizing])

    async function loadUsers(s) {
        const q = s !== undefined ? s : search
        try {
            const data = await apiRequest(`/api/admin/email-campaigns/users?search=${encodeURIComponent(q)}`)
            if (data.success) {
                setUsers(data.users)
                setTotalUsers(data.total)
            }
        } catch (e) { console.log("[EMAIL CAMPAIGNS] load users error:", e.message) }
    }

    async function loadTemplates() {
        try {
            const data = await apiRequest("/api/admin/email-campaigns/templates")
            if (data.success) setTemplates(data.templates)
        } catch (e) { console.log("[EMAIL CAMPAIGNS] load templates error:", e.message) }
    }

    async function loadPastCampaigns() {
        try {
            const data = await apiRequest("/api/admin/email-campaigns/list")
            if (data.success) setPastCampaigns(data.campaigns)
        } catch (e) { console.log("[EMAIL CAMPAIGNS] load past campaigns error:", e.message) }
    }

    function handleSearch(val) {
        setSearch(val)
        loadUsers(val)
    }

    function toggleUser(id) {
        const next = new Set(selected)
        if (next.has(id)) next.delete(id)
        else next.add(id)
        setSelected(next)
    }

    function selectAll() {
        setSelected(new Set(users.map(u => u.id)))
    }

    function selectNone() {
        setSelected(new Set())
    }

    function invertSelection() {
        const ids = new Set(users.map(u => u.id))
        for (const id of selected) ids.delete(id)
        setSelected(ids)
    }

    function applyTemplate(t) {
        setSubject("")
        setHtmlBody(t.html_body)
    }

    async function handleSend() {
        setConfirmOpen(false)
        setStatus("sending")
        setProgress("Preparing...")
        setProgressCount({ current: 0, total: selected.size })
        setResults(null)
        setError("")

        try {
            setProgress("Connecting...")
            const body = {
                subject,
                htmlBody,
                recipientIds: Array.from(selected)
            }

            const res = await apiRequestRaw("/api/admin/email-campaigns/send", {
                method: "POST",
                body: JSON.stringify(body)
            })

            if (res.response.ok) {
                setResults(res.data)
                setStatus("done")
                setProgress("Completed.")
                setProgressCount({ current: selected.size, total: selected.size })
                loadPastCampaigns()
            } else {
                setStatus("error")
                setError(res.data?.message || `HTTP ${res.response.status}`)
                setProgress("Failed.")
            }
        } catch (e) {
            setStatus("error")
            setError(e.message)
            setProgress("Error.")
        }
    }

    async function handleSaveTemplate() {
        if (!templateName || !htmlBody) return
        try {
            await apiRequest("/api/admin/email-campaigns/templates", {
                method: "POST",
                body: JSON.stringify({ name: templateName, description: templateDesc, htmlBody })
            })
            setTemplateName("")
            setTemplateDesc("")
            loadTemplates()
        } catch (e) { console.log("[EMAIL CAMPAIGNS] save template error:", e.message) }
    }

    const statRow = (label, val, color) => val > 0 ? (
        <div style={{ margin: "4px 0", color: color || "#ccc" }}>{label}: <strong>{val}</strong></div>
    ) : null

    return (
        <div className="adminSection" style={{ padding: "24px" }}>
            <h2>Email Campaigns</h2>

            {/* ── User selection ── */}
            <div className="adminCard" style={{ marginBottom: "16px" }}>
                <h3>Recipients</h3>
                <input
                    type="text"
                    placeholder="Search by username, email, or role..."
                    value={search}
                    onChange={e => handleSearch(e.target.value)}
                    style={{
                        width: "100%", padding: "8px", marginBottom: "8px",
                        background: "rgba(255,255,255,0.06)", border: "1px solid rgba(255,255,255,0.1)",
                        color: "white", borderRadius: "4px"
                    }}
                />
                <div style={{ marginBottom: "8px", display: "flex", gap: "8px", flexWrap: "wrap" }}>
                    <button className="adminButton" onClick={selectAll}>Select All</button>
                    <button className="adminButton" onClick={selectNone}>Select None</button>
                    <button className="adminButton" onClick={invertSelection}>Invert</button>
                    <span style={{ marginLeft: "12px", color: "#888" }}>
                        Selected: <strong style={{ color: "#4ade80" }}>{selected.size}</strong> &nbsp;|&nbsp; Total: <strong>{totalUsers}</strong>
                    </span>
                </div>
                <div style={{ maxHeight: "300px", overflowY: "auto", border: "1px solid rgba(255,255,255,0.06)", borderRadius: "4px" }}>
                    <table style={{ width: "100%", borderCollapse: "collapse", fontSize: "13px" }}>
                        <thead>
                            <tr style={{ background: "rgba(255,255,255,0.04)", position: "sticky", top: 0 }}>
                                <th style={{ padding: "6px", width: "30px" }}></th>
                                <th style={{ padding: "6px", textAlign: "left" }}>User</th>
                                <th style={{ padding: "6px", textAlign: "left" }}>Email</th>
                                <th style={{ padding: "6px", textAlign: "left" }}>Role</th>
                                <th style={{ padding: "6px", textAlign: "left" }}>Created</th>
                                <th style={{ padding: "6px", textAlign: "center" }}>Verified</th>
                            </tr>
                        </thead>
                        <tbody>
                            {users.map(u => (
                                <tr key={u.id}
                                    style={{ borderTop: "1px solid rgba(255,255,255,0.04)", cursor: "pointer" }}
                                    onClick={() => toggleUser(u.id)}
                                >
                                    <td style={{ padding: "6px", textAlign: "center" }}>
                                        <input type="checkbox" checked={selected.has(u.id)} readOnly />
                                    </td>
                                    <td style={{ padding: "6px", display: "flex", alignItems: "center", gap: "6px" }}>
                                        <Avatar user={u} size="sm" />
                                        <span>{u.username}</span>
                                    </td>
                                    <td style={{ padding: "6px", color: "#aaa" }}>{u.email}</td>
                                    <td style={{ padding: "6px" }}>{u.role}</td>
                                    <td style={{ padding: "6px", color: "#888", fontSize: "12px" }}>
                                        {new Date(u.created_at).toLocaleDateString()}
                                    </td>
                                    <td style={{ padding: "6px", textAlign: "center" }}>
                                        {u.email_verified ? "✓" : "✗"}
                                    </td>
                                </tr>
                            ))}
                        </tbody>
                    </table>
                </div>
            </div>

            {/* ── Templates ── */}
            <div className="adminCard" style={{ marginBottom: "16px" }}>
                <h3>Templates</h3>
                <div style={{ display: "flex", gap: "8px", flexWrap: "wrap", marginBottom: "8px" }}>
                    {templates.map(t => (
                        <button key={t.id} className="adminButton" onClick={() => applyTemplate(t)}>
                            {t.name}
                        </button>
                    ))}
                    {templates.length === 0 && <span style={{ color: "#666" }}>No saved templates</span>}
                </div>
                <div style={{ display: "flex", gap: "8px", flexWrap: "wrap", marginTop: "8px", borderTop: "1px solid rgba(255,255,255,0.06)", paddingTop: "8px" }}>
                    <input type="text" placeholder="Template name" value={templateName}
                        onChange={e => setTemplateName(e.target.value)}
                        style={{ padding: "6px", background: "rgba(255,255,255,0.06)", border: "1px solid rgba(255,255,255,0.1)", color: "white", borderRadius: "4px" }}
                    />
                    <input type="text" placeholder="Description" value={templateDesc}
                        onChange={e => setTemplateDesc(e.target.value)}
                        style={{ padding: "6px", background: "rgba(255,255,255,0.06)", border: "1px solid rgba(255,255,255,0.1)", color: "white", borderRadius: "4px", flex: 1 }}
                    />
                    <button className="adminButton" onClick={handleSaveTemplate}>Save Template</button>
                </div>
            </div>

            {/* ── Email Composer ── */}
            <div className="adminCard" style={{ marginBottom: "16px" }}>
                <h3>Email Composer</h3>
                <input type="text" placeholder="Subject" value={subject}
                    onChange={e => setSubject(e.target.value)}
                    style={{ width: "100%", padding: "8px", marginBottom: "8px", background: "rgba(255,255,255,0.06)", border: "1px solid rgba(255,255,255,0.1)", color: "white", borderRadius: "4px" }}
                />
                <div ref={containerRef} style={{ display: "flex", gap: "0", height: "400px", border: "1px solid rgba(255,255,255,0.06)", borderRadius: "4px", position: "relative" }}>
                    <textarea value={htmlBody}
                        onChange={e => setHtmlBody(e.target.value)}
                        style={{
                            width: "50%", height: "100%", resize: "none", padding: "12px",
                            background: "#1a1a2e", color: "#e0e0e0", border: "none",
                            fontFamily: "monospace", fontSize: "13px", outline: "none"
                        }}
                    />
                    <div onMouseDown={() => setResizing(true)}
                        style={{ width: "4px", cursor: "col-resize", background: "rgba(255,255,255,0.1)", flexShrink: 0 }} />
                    <div style={{ flex: 1, overflow: "hidden" }}>
                        <LivePreview html={htmlBody} />
                    </div>
                </div>
            </div>

            {/* ── Send button ── */}
            <div className="adminCard" style={{ marginBottom: "16px" }}>
                <button className="adminButton"
                    onClick={() => setConfirmOpen(true)}
                    disabled={status === "sending" || selected.size === 0 || !subject || !htmlBody}
                    style={{ fontSize: "16px", padding: "10px 24px" }}
                >
                    {status === "sending" ? "Sending..." : `Send to ${selected.size} recipient${selected.size !== 1 ? "s" : ""}`}
                </button>
            </div>

            {/* ── Confirmation dialog ── */}
            {confirmOpen && (
                <div style={{ position: "fixed", inset: 0, background: "rgba(0,0,0,0.7)", display: "flex", alignItems: "center", justifyContent: "center", zIndex: 9999 }}>
                    <div className="adminCard" style={{ maxWidth: "500px", width: "90%" }}>
                        <h3>Send this email?</h3>
                        <p><strong>Recipients:</strong> {selected.size}</p>
                        <p><strong>Subject:</strong> {subject}</p>
                        <div style={{ display: "flex", gap: "12px", marginTop: "16px" }}>
                            <button className="adminButton" onClick={() => setConfirmOpen(false)}>Cancel</button>
                            <button className="adminButton" onClick={handleSend}
                                style={{ background: "#dc2626", color: "white" }}>Send</button>
                        </div>
                    </div>
                </div>
            )}

            {/* ── Progress & Results ── */}
            {status !== "idle" && (
                <div className="adminCard" style={{ marginBottom: "16px" }}>
                    <h3>Status</h3>
                    <div style={{ margin: "8px 0", color: status === "error" ? "#f87171" : "#4ade80" }}>
                        {progress}
                    </div>
                    {status === "sending" && (
                        <div style={{ margin: "8px 0", color: "#888" }}>
                            {progressCount.current} / {progressCount.total}
                        </div>
                    )}
                    {results && (
                        <div style={{ marginTop: "8px" }}>
                            <h4>Results</h4>
                            {statRow("Delivered", results.delivered, "#4ade80")}
                            {statRow("Failed", results.failed, "#f87171")}
                            {statRow("Skipped", results.skipped, "#fbbf24")}
                            {statRow("Rejected", results.rejected, "#f97316")}
                            {statRow("Invalid email", results.invalidEmail, "#a78bfa")}
                            {statRow("SMTP error", results.smtpError, "#fb923c")}
                            {statRow("DB error", results.dbError, "#ef4444")}
                        </div>
                    )}
                    {error && <div style={{ color: "#f87171", marginTop: "8px" }}>{error}</div>}
                </div>
            )}

            {/* ── Past Campaigns ── */}
            <div className="adminCard">
                <h3 onClick={() => setShowPast(!showPast)} style={{ cursor: "pointer" }}>
                    {showPast ? "▼" : "▶"} Past Campaigns ({pastCampaigns.length})
                </h3>
                {showPast && (
                    <table style={{ width: "100%", borderCollapse: "collapse", fontSize: "13px", marginTop: "8px" }}>
                        <thead>
                            <tr style={{ background: "rgba(255,255,255,0.04)" }}>
                                <th style={{ padding: "6px", textAlign: "left" }}>Subject</th>
                                <th style={{ padding: "6px", textAlign: "left" }}>Status</th>
                                <th style={{ padding: "6px", textAlign: "right" }}>Total</th>
                                <th style={{ padding: "6px", textAlign: "right" }}>Delivered</th>
                                <th style={{ padding: "6px", textAlign: "right" }}>Failed</th>
                                <th style={{ padding: "6px", textAlign: "left" }}>By</th>
                                <th style={{ padding: "6px", textAlign: "left" }}>Sent</th>
                            </tr>
                        </thead>
                        <tbody>
                            {pastCampaigns.map(c => (
                                <tr key={c.id} style={{ borderTop: "1px solid rgba(255,255,255,0.04)" }}>
                                    <td style={{ padding: "6px" }}>{c.subject}</td>
                                    <td style={{ padding: "6px", color: c.status === "sent" ? "#4ade80" : c.status === "partial" ? "#fbbf24" : "#f87171" }}>{c.status}</td>
                                    <td style={{ padding: "6px", textAlign: "right" }}>{c.total_recipients}</td>
                                    <td style={{ padding: "6px", textAlign: "right", color: "#4ade80" }}>{c.delivered_count}</td>
                                    <td style={{ padding: "6px", textAlign: "right", color: "#f87171" }}>{c.failed_count + c.rejected_count + c.smtp_error_count + c.invalid_email_count}</td>
                                    <td style={{ padding: "6px" }}>{c.created_by}</td>
                                    <td style={{ padding: "6px", color: "#888", fontSize: "12px" }}>
                                        {c.sent_at ? new Date(c.sent_at).toLocaleString() : "-"}
                                    </td>
                                </tr>
                            ))}
                        </tbody>
                    </table>
                )}
            </div>
        </div>
    )
}
