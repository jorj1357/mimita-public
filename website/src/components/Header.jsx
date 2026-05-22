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
              ABOUT
            </Link>

            <Link to="/donate" onClick={() => setOpen(false)}>
              DONATE
            </Link>

            <Link to="/download" onClick={() => setOpen(false)}>
              DOWNLOAD
            </Link>

            <Link to="/feedback" onClick={() => setOpen(false)}>
              FEEDBACK
            </Link>

            <Link to="/" onClick={() => setOpen(false)}>
              HOME
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

    </header>
  )
}