import { Link } from "react-router-dom"

import {
  useState,
  useEffect,
  useRef
} from "react"

export default function Header() {

  const [open, setOpen] = useState(false)

  const menuRef = useRef()

  useEffect(() => {

    function handleClickOutside(event) {

      if (
        menuRef.current &&
        !menuRef.current.contains(event.target)
      ) {
        setOpen(false)
      }

    }

    document.addEventListener(
      "mousedown",
      handleClickOutside
    )

    return () => {
      document.removeEventListener(
        "mousedown",
        handleClickOutside
      )
    }

  }, [])

  return (
    <header className="header">

      <div className="headerLeft">

        <div ref={menuRef}>

          <button
            className="menuButton"
            onClick={() => setOpen(!open)}
          >
            ☰
          </button>

          {open && (

            <div className="dropdownMenu">

              <Link to="/" onClick={() => setOpen(false)}>
                home
              </Link>

              <Link to="/3cage" onClick={() => setOpen(false)}>
                3cage
              </Link>

              <Link to="/jorj" onClick={() => setOpen(false)}>
                jorj
              </Link>

              <Link to="/download" onClick={() => setOpen(false)}>
                download
              </Link>

              <a
                href="https://github.com/jorj1357/mimita-public"
                target="_blank"
                rel="noopener noreferrer"
                onClick={() => setOpen(false)}
              >
                github
              </a>

              <a
                href="https://discord.gg/4MRjvpDu7e"
                target="_blank"
                rel="noopener noreferrer"
                onClick={() => setOpen(false)}
              >
                discord
              </a>

              <Link to="/about" onClick={() => setOpen(false)}>
                about
              </Link>

              <Link to="/articles" onClick={() => setOpen(false)}>
                articles
              </Link>

              <Link to="/socials" onClick={() => setOpen(false)}>
                socials
              </Link>

              <Link to="/contribute" onClick={() => setOpen(false)}>
                contribute
              </Link>

              <Link to="/newsletter" onClick={() => setOpen(false)}>
                newsletter
              </Link>

              <Link to="/terms" onClick={() => setOpen(false)}>
                terms
              </Link>

              <Link to="/signin" onClick={() => setOpen(false)}>
                sign in
              </Link>

              <Link to="/account" onClick={() => setOpen(false)}>
                account
              </Link>

            </div>

          )}

        </div>

        <Link className="headerMainLink" to="/">
          mimita
        </Link>

        <Link className="headerMainLink" to="/3cage">
          3cage
        </Link>

        <Link className="headerMainLink" to="/jorj">
          jorj
        </Link>

        <a
          className="headerMainLink"
          href="https://github.com/jorj1357/mimita-public"
          target="_blank"
          rel="noopener noreferrer"
        >
          github
        </a>

        <a
          className="headerMainLink"
          href="https://discord.gg/4MRjvpDu7e"
          target="_blank"
          rel="noopener noreferrer"
        >
          discord
        </a>

        <Link className="headerMainLink" to="/contribute">
          donate
        </Link>

      </div>

    </header>
  )
}
