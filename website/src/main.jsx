import React from "react"
import ReactDOM from "react-dom/client"

import {
  BrowserRouter,
  Routes,
  Route,
} from "react-router-dom"

import "./App.css"

/* =========================
   PAGES
========================= */

import Home from "./pages/Home"

import About from "./pages/About"

import Download from "./pages/Download"

import Socials from "./pages/Socials"

import Contribute from "./pages/Contribute"

import Newsletter from "./pages/Newsletter"

import Feedback from "./pages/Feedback"

import Terms from "./pages/Terms"

// its actuallt 3cage but cant put numbers so whatever 5 25 2026
import ThreeCage from "./pages/Socials-Threecage"

// and this is jorj1357 but still shhh 
import Jorj from "./pages/Socials-jorj"

import NotFound from "./pages/NotFound"

/* =========================
   APP
========================= */

ReactDOM.createRoot(
  document.getElementById("root")
).render(

  <React.StrictMode>

    <BrowserRouter>

      <Routes>

        <Route
          path="/"
          element={<Home />}
        />

        <Route
          path="/about"
          element={<About />}
        />

        <Route
          path="/download"
          element={<Download />}
        />

        <Route
          path="/socials"
          element={<Socials />}
        />

        <Route
          path="/contribute"
          element={<Contribute />}
        />

        <Route
          path="/newsletter"
          element={<Newsletter />}
        />

        {/* commented bc doesnt work and ugh 5 25 2026  */}
{/* 
        <Route
          path="/feedback"
          element={<Feedback />}
        /> */}

        <Route
          path="/terms"
          element={<Terms />}
        />

        <Route
          path="/3cage"
          element={<ThreeCage />}
        />

        <Route
          path="/jorj"
          element={<Jorj />}
        />

        {/* goes last 5 25 2026
        becaues
        Putting it last is important because:
        path="*" catches everything that doesn’t exist.
        */}
        <Route
          path="*"
          element={<NotFound />}
        />

      </Routes>

    </BrowserRouter>

  </React.StrictMode>
)