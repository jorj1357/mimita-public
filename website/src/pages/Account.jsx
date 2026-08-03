import { Link, useNavigate } from "react-router-dom"
import { useEffect, useState } from "react"

import Layout from "../components/Layout"
import Username from "../components/Username"
import Avatar from "../components/Avatar"
import AvatarCropModal from "../components/AvatarCropModal"
import { apiRequest } from "../lib/api"

export default function Account() {
    const navigate = useNavigate()
    const [user, setUser] = useState(null)
    const [bio, setBio] = useState("")
    const [deletePassword, setDeletePassword] = useState("")
    const [message, setMessage] = useState("loading account...")
    const [uploading, setUploading] = useState(false)
    const [cropFile, setCropFile] = useState(null)
    const [dangerOpen, setDangerOpen] = useState(false)
    const [changePw, setChangePw] = useState({ current: "", newPass: "", confirm: "" })
    const [toast, setToast] = useState(null)

    useEffect(() => {
        apiRequest("/api/auth/me")
            .then((data) => {
                setUser(data.user)
                setBio(data.user.bio || "")
                setMessage("")
            })
            .catch(() => navigate("/signin"))
    }, [navigate])

    async function saveProfile(event) {
        event.preventDefault()
        try {
            await apiRequest("/api/account/profile", {
                method: "PATCH",
                body: JSON.stringify({ bio })
            })
            setMessage("profile saved")
        }
        catch (error) {
            setMessage(error.message)
        }
    }

    function handleAvatarSelect(event) {
        const file = event.target.files?.[0]
        if (!file) return

        if (file.size > 5 * 1024 * 1024) {
            setMessage("file too large (max 5MB)")
            return
        }

        const allowed = ["image/jpeg", "image/png", "image/webp"]
        if (!allowed.includes(file.type)) {
            setMessage("only jpg, png, and webp allowed")
            return
        }

        setCropFile(file)
        event.target.value = ""
    }

    function getCsrfToken() {
        const match = document.cookie.match(/(?:^|;\s*)csrf_token=([^;]*)/)
        return match ? decodeURIComponent(match[1]) : null
    }

    async function handleCropSave(blob) {
        setCropFile(null)
        setUploading(true)
        setMessage("")

        try {
            const formData = new FormData()
            formData.append("avatar", blob, "avatar.png")

            const headers = {}
            const csrf = getCsrfToken()
            if (csrf) headers["X-CSRF-Token"] = csrf

            const response = await fetch("/api/account/avatar", {
                method: "POST",
                credentials: "include",
                headers,
                body: formData
            })

            if (!response.ok) {
                const err = await response.json().catch(() => ({}))
                throw new Error(err.message || "upload failed")
            }

            const data = await response.json()
            setUser((prev) => ({ ...prev, avatar_url: data.avatar_url, avatar_updated_at: data.avatar_updated_at }))
            setMessage("avatar updated")
        }
        catch (error) {
            setMessage(error.message)
        }
        finally {
            setUploading(false)
        }
    }

    async function handleRemoveAvatar() {
        if (!window.confirm("Remove your avatar?")) return
        try {
            await apiRequest("/api/account/avatar", { method: "DELETE" })
            setUser((prev) => ({ ...prev, avatar_url: "", avatar_updated_at: null }))
            setMessage("avatar removed")
        }
        catch (error) {
            setMessage(error.message)
        }
    }

    async function toggleNotifications(event) {
        const enabled = event.target.checked
        try {
            await apiRequest("/api/account/notification-preferences", {
                method: "PATCH",
                body: JSON.stringify({ emailNotifications: enabled })
            })
            setUser((current) => ({
                ...current,
                email_notifications_enabled: enabled
            }))
            setMessage("notification preferences saved")
        }
        catch (error) {
            setMessage(error.message)
        }
    }

    async function signOut() {
        try {
            const data = await apiRequest("/api/auth/signout", { method: "POST" })
            navigate("/signin", { state: { message: data.message } })
        }
        catch (error) {
            setMessage(error.message)
        }
    }

    async function changePassword(event) {
        event.preventDefault()
        if (changePw.newPass !== changePw.confirm) {
            showToast("new passwords do not match", "error")
            return
        }
        try {
            await apiRequest("/api/account/change-password", {
                method: "POST",
                body: JSON.stringify({ currentPassword: changePw.current, newPassword: changePw.newPass })
            })
            showToast(`password changed — confirmation sent to ${user.email}`, "success")
            setChangePw({ current: "", newPass: "", confirm: "" })
        } catch (error) {
            showToast(error.message || "password change failed", "error")
        }
    }

    function showToast(text, type) {
        setToast({ text, type })
        setTimeout(() => setToast(null), 6000)
    }

    async function deleteAccount(event) {
        event.preventDefault()
        if (!window.confirm("Permanently delete this account?")) return
        try {
            await apiRequest("/api/account", {
                method: "DELETE",
                body: JSON.stringify({ password: deletePassword })
            })
            navigate("/signup")
        }
        catch (error) {
            setMessage(error.message)
        }
    }

    if (!user) {
        return (
            <Layout>
                <section className="authPage">
                    <p>{message}</p>
                </section>
            </Layout>
        )
    }

    return (
        <Layout>
            <section className="authPage">
                <div className="authCard accountCard">
                    <div className="accountPrivacyBanner">
                        <p>
                            <strong>This is your account editor page.</strong> Only you can view and change these settings.
                        </p>
                        <p>
                            This page is private. Your public profile info (bio, avatar) may be visible to other players.
                            Your username cannot be changed. Bio and avatar are editable below.
                        </p>
                    </div>

                    <h1>ACCOUNT</h1>

                    <div className="accountAvatarSection">
                        <Avatar user={user} size="lg" />
                        <label className="accountUploadBtn">
                            {uploading ? "uploading..." : "change avatar"}
                            <input
                                type="file"
                                accept="image/jpeg,image/png,image/webp"
                                onChange={handleAvatarSelect}
                                hidden
                                disabled={uploading}
                            />
                        </label>
                        {user.avatar_url && (
                            <button
                                type="button"
                                className="accountRemoveBtn"
                                onClick={handleRemoveAvatar}
                            >
                                remove avatar
                            </button>
                        )}
                        <p className="accountAvatarHint">jpg, png, webp &middot; max 5MB &middot; 512x512</p>
                    </div>

                    <p className="accountUsername">
                        <Username user={user} size="lg" />
                    </p>
                    <p className="accountEmail">{user.email}</p>
                    <Link className="accountVipLink" to="/vip">Manage VIP</Link>

                    <form onSubmit={saveProfile}>
                        <label htmlFor="bio">bio</label>
                        <textarea
                            id="bio"
                            value={bio}
                            maxLength={500}
                            onChange={(event) => setBio(event.target.value)}
                        />
                        <button type="submit">SAVE PROFILE</button>
                    </form>

                    <label className="authToggle">
                        <input
                            type="checkbox"
                            checked={user.email_notifications_enabled}
                            onChange={toggleNotifications}
                        />
                        email notifications
                    </label>

                    <label className="authToggle">
                        <input
                            type="checkbox"
                            checked={user.email_visible}
                            onChange={async (event) => {
                                const enabled = event.target.checked
                                try {
                                    await apiRequest("/api/account/email-visibility", {
                                        method: "PATCH",
                                        body: JSON.stringify({ emailVisible: enabled })
                                    })
                                    setUser((current) => ({
                                        ...current,
                                        email_visible: enabled
                                    }))
                                    setMessage("email visibility updated")
                                }
                                catch (error) {
                                    setMessage(error.message)
                                }
                            }}
                        />
                        show my email on my public profile
                    </label>

                    <button type="button" onClick={signOut}>
                        SIGN OUT
                    </button>

                    <div className="dangerSection">
                        <button
                            type="button"
                            className="dangerToggle"
                            onClick={() => setDangerOpen(!dangerOpen)}
                        >
                            {dangerOpen ? "▾" : "▸"} danger zone
                        </button>

                        {dangerOpen && (
                            <div className="dangerContent">
                                <h3 style={{ color: "#a020ff", marginBottom: "0.75rem", marginTop: "0.5rem" }}>CHANGE PASSWORD</h3>
                                <form className="dangerZone" onSubmit={changePassword} style={{ marginBottom: "1.5rem" }}>
                                    <label htmlFor="currentPw">current password</label>
                                    <input id="currentPw" type="password" value={changePw.current}
                                        onChange={e => setChangePw(p => ({ ...p, current: e.target.value }))} required />
                                    <label htmlFor="newPw">new password</label>
                                    <input id="newPw" type="password" value={changePw.newPass}
                                        onChange={e => setChangePw(p => ({ ...p, newPass: e.target.value }))} required />
                                    <label htmlFor="confirmPw">confirm new password</label>
                                    <input id="confirmPw" type="password" value={changePw.confirm}
                                        onChange={e => setChangePw(p => ({ ...p, confirm: e.target.value }))} required />
                                    <button type="submit">CHANGE PASSWORD</button>
                                </form>

                                <form className="dangerZone" onSubmit={deleteAccount}>
                                    <h2>DELETE ACCOUNT</h2>
                                    <label htmlFor="deletePassword">confirm password</label>
                                    <input
                                        id="deletePassword"
                                        type="password"
                                        value={deletePassword}
                                        onChange={(event) => setDeletePassword(event.target.value)}
                                        required
                                    />
                                    <button type="submit">DELETE ACCOUNT</button>
                                </form>
                            </div>
                        )}
                    </div>

                    {message && <p className="authMessage">{message}</p>}
                </div>
            </section>

            {cropFile && (
                <AvatarCropModal
                    file={cropFile}
                    onSave={handleCropSave}
                    onClose={() => setCropFile(null)}
                />
            )}

            {toast && (
                <div className={`accountToast accountToast--${toast.type}`} onClick={() => setToast(null)}>
                    {toast.text}
                    <span className="accountToastClose">&times;</span>
                </div>
            )}
        </Layout>
    )
}