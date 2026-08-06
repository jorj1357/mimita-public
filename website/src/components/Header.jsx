import { Link } from "react-router-dom"
import { useState, useEffect, useRef } from "react"
import Avatar from "./Avatar"
import Username from "./Username"
import RainbowText from "./RainbowText"
import NameStyleEditor from "./NameStyleEditor"
import { normalizeStyle } from "../lib/vipStyle"
import { logAuthEvent } from "../lib/api-log"
import { apiRequest } from "../lib/api"

const DISCORD_URL = import.meta.env.VITE_DISCORD_URL || "https://discord.gg/sY8QHbfG9D"

const ACTIVE_SUBSCRIPTION_STATUSES = new Set(["active", "trialing", "past_due"])

const HAMBURGER_ITEMS = [
    { label: "Edit Account", path: "/account", auth: true },
    { label: "Admin", path: "/admin/dashboard" },
    { label: "Articles", path: "/articles" },
    { label: "Banner", path: "/banner/create", rainbow: true },
    { label: "Contact", path: "/contact" },
    { label: "Download MiMITA", path: "/download" },
    { label: "FAQ", path: "/faq" },
    { label: "Feedback", path: "/feedback" },
    { label: "Games", path: "/games" },
    { label: "Leaderboards", path: "/leaderboard" },
    { label: "Link Account", path: "/link" },
    { label: "MiMITA News!!!", path: "/news", rainbow: true },
    { label: "Profile", path: "/profile", auth: true },
    { label: "Support", path: "/support" },
    { label: "VIP", path: "/vip", auth: true },
    { label: "Terms", path: "/terms" },
    { label: "Users", path: "/users" },
]

export default function Header() {
    const [open, setOpen] = useState(false)
    const [profileOpen, setProfileOpen] = useState(false)
    const [user, setUser] = useState(null)
    const [isAdmin, setIsAdmin] = useState(false)
    const [checkingAuth, setCheckingAuth] = useState(true)
    const [style, setStyle] = useState(normalizeStyle(null))
    const [styleBusy, setStyleBusy] = useState("")
    const menuRef = useRef()
    const hamburgerRef = useRef()
    const profileRef = useRef()

    useEffect(() => {
        fetch("/api/auth/me", { credentials: "include" })
            .then((r) => r.json())
            .then((data) => {
                if (data.success) {
                    setUser(data.user)
                    setStyle(normalizeStyle(data.user.vip?.name_style))
                    logAuthEvent("auth state restored", { username: data.user.username, role: data.user.role })
                    const adminRoles = ["admin", "owner"]
                    setIsAdmin(adminRoles.includes(data.user.role))
                } else {
                    logAuthEvent("auth state invalid", { reason: "API returned success=false" })
                }
                setCheckingAuth(false)
            })
            .catch(() => {
                logAuthEvent("auth state invalid", { reason: "network error" })
                setCheckingAuth(false)
            })

        fetch("/api/admin/check", { credentials: "include" })
            .then((r) => r.json())
            .then((data) => {
                if (data.isAdmin) setIsAdmin(true)
            })
            .catch(() => {})
    }, [])

    // Close dropdowns on outside click
    useEffect(() => {
        function handleClickOutside(event) {
            if (menuRef.current && !menuRef.current.contains(event.target)) {
                setOpen(false)
            }
            if (profileRef.current && !profileRef.current.contains(event.target)) {
                setProfileOpen(false)
            }
        }
        document.addEventListener("mousedown", handleClickOutside)
        return () => document.removeEventListener("mousedown", handleClickOutside)
    }, [])

    function closeMenu() {
        setOpen(false)
    }

    async function saveStyle() {
        setStyleBusy("save-style")
        try {
            const data = await apiRequest("/api/vip/style", {
                method: "PATCH",
                body: JSON.stringify({ style })
            })
            setUser(current => ({ ...current, vip: data.vip }))
            setStyle(normalizeStyle(data.vip?.name_style))
        }
        catch (error) {
            logAuthEvent("vip style save error", { error: error.message })
        }
        finally {
            setStyleBusy("")
        }
    }

    async function resetStyle() {
        setStyleBusy("reset-style")
        try {
            const data = await apiRequest("/api/vip/style/reset", { method: "POST" })
            setUser(current => ({ ...current, vip: data.vip }))
            setStyle(normalizeStyle(data.vip?.name_style))
        }
        catch (error) {
            logAuthEvent("vip style reset error", { error: error.message })
        }
        finally {
            setStyleBusy("")
        }
    }

    async function manageSubscription() {
        setStyleBusy("manage-subscription")
        try {
            const data = await apiRequest("/api/vip/manage-subscription", { method: "POST" })
            if (data.url) window.location.assign(data.url)
        }
        catch (error) {
            logAuthEvent("manage subscription error", { error: error.message })
        }
        finally {
            setStyleBusy("")
        }
    }

    async function handleSignOut() {
        logAuthEvent("logout", { username: user?.username })
        try {
            await apiRequest("/api/auth/signout", { method: "POST" })
            logAuthEvent("logout", `success user_id=${user?.id}`)
        } catch (e) {
            logAuthEvent("logout error", { error: e.message, status: e.status })
        }
        // Force full page reload to clear all state regardless of API result.
        // This ensures cookies are cleared, React state is gone, and the browser
        // re-evaluates auth from scratch on the next navigation.
        window.location.replace("/")
    }

    const profileLabel = user ? `${user.username}'s Profile` : "Profile"
    const profilePath = user ? `/users/${encodeURIComponent(user.username)}` : "/profile"

    const filteredItems = HAMBURGER_ITEMS.filter(item => {
        if (item.admin && !isAdmin) return false
        if (item.auth && !user) return false
        return true
    }).map(item => {
        if (item.path === "/profile") return { ...item, label: profileLabel, path: profilePath }
        return item
    })

    return (
        <header className="header">
            <div className="headerInner">
                <div className="headerLeft">
                    <div ref={menuRef} className="headerMenuWrap">
                        <button
                            className="menuButton"
                            onClick={() => setOpen(!open)}
                            aria-label={open ? "Close menu" : "Open menu"}
                            aria-expanded={open}
                        >
                            ☰
                        </button>

                        {open && (
                            <div className="dropdownMenu" ref={hamburgerRef}>
                                <div className="dropdownMenuScroll">
                                    {filteredItems.map((item) => (
                                        <Link
                                            key={item.path}
                                            to={item.path}
                                            onClick={closeMenu}
                                        >
                                            {item.rainbow ? (
                                                <RainbowText>{item.label}</RainbowText>
                                            ) : (
                                                item.label
                                            )}
                                        </Link>
                                    ))}
                                    {user ? (
                                        <button
                                            type="button"
                                            className="dropdownSignOut"
                                            onClick={() => { closeMenu(); handleSignOut() }}
                                        >
                                            Sign Out
                                        </button>
                                    ) : (
                                        <>
                                            <Link to="/signin" onClick={closeMenu}>Sign In</Link>
                                            <Link to="/signup" onClick={closeMenu}>Sign Up</Link>
                                        </>
                                    )}
                                </div>
                            </div>
                        )}
                    </div>

                    <Link className="headerMainLink" to="/">Home</Link>
                    <Link className="headerMainLink" to="/games">Games</Link>
                    <Link className="headerMainLink" to="/about">About</Link>
                    <a
                        className="headerMainLink"
                        href={DISCORD_URL}
                        target="_blank"
                        rel="noopener noreferrer"
                    >
                        Discord
                    </a>
                    <Link className="headerMainLink" to="/jorj1357">jorj1357</Link>
                    <Link className="headerMainLink" to="/contribute">Donate</Link>
                    <Link className="headerMainLink" to="/vip">VIP</Link>
                </div>

                <div className="headerRight">
                    {checkingAuth ? (
                        <div className="headerAvatarSkeleton" />
                    ) : user ? (
                        <div ref={profileRef} className="headerProfileWrap">
                            <button
                                className="headerProfileBtn"
                                onClick={() => setProfileOpen(!profileOpen)}
                                aria-label="Profile menu"
                                aria-expanded={profileOpen}
                            >
                                <Avatar user={user} size="sm" />
                            </button>

                            {profileOpen && (
                                <div className="profileDropdown">
                                    <div className="profileDropdownHeader">
                                        <Avatar user={user} size="sm" />
                                        <div className="profileDropdownUser">
                                            <span className="profileDropdownName"><Username user={user} size="sm" /></span>
                                        </div>
                                    </div>
                                    <Link to={profilePath} onClick={() => setProfileOpen(false)}>
                                        Profile
                                    </Link>
                                    <Link to="/account" onClick={() => setProfileOpen(false)}>
                                        Edit Account
                                    </Link>
                                    {user.vip && (
                                        <div className="profileDropdownVip">
                                            <span className="profileDropdownVipTitle">VIP Name Style</span>
                                            <NameStyleEditor
                                                compact
                                                user={user}
                                                vip={user.vip}
                                                style={style}
                                                onChange={setStyle}
                                                onSave={saveStyle}
                                                onReset={resetStyle}
                                                busy={styleBusy}
                                            />
                                            {ACTIVE_SUBSCRIPTION_STATUSES.has(user.vip?.subscription?.status) ? (
                                                <button
                                                    type="button"
                                                    className="profileDropdownManageSub"
                                                    onClick={manageSubscription}
                                                    disabled={styleBusy === "manage-subscription"}
                                                >
                                                    Manage Subscription
                                                </button>
                                            ) : null}
                                        </div>
                                    )}
                                    <Link to="/vip" onClick={() => setProfileOpen(false)}>
                                        VIP
                                    </Link>
                                    <button
                                        type="button"
                                        className="profileDropdownSignOut"
                                        onClick={() => handleSignOut()}
                                    >
                                        Sign Out
                                    </button>
                                </div>
                            )}
                        </div>
                    ) : (
                        <Link className="headerSignInLink" to="/signin">
                            Sign In
                        </Link>
                    )}
                </div>
            </div>
        </header>
    )
}
