import { useEffect, useState } from "react"
import { useNavigate, Link } from "react-router-dom"
import { apiRequestRaw } from "../lib/api.js"
import Layout from "../components/Layout"
import "../styles/banner.css"

export default function AdminBanner() {
    const navigate = useNavigate()
    const [banners, setBanners] = useState([])
    const [loading, setLoading] = useState(true)
    const [error, setError] = useState("")
    const [editingId, setEditingId] = useState(null)
    const [editForm, setEditForm] = useState({ message: "", target_url: "", background_color: "", text_color: "" })
    const [message, setMessage] = useState("")
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

    function startEdit(b) {
        setEditingId(b.id)
        setEditForm({
            message: b.message,
            target_url: b.target_url,
            background_color: b.background_color,
            text_color: b.text_color
        })
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

    async function remove(b) {
        if (confirm(`delete banner #${b.id}? (kept in history as deleted)`)) {
            await run(`/api/admin/banners/${b.id}`, "DELETE")
        }
    }

    async function advance() {
        await run("/api/admin/banners/advance", "POST")
    }

    function fmt(d) {
        if (!d) return "—"
        return new Date(d).toLocaleString("en-US", { year: "numeric", month: "short", day: "numeric", hour: "2-digit", minute: "2-digit" })
    }

    return (
        <Layout>
            <div className="adminPage">
                <div className="adminHeader">
                    <h1 className="adminTitle">banners</h1>
                    <div className="adminHeaderActions">
                        <button className="adminRefreshBtn" onClick={advance}>advance queue</button>
                        <Link to="/banner/create" className="adminButton" style={{ padding: "0.5rem 1rem", background: "white", color: "black", textDecoration: "none", fontWeight: 700 }}>
                            create banner
                        </Link>
                    </div>
                </div>

                {error && <p className="adminError">{error}</p>}
                {message && <p className="adminEditorMessage">{message}</p>}

                {loading ? (
                    <p className="adminEmpty">loading...</p>
                ) : banners.length === 0 ? (
                    <p className="adminEmpty">no banners yet</p>
                ) : (
                    <div className="bannerAdminTable">
                        <div className="bannerAdminRow bannerAdminRowHead">
                            <span>#</span><span>status</span><span>kind</span><span>owner</span><span>message / url</span>
                            <span>days</span><span>amount</span><span>order</span><span>reports</span><span>created</span><span>active</span><span>expires</span><span>actions</span>
                        </div>
                        {banners.map(b => (
                            <div className="bannerAdminRow" key={b.id}>
                                <span>{b.id}</span>
                                <span>{b.status}</span>
                                <span>{b.kind}</span>
                                <span>{b.owner_username}</span>
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
                                <span>{b.order_amount_cents != null ? `$${(b.order_amount_cents / 100).toFixed(2)}` : "—"}</span>
                                <span>{b.payment_order_id || "—"}</span>
                                <span>{b.report_count || 0}</span>
                                <span>{fmt(b.created_at)}</span>
                                <span>{fmt(b.starts_at)}</span>
                                <span>{fmt(b.expires_at)}</span>
                                <span className="bannerAdminActions">
                                    {editingId === b.id ? (
                                        <>
                                            <button className="bannerAdminBtn" onClick={() => saveEdit(b.id)}>save</button>
                                            <button className="bannerAdminBtn" onClick={() => setEditingId(null)}>cancel</button>
                                        </>
                                    ) : (
                                        <button className="bannerAdminBtn" onClick={() => startEdit(b)}>edit</button>
                                    )}
                                    <button className="bannerAdminBtn" onClick={() => disable(b)}>disable</button>
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
