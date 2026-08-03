import { useEffect, useState } from "react"
import { useNavigate } from "react-router-dom"
import { apiRequestRaw } from "../lib/api.js"
import Layout from "../components/Layout"
import "../styles/banner.css"

export default function AdminBanner() {
    const navigate = useNavigate()
    const [banners, setBanners] = useState([])
    const [loading, setLoading] = useState(true)
    const [error, setError] = useState("")
    const [message, setMessage] = useState("")
    const [editingId, setEditingId] = useState(null)
    const [editForm, setEditForm] = useState({ message: "", target_url: "", background_color: "", text_color: "" })
    const [createForm, setCreateForm] = useState({ message: "", target_url: "", background_color: "#000000", text_color: "#ffffff", days: 7 })
    const [creating, setCreating] = useState(false)
    const fetchedRef = { current: false }

    useEffect(() => {
        if (fetchedRef.current) return
        fetchedRef.current = true
        fetchBanners()
    }, [])

    async function fetchBanners() {
        setLoading(true)
        setError("")
        try {
            const { response, data } = await apiRequestRaw("/api/admin/banners")
            if (response.status === 401) { navigate("/admin/login"); return }
            if (response.status === 403) { navigate("/admin/no-permission"); return }
            if (data?.success) setBanners(data.banners || [])
            else setError(data?.message || "unable to load banners")
        }
        catch {
            setError("unable to load banners")
        }
        finally {
            setLoading(false)
        }
    }

    async function run(path, method, body) {
        setMessage("")
        try {
            const { response, data } = await apiRequestRaw(path, { method, body: body ? JSON.stringify(body) : undefined })
            if (response.status === 401) { navigate("/admin/login"); return false }
            if (response.status === 403) { navigate("/admin/no-permission"); return false }
            if (data?.success) { setMessage("done"); fetchBanners(); return true }
            setError(data?.message || "failed")
            return false
        }
        catch {
            setError("request failed")
            return false
        }
    }

    async function createBanner(e) {
        e.preventDefault()
        setCreating(true)
        const ok = await run("/api/admin/banners", "POST", createForm)
        if (ok) setCreateForm({ ...createForm, message: "", target_url: "" })
        setCreating(false)
    }

    function startEdit(b) {
        setEditingId(b.id)
        setEditForm({ message: b.message, target_url: b.target_url, background_color: b.background_color, text_color: b.text_color })
    }

    async function saveEdit(id) {
        const ok = await run(`/api/admin/banners/${id}`, "PATCH", editForm)
        if (ok) setEditingId(null)
    }

    async function disable(b) {
        if (confirm(`disable banner #${b.id}? the queue will advance.`)) {
            await run(`/api/admin/banners/${b.id}/disable`, "PATCH", { reason: "disabled by admin" })
        }
    }

    async function reEnable(b) {
        if (confirm(`re-enable banner #${b.id}? it will re-enter the queue.`)) {
            await run(`/api/admin/banners/${b.id}/re-enable`, "PATCH", {})
        }
    }

    async function remove(b) {
        if (confirm(`delete banner #${b.id}? it stays in history as deleted.`)) {
            await run(`/api/admin/banners/${b.id}`, "DELETE")
        }
    }

    async function move(b) {
        const position = prompt(`move banner #${b.id} to queue position (1 = next up):`, "1")
        if (position) {
            await run(`/api/admin/banners/${b.id}/move`, "PATCH", { position: Number(position) })
        }
    }

    async function advance() {
        await run("/api/admin/banners/advance", "POST")
    }

    function fmt(d) {
        if (!d) return ""
        return new Date(d).toLocaleString("en-US", { month: "short", day: "numeric", hour: "2-digit", minute: "2-digit" })
    }

    return (
        <Layout>
            <div className="adminPage">
                <div className="adminHeader">
                    <h1 className="adminTitle">banners</h1>
                    <div className="adminHeaderActions">
                        <button className="adminRefreshBtn" onClick={advance}>advance queue</button>
                    </div>
                </div>

                {error && <p className="adminError">{error}</p>}
                {message && <p className="adminEditorMessage">{message}</p>}

                <div className="adminSection adminSectionWide">
                    <h2>create an admin banner (any length, no payment)</h2>
                    <form className="bannerCreatorForm" onSubmit={createBanner}>
                        <div className="bannerCreatorField">
                            <label>message</label>
                            <textarea value={createForm.message} onChange={e => setCreateForm(f => ({ ...f, message: e.target.value }))} rows={2} required />
                        </div>
                        <div className="bannerCreatorField">
                            <label>link</label>
                            <input type="text" value={createForm.target_url} onChange={e => setCreateForm(f => ({ ...f, target_url: e.target.value }))} placeholder="https://..." />
                        </div>
                        <div className="bannerCreatorRow">
                            <div className="bannerCreatorField">
                                <label>days (1 to 365)</label>
                                <input type="number" min="1" max="365" value={createForm.days} onChange={e => setCreateForm(f => ({ ...f, days: Number(e.target.value) }))} />
                            </div>
                            <div className="bannerCreatorField">
                                <label>background</label>
                                <input type="text" value={createForm.background_color} onChange={e => setCreateForm(f => ({ ...f, background_color: e.target.value }))} />
                            </div>
                            <div className="bannerCreatorField">
                                <label>text</label>
                                <input type="text" value={createForm.text_color} onChange={e => setCreateForm(f => ({ ...f, text_color: e.target.value }))} />
                            </div>
                        </div>
                        <button type="submit" className="bannerCreatorSubmit" disabled={creating}>
                            {creating ? "placing..." : "place admin banner"}
                        </button>
                    </form>
                </div>

                {loading ? (
                    <p className="adminEmpty">loading...</p>
                ) : banners.length === 0 ? (
                    <p className="adminEmpty">no banners yet</p>
                ) : (
                    <div className="bannerAdminTable">
                        <div className="bannerAdminRow bannerAdminRowHead">
                            <span>#</span><span>status</span><span>kind</span><span>owner</span><span>email</span>
                            <span>message / url</span><span>days</span><span>remaining</span><span>amount</span><span>currency</span>
                            <span>payment</span><span>created</span><span>paid</span><span>active</span><span>expires</span>
                            <span>reports</span><span>actions</span>
                        </div>
                        {banners.map(b => (
                            <div className="bannerAdminRow" key={b.id}>
                                <span>{b.id}</span>
                                <span>{b.status}</span>
                                <span>{b.kind}</span>
                                <span>{b.owner_username}</span>
                                <span>{b.owner_email}</span>
                                <span className="bannerAdminMsg">
                                    {editingId === b.id ? (
                                        <span className="bannerAdminEdit">
                                            <input value={editForm.message} onChange={e => setEditForm(f => ({ ...f, message: e.target.value }))} />
                                            <input value={editForm.target_url} onChange={e => setEditForm(f => ({ ...f, target_url: e.target.value }))} />
                                            <input value={editForm.background_color} onChange={e => setEditForm(f => ({ ...f, background_color: e.target.value }))} />
                                            <input value={editForm.text_color} onChange={e => setEditForm(f => ({ ...f, text_color: e.target.value }))} />
                                        </span>
                                    ) : (
                                        <span>{b.message}{b.target_url ? <a className="bannerAdminUrl" href={b.target_url} target="_blank" rel="noopener noreferrer"> {b.target_url}</a> : ""}</span>
                                    )}
                                </span>
                                <span>{b.days}</span>
                                <span>{b.remaining_days != null ? b.remaining_days.toFixed(1) : ""}</span>
                                <span>{b.order_amount_cents != null ? `$${(b.order_amount_cents / 100).toFixed(2)}` : ""}</span>
                                <span>{b.order_currency || ""}</span>
                                <span>{b.order_status || ""}</span>
                                <span>{fmt(b.created_at)}</span>
                                <span>{fmt(b.order_paid_at)}</span>
                                <span>{fmt(b.starts_at)}</span>
                                <span>{fmt(b.expires_at)}</span>
                                <span>{b.report_count || 0}</span>
                                <span className="bannerAdminActions">
                                    {editingId === b.id ? (
                                        <>
                                            <button className="bannerAdminBtn" onClick={() => saveEdit(b.id)}>save</button>
                                            <button className="bannerAdminBtn" onClick={() => setEditingId(null)}>cancel</button>
                                        </>
                                    ) : (
                                        <button className="bannerAdminBtn" onClick={() => startEdit(b)}>edit</button>
                                    )}
                                    {b.status !== "disabled" && <button className="bannerAdminBtn" onClick={() => disable(b)}>disable</button>}
                                    {b.status === "disabled" && <button className="bannerAdminBtn" onClick={() => reEnable(b)}>re-enable</button>}
                                    <button className="bannerAdminBtn" onClick={() => move(b)}>move</button>
                                    <button className="bannerAdminBtn" onClick={() => remove(b)}>delete</button>
                                </span>
                            </div>
                        ))}
                    </div>
                )}
            </div>
        </Layout>
    )
}
