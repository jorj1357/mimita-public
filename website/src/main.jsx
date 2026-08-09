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
import Contact from "./pages/Contact"
import FAQ from "./pages/FAQ"
import News from "./pages/News"
import Leaderboard from "./pages/Leaderboard"
import Support from "./pages/Support"
import Contribute from "./pages/Contribute"
import Terms from "./pages/Terms"
import NotFound from "./pages/NotFound"
import Auth from "./pages/Auth"
import ForgotPassword from "./pages/ForgotPassword"
import ArticlesIndex from "./pages/ArticlesIndex"
import ArticlePage from "./pages/ArticlePage"
import Account from "./pages/Account"
import UserProfile from "./pages/UserProfile"
import Users from "./pages/Users"
import Vip from "./pages/Vip"
import VipSuccess from "./pages/VipSuccess"
import AdminLogin from "./pages/AdminLogin"
import AdminDashboard from "./pages/AdminDashboard"
import AdminNoPermission from "./pages/AdminNoPermission"
import EmailCampaigns from "./pages/EmailCampaigns"
import ProfilePage from "./pages/ProfilePage"
import Feedback from "./pages/Feedback"
import Link from "./pages/Link"
import ClientSignIn from "./pages/ClientSignIn"
import Games from "./pages/Games"
import AimTestV1 from "./pages/AimTestV1"
import RhythmTestV1 from "./pages/RhythmTestV1"
import DreamToy from "./pages/DreamToy"
import ChiliRace from "./pages/ChiliRace"
import AdminArticleEditor from "./pages/AdminArticleEditor"
import Jorj from "./pages/Socials-jorj"
import BannerCreate from "./pages/BannerCreate"
import BannerSuccess from "./pages/BannerSuccess"
import BannerStatus from "./pages/BannerStatus"
import AdminBanner from "./pages/AdminBanner"
import AdminSupport from "./pages/AdminSupport"

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
        <Route path="/contact" element={<Contact />} />
        <Route path="/faq" element={<FAQ />} />
        <Route path="/news" element={<News />} />
        <Route path="/leaderboard" element={<Leaderboard />} />
        <Route path="/support" element={<Support />} />
        <Route path="/contribute" element={<Contribute />} />
        <Route path="/terms" element={<Terms />} />

        <Route path="/signup" element={<Auth mode="signup" />} />
        <Route path="/signin" element={<Auth mode="signin" />} />
        <Route path="/forgot-password" element={<ForgotPassword />} />
        <Route path="/account" element={<Account />} />
        <Route path="/vip" element={<Vip />} />
        <Route path="/vip/success" element={<VipSuccess />} />

        <Route path="/users" element={<Users />} />
        <Route path="/users/id/:id" element={<UserProfile />} />
        <Route path="/users/:username" element={<UserProfile />} />
        <Route path="/u/:username" element={<UserProfile />} />
        <Route path="/profile" element={<ProfilePage />} />

        <Route path="/articles" element={<ArticlesIndex />} />
        <Route path="/articles/:slug" element={<ArticlePage />} />

        <Route path="/admin" element={<AdminLogin />} />
        <Route path="/admin/login" element={<AdminLogin />} />
        <Route path="/admin/dashboard" element={<AdminDashboard />} />
        <Route path="/admin/email-campaigns" element={<EmailCampaigns />} />
        <Route path="/admin/articles" element={<AdminArticleEditor />} />
        <Route path="/admin/no-permission" element={<AdminNoPermission />} />

        <Route path="/feedback" element={<Feedback />} />
        <Route path="/link" element={<Link />} />
        <Route path="/clientsignin" element={<ClientSignIn />} />
        <Route path="/jorj1357" element={<Jorj />} />

        <Route path="/games" element={<Games />} />
        <Route path="/games/aim-test-v1" element={<AimTestV1 />} />
        <Route path="/games/rhythm-test-v1" element={<RhythmTestV1 />} />
        <Route path="/games/dream-toy" element={<DreamToy />} />
        <Route path="/games/chili-race" element={<ChiliRace />} />

        <Route path="/banner" element={<BannerCreate />} />
        <Route path="/banner/create" element={<BannerCreate />} />
        <Route path="/banner/success" element={<BannerSuccess />} />
        <Route path="/banner/status" element={<BannerStatus />} />
        <Route path="/admin/banners" element={<AdminBanner />} />
        <Route path="/admin/support" element={<AdminSupport />} />

        <Route path="*" element={<NotFound />} />

      </Routes>

    </BrowserRouter>

  </React.StrictMode>
)
