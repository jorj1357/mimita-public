import React from 'react'
import ReactDOM from 'react-dom/client'
import About from "./pages/About"
import Donate from "./pages/Donate"

import {
  BrowserRouter,
  Routes,
  Route,
} from "react-router-dom"

import "./App.css"

import Home from "./pages/Home"
import Download from "./pages/Download"
import Socials from "./pages/Socials"

ReactDOM.createRoot(document.getElementById('root')).render(
  <React.StrictMode>

    <BrowserRouter>

      <Routes>

        <Route
          path="/"
          element={<Home />}
        />

        <Route
          path="/donate"
          element={<Donate />}
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
          path="/about"
          element={<About />}
        />

      </Routes>

    </BrowserRouter>

  </React.StrictMode>,
)