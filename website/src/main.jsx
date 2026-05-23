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
{/* 
        <Route
          path="/feedback"
          element={<Feedback />}
        /> */}

        <Route
          path="/terms"
          element={<Terms />}
        />

      </Routes>

    </BrowserRouter>

  </React.StrictMode>
)