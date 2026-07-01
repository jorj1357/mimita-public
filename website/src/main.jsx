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
import Terms from "./pages/Terms"
import ThreeCage from "./pages/Socials-Threecage"
import Jorj from "./pages/Socials-jorj"
import NotFound from "./pages/NotFound"
import Auth from "./pages/Auth"
import ArticlesIndex from "./pages/ArticlesIndex"
import ArticlePage from "./pages/ArticlePage"
import Account from "./pages/Account"
import PasswordChange from "./pages/PasswordChange"
import UserProfile from "./pages/UserProfile"
import Users from "./pages/Users"
import PasswordPrinciples from "./pages/PasswordPrinciples"
import AdminLogin from "./pages/AdminLogin"
import AdminDashboard from "./pages/AdminDashboard"
import AdminNoPermission from "./pages/AdminNoPermission"
import ProfilePage from "./pages/ProfilePage"
import Feedback from "./pages/Feedback"
import Link from "./pages/Link"
import ClientSignIn from "./pages/ClientSignIn"
import Games from "./pages/Games"
import AimTestV1 from "./pages/AimTestV1"

/* =========================
   APP
========================= */

ReactDOM.createRoot(
  document.getElementById("root")
).render(

  <React.StrictMode>

    <BrowserRouter>

      <Routes>

        <Route path="/" element={<Home />} />
        <Route path="/about" element={<About />} />
        <Route path="/download" element={<Download />} />
        <Route path="/socials" element={<Socials />} />
        <Route path="/contribute" element={<Contribute />} />
        <Route path="/newsletter" element={<Newsletter />} />
        <Route path="/terms" element={<Terms />} />

        <Route path="/signup" element={<Auth mode="signup" />} />
        <Route path="/signin" element={<Auth mode="signin" />} />
        <Route path="/account" element={<Account />} />
        <Route path="/change-password" element={<PasswordChange />} />
        <Route path="/password-principles" element={<PasswordPrinciples />} />

        <Route path="/users" element={<Users />} />
        <Route path="/users/:username" element={<UserProfile />} />
        <Route path="/u/:username" element={<UserProfile />} />
        <Route path="/profile" element={<ProfilePage />} />

        <Route path="/3cage" element={<ThreeCage />} />
        <Route path="/jorj" element={<Jorj />} />
        <Route path="/articles" element={<ArticlesIndex />} />
        <Route path="/articles/:slug" element={<ArticlePage />} />

        <Route path="/admin" element={<AdminLogin />} />
        <Route path="/admin/login" element={<AdminLogin />} />
        <Route path="/admin/dashboard" element={<AdminDashboard />} />
        <Route path="/admin/no-permission" element={<AdminNoPermission />} />

        <Route path="/feedback" element={<Feedback />} />
        <Route path="/link" element={<Link />} />
        <Route path="/clientsignin" element={<ClientSignIn />} />

        <Route path="/games" element={<Games />} />
        <Route path="/games/aim-test-v1" element={<AimTestV1 />} />

        <Route path="*" element={<NotFound />} />

      </Routes>

    </BrowserRouter>

  </React.StrictMode>
)
