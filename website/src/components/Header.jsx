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

              <Link to="/about" onClick={() => setOpen(false)}>
                about
              </Link>

              <Link to="/contribute" onClick={() => setOpen(false)}>
                contribute
                </Link>

              <Link to="/download" onClick={() => setOpen(false)}>
                download
              </Link>

              {/* <Link to="/feedback" onClick={() => setOpen(false)}>
                FEEDBACK
              </Link> */}

              <Link to="/newsletter" onClick={() => setOpen(false)}>
                newsletter
              </Link>

              <Link to="/socials" onClick={() => setOpen(false)}>
                socials
              </Link>

              <Link to="/terms" onClick={() => setOpen(false)}>
                terms
              </Link>

            </div>

          )}

        </div>

        <Link className="headerMainLink" to="/">
          home
        </Link>

        <Link className="headerMainLink" to="/download">
          download
        </Link>

        <Link className="headerMainLink" to="/about">
          about
        </Link>

        <Link className="headerMainLink" to="/contribute">
        contribute
        </Link>

        <Link className="headerMainLink" to="/socials">
        socials
        </Link>

      </div>

    </header>
  )
}