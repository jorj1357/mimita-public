import { Link } from "react-router-dom"
import { useState, useEffect, useRef } from "react"

const API = import.meta.env.VITE_API_ORIGIN || ""

export default function Header() {
  const [open, setOpen] = useState(false)
  const [user, setUser] = useState(null)
  const [isAdmin, setIsAdmin] = useState(false)
  const menuRef = useRef()

  useEffect(() => {
    fetch(`${API}/api/auth/me`, { credentials: "include" })
      .then((r) => r.json())
      .then((data) => {
        if (data.success) {
          setUser(data.user)
          const adminRoles = ["admin", "owner"]
          setIsAdmin(adminRoles.includes(data.user.role))
        }
      })
      .catch(() => {})

    fetch(`${API}/api/admin/check`, { credentials: "include" })
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

  return (
    <header className="header">
      <div className="headerLeft">
        <div ref={menuRef}>
          <button className="menuButton" onClick={() => setOpen(!open)}>
            ☰
          </button>

          {open && (
            <div className="dropdownMenu">
              <Link to="/" onClick={() => setOpen(false)}>home</Link>
              <Link to="/3cage" onClick={() => setOpen(false)}>3cage</Link>
              <Link to="/jorj" onClick={() => setOpen(false)}>jorj</Link>
              <Link to="/download" onClick={() => setOpen(false)}>download</Link>
              <a href="https://github.com/jorj1357/mimita-public" target="_blank" rel="noopener noreferrer" onClick={() => setOpen(false)}>github</a>
              <a href="https://discord.gg/4MRjvpDu7e" target="_blank" rel="noopener noreferrer" onClick={() => setOpen(false)}>discord</a>
              <Link to="/about" onClick={() => setOpen(false)}>about</Link>
              <Link to="/articles" onClick={() => setOpen(false)}>articles</Link>
              <Link to="/socials" onClick={() => setOpen(false)}>socials</Link>
              <Link to="/contribute" onClick={() => setOpen(false)}>contribute</Link>
              <Link to="/newsletter" onClick={() => setOpen(false)}>newsletter</Link>
              <Link to="/terms" onClick={() => setOpen(false)}>terms</Link>
              {!user && <Link to="/signin" onClick={() => setOpen(false)}>sign in</Link>}
              {!user && <Link to="/signup" onClick={() => setOpen(false)}>sign up</Link>}
              {user && <Link to="/account" onClick={() => setOpen(false)}>account</Link>}
              {user && <Link to={`/u/${user.username}`} onClick={() => setOpen(false)}>profile</Link>}
              {isAdmin && <Link to="/admin/dashboard" onClick={() => setOpen(false)}>admin</Link>}
              {user && <Link to="/" onClick={() => { setOpen(false); fetch("/api/auth/signout", { method: "POST", credentials: "include" }).then(() => window.location.reload()) }}>sign out</Link>}
            </div>
          )}
        </div>

        <Link className="headerMainLink" to="/">mimita</Link>
        <Link className="headerMainLink" to="/3cage">3cage</Link>
        <Link className="headerMainLink" to="/jorj">jorj</Link>
        <a className="headerMainLink" href="https://github.com/jorj1357/mimita-public" target="_blank" rel="noopener noreferrer">github</a>
        <a className="headerMainLink" href="https://discord.gg/4MRjvpDu7e" target="_blank" rel="noopener noreferrer">discord</a>
        <Link className="headerMainLink" to="/contribute">donate</Link>

        {isAdmin && (
          <Link className="headerMainLink headerAdminLink" to="/admin/dashboard">
            admin
          </Link>
        )}
      </div>

      <div className="headerRight">
        {user ? (
          <div className="headerUserInfo">
            <Link className="headerUserLink" to={`/u/${user.username}`}>
              {user.avatar_url ? (
                <img src={user.avatar_url} alt="" className="headerAvatar" />
              ) : (
                <span className="headerAvatarPlaceholder">{user.username[0]?.toUpperCase()}</span>
              )}
              <span className="headerUserName">{user.username}</span>
            </Link>
          </div>
        ) : (
          <div className="headerAuth">
            <Link className="headerAuthLink" to="/signin">sign in</Link>
            <Link className="headerAuthLink headerSignupLink" to="/signup">sign up</Link>
          </div>
        )}
      </div>
    </header>
  )
}