import { Link } from "react-router-dom"
import { useState, useEffect, useRef } from "react"
import Avatar from "./Avatar"
import RainbowText from "./RainbowText"
import { logAuthEvent, logRequestError } from "../lib/api-log"
import { apiRequest } from "../lib/api"

const DISCORD_URL = import.meta.env.VITE_DISCORD_URL || "https://discord.gg/sY8QHbfG9D"

const HAMBURGER_ITEMS = [
    { label: "Edit Account", path: "/account", auth: true },
    { label: "Admin", path: "/admin/dashboard" },
    { label: "Articles", path: "/articles" },
    { label: "Contact", path: "/contact" },
    { label: "Download MiMITA", path: "/download" },
    { label: "FAQ", path: "/faq" },
    { label: "Feedback", path: "/feedback" },
    { label: "Games", path: "/games" },
    { label: "Leaderboards", path: "/leaderboard" },
    { label: "Link Account", path: "/link" },
    { label: "MiMITA News!!!", path: "/news", rainbow: true },
    { label: "Support", path: "/support" },
    { label: "Terms", path: "/terms" },
    { label: "Users", path: "/users" },
]

export default function Header() {
    const [open, setOpen] = useState(false)
    const [profileOpen, setProfileOpen] = useState(false)
    const [user, setUser] = useState(null)
    const [isAdmin, setIsAdmin] = useState(false)
    const [checkingAuth, setCheckingAuth] = useState(true)
    const menuRef = useRef()
    const hamburgerRef = useRef()
    const profileRef = useRef()

    useEffect(() => {
        fetch("/api/auth/me", { credentials: "include" })
            .then((r) => r.json())
            .then((data) => {
                if (data.success) {
                    setUser(data.user)
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

    async function handleSignOut() {
        logAuthEvent("logout", { username: user?.username })
        try {
            const data = await apiRequest("/api/auth/signout", { method: "POST" })
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

    const filteredItems = HAMBURGER_ITEMS.filter(item => {
        if (item.admin && !isAdmin) return false
        if (item.auth && !user) return false
        return true
    }).map(item => {
        if (item.path === "/profile") return { ...item, label: profileLabel }
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
                                            {item.label}
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
                    <Link className="headerMainLink" to="/contribute">Donate</Link>
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
                                            <span className="profileDropdownName">{user.username}</span>
                                        </div>
                                    </div>
                                    <Link to="/profile" onClick={() => setProfileOpen(false)}>
                                        Profile
                                    </Link>
                                    <Link to="/settings" onClick={() => setProfileOpen(false)}>
                                        Settings
                                    </Link>
                                    <Link to="/account" onClick={() => setProfileOpen(false)}>
                                        Account
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
