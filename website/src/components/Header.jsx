import { Link } from "react-router-dom"

export default function Header() {
  return (
    <header className="header">

      <Link to="/">
        HOME
      </Link>

      <Link to="/download">
        MIMITA DOWNLOAD
      </Link>

      <Link to="/socials">
        SOCIAL LINKS
      </Link>

      <Link to="/about">
        ABOUT
      </Link>

      <Link to="/feedback">
        FEEDBACK
      </Link>

      <Link to="/donate">
        DONATE
      </Link>

    </header>
  )
}