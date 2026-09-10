import { useEffect, useState } from "react"
import Layout from "../components/Layout"
import { apiRequest } from "../lib/api"

export default function AdminAnalytics() {
    const [data, setData] = useState(null)
    const [loading, setLoading] = useState(true)

    useEffect(() => {
        apiRequest("/api/admin/dashboard")
            .then(data => {
                if (data?.success) setData(data.metrics || data)
            })
            .catch(() => {})
            .finally(() => setLoading(false))
    }, [])

    function formatNumber(n) {
        if (n == null) return "0"
        return Number(n).toLocaleString()
    }

    return (
        <Layout>
            <section className="adminPage">
                <h1 className="pageHeading">Analytics Dashboard</h1>

                {loading ? (
                    <p>Loading analytics...</p>
                ) : !data ? (
                    <p>Failed to load analytics.</p>
                ) : (
                    <>
                        <div className="analyticsSection">
                            <h2>Join Events</h2>
                            {data.join_events ? (
                                <div className="analyticsGrid">
                                    <div className="analyticsCard">
                                        <h3>Today</h3>
                                        {Object.entries(data.join_events.today || {}).map(([source, count]) => (
                                            <div key={source} className="analyticsRow">
                                                <span>{source}</span>
                                                <span>{formatNumber(count)}</span>
                                            </div>
                                        ))}
                                        {Object.keys(data.join_events.today || {}).length === 0 && (
                                            <p>No joins today</p>
                                        )}
                                    </div>
                                    <div className="analyticsCard">
                                        <h3>This Week</h3>
                                        {Object.entries(data.join_events.thisWeek || {}).map(([source, count]) => (
                                            <div key={source} className="analyticsRow">
                                                <span>{source}</span>
                                                <span>{formatNumber(count)}</span>
                                            </div>
                                        ))}
                                    </div>
                                    <div className="analyticsCard">
                                        <h3>This Month</h3>
                                        {Object.entries(data.join_events.thisMonth || {}).map(([source, count]) => (
                                            <div key={source} className="analyticsRow">
                                                <span>{source}</span>
                                                <span>{formatNumber(count)}</span>
                                            </div>
                                        ))}
                                    </div>
                                    <div className="analyticsCard">
                                        <h3>All Time</h3>
                                        <div className="analyticsRow">
                                            <span>Total joins</span>
                                            <span>{formatNumber(data.join_events.allTime)}</span>
                                        </div>
                                    </div>
                                </div>
                            ) : (
                                <p>No join event data available.</p>
                            )}
                        </div>

                        <div className="analyticsSection">
                            <h2>Active Users</h2>
                            <div className="analyticsGrid">
                                <div className="analyticsCard">
                                    <h3>DAU</h3>
                                    <p className="analyticsBigNumber">{formatNumber(data.dau?.today)}</p>
                                </div>
                                <div className="analyticsCard">
                                    <h3>WAU</h3>
                                    <p className="analyticsBigNumber">{formatNumber(data.wau?.today)}</p>
                                </div>
                                <div className="analyticsCard">
                                    <h3>MAU</h3>
                                    <p className="analyticsBigNumber">{formatNumber(data.mau?.today)}</p>
                                </div>
                            </div>
                        </div>

                        <div className="analyticsSection">
                            <h2>Overview</h2>
                            <div className="analyticsGrid">
                                <div className="analyticsCard">
                                    <h3>Total Users</h3>
                                    <p className="analyticsBigNumber">{formatNumber(data.total_users)}</p>
                                </div>
                                <div className="analyticsCard">
                                    <h3>Accounts Created (Today)</h3>
                                    <p className="analyticsBigNumber">{formatNumber(data.accounts_created_today?.today)}</p>
                                </div>
                                <div className="analyticsCard">
                                    <h3>Active Sessions</h3>
                                    <p className="analyticsBigNumber">{formatNumber(data.active_sessions)}</p>
                                </div>
                                <div className="analyticsCard">
                                    <h3>Feedback (Today)</h3>
                                    <p className="analyticsBigNumber">{formatNumber(data.feedback_today)}</p>
                                </div>
                            </div>
                        </div>
                    </>
                )}
            </section>
        </Layout>
    )
}
