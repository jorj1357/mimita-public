import { useEffect, useState } from "react"
import { Link } from "react-router-dom"
import Layout from "../components/Layout"
import FeedbackBox from "../components/FeedbackBox"

export default function AdminNoPermission() {
    const [user, setUser] = useState(null)

    useEffect(() => {
        fetch("/api/auth/me", { credentials: "include" })
            .then((r) => r.json())
            .then((data) => {
                if (data.success) setUser(data.user)
            })
            .catch(() => {})
    }, [])

    return (
        <Layout>
            <div className="adminNoPermPage">
                <div className="adminNoPermCard">
                    <div className="adminNoPermIcon" aria-hidden="true">
                        <svg width="64" height="64" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5">
                            <path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z" stroke="#ef4444" fill="none" />
                            <line x1="15" y1="9" x2="9" y2="15" stroke="#ef4444" strokeWidth="2" />
                            <line x1="9" y1="9" x2="15" y2="15" stroke="#ef4444" strokeWidth="2" />
                        </svg>
                    </div>

                    <h1 className="adminNoPermTitle">
                        You do not have permission to view this page.
                    </h1>

                    <p className="adminNoPermText">
                        This page requires administrator permissions.
                        If you believe this is incorrect, let us know below.
                    </p>

                    <div className="adminNoPermActions">
                        <Link to="/" className="adminNoPermBtn">
                            Return Home
                        </Link>
                        {!user && (
                            <Link to="/signin" className="adminNoPermBtn adminNoPermBtnSecondary">
                                Sign In
                            </Link>
                        )}
                    </div>
                </div>

                <FeedbackBox pageName="admin-no-permission" />
            </div>
        </Layout>
    )
}
