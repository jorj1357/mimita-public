import { Link } from "react-router-dom"
import { useState, useEffect, useRef } from "react"
import Avatar from "./Avatar"

const DISCORD_URL = import.meta.env.VITE_DISCORD_URL || "https://discord.gg/sY8QHbfG9D"

const HAMBURGER_ITEMS = [
    { label: "3cage", path: "/3cage" },
    { label: "Account", path: "/account", auth: true },
    { label: "Admin", path: "/admin/dashboard" },
    { label: "Articles", path: "/articles" },
    { label: "Change Password", path: "/change-password", auth: true },
    { label: "Contact", path: "/contact" },
    { label: "Download", path: "/download" },
    { label: "FAQ", path: "/faq" },
    { label: "Feedback", path: "/feedback" },
    { label: "Jorj", path: "/jorj" },
    { label: "Leaderboard", path: "/leaderboard" },
    { label: "Link Account", path: "/link" },
    { label: "News", path: "/news" },
    { label: "Newsletter", path: "/newsletter" },
    { label: "Patch Notes", path: "/patch-notes" },
    { label: "Password Principles", path: "/password-principles" },
    { label: "Privacy", path: "/privacy" },
    { label: "Profile", path: "/profile", auth: true },
    { label: "Roadmap", path: "/roadmap" },
    { label: "Settings", path: "/settings", auth: true },
    { label: "Socials", path: "/socials" },
    { label: "Support", path: "/support" },
    { label: "Terms", path: "/terms" },
    { label: "Users", path: "/users" },
]

export default function Header() {
    const [open, setOpen] = useState(false)
    const [user, setUser] = useState(null)
    const [isAdmin, setIsAdmin] = useState(false)
    const [checkingAuth, setCheckingAuth] = useState(true)
    const menuRef = useRef()
    const hamburgerRef = useRef()

    useEffect(() => {
        fetch("/api/auth/me", { credentials: "include" })
            .then((r) => r.json())
            .then((data) => {
                if (data.success) {
                    setUser(data.user)
                    const adminRoles = ["admin", "owner"]
                    setIsAdmin(adminRoles.includes(data.user.role))
                }
                setCheckingAuth(false)
            })
            .catch(() => setCheckingAuth(false))

        fetch("/api/admin/check", { credentials: "include" })
            .then((r) => r.json())
            .then((data) => {
                if (data.isAdmin) setIsAdmin(true)
            })
            .catch(() => {})
    }, [])

    useEffect(() => {
        function handleClickOutside(event) {
            if (menuRef.current && !menuRef.current.contains(event.target)) {
                setOpen(false)
            }
        }
        document.addEventListener("mousedown", handleClickOutside)
        return () => document.removeEventListener("mousedown", handleClickOutside)
    }, [])

    function closeMenu() {
        setOpen(false)
    }

    async function handleSignOut() {
        try {
            await fetch("/api/auth/signout", { method: "POST", credentials: "include" })
            window.location.reload()
        } catch {
            window.location.reload()
        }
    }

    const filteredItems = HAMBURGER_ITEMS.filter(item => {
        if (item.admin && !isAdmin) return false
        if (item.auth && !user) return false
        return true
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
                        <Link className="headerProfileLink" to="/profile" aria-label="View profile">
                            <Avatar user={user} size="sm" />
                        </Link>
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
