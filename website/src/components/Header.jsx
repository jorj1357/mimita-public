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
                HOME
              </Link>

              <Link to="/about" onClick={() => setOpen(false)}>
                ABOUT
              </Link>

              <Link to="/contribute" onClick={() => setOpen(false)}>
                CONTRIBUTE
                </Link>

              <Link to="/download" onClick={() => setOpen(false)}>
                DOWNLOAD
              </Link>

              <Link to="/feedback" onClick={() => setOpen(false)}>
                FEEDBACK
              </Link>

              <Link to="/newsletter" onClick={() => setOpen(false)}>
                NEWSLETTER
              </Link>

              <Link to="/socials" onClick={() => setOpen(false)}>
                SOCIALS
              </Link>

              <Link to="/terms" onClick={() => setOpen(false)}>
                TERMS
              </Link>

            </div>

          )}

        </div>

        <Link className="headerMainLink" to="/">
          HOME
        </Link>

        <Link className="headerMainLink" to="/download">
          DOWNLOAD
        </Link>

        <Link className="headerMainLink" to="/about">
          ABOUT
        </Link>

        <Link className="headerMainLink" to="/contribute">
        CONTRIBUTE
        </Link>

      </div>

    </header>
  )
}