import React from 'react'
import ReactDOM from 'react-dom/client'

import {
  BrowserRouter,
  Routes,
  Route,
} from "react-router-dom"

import App from './App.jsx'
import Download from './pages/Download.jsx'

import './App.css'

ReactDOM.createRoot(document.getElementById('root')).render(
  <React.StrictMode>

    <BrowserRouter>

      <Routes>
        <Route path="/" element={<App />} />
        <Route path="/download" element={<Download />} />
      </Routes>

    </BrowserRouter>

  </React.StrictMode>,
)