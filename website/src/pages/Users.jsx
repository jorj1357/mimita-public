import { useEffect, useState } from "react"
import { Link } from "react-router-dom"
import Layout from "../components/Layout"
import Avatar from "../components/Avatar"

const SORT_OPTIONS = [
    { value: "newest", label: "Newest Joined" },
    { value: "oldest", label: "Oldest Joined" },
    { value: "username_az", label: "Username A → Z" },
    { value: "username_za", label: "Username Z → A" },
    { value: "achievements", label: "Most Achievements" },
    { value: "least_achievements", label: "Least Achievements" }
]

export default function Users() {
    const [users, setUsers] = useState([])
    const [loading, setLoading] = useState(true)
    const [error, setError] = useState("")
    const [search, setSearch] = useState("")
    const [sort, setSort] = useState("newest")
    const [page, setPage] = useState(1)
    const [pages, setPages] = useState(1)
    const [total, setTotal] = useState(0)

    useEffect(() => {
        setPage(1)
    }, [search, sort])

    useEffect(() => {
        setLoading(true)
        setError("")
        const timer = setTimeout(() => {
            const params = new URLSearchParams({
                page: String(page),
                limit: "50",
                sort,
                ...(search.trim() ? { search: search.trim() } : {})
            })
            fetch(`/api/users?${params}`)
                .then(r => r.json())
                .then(data => {
                    if (data.success) {
                        setUsers(data.users)
                        setPages(data.pages)
                        setTotal(data.total)
                    } else {
                        setError(data.message || "failed to load users")
                    }
                })
                .catch(() => setError("unable to reach server"))
                .finally(() => setLoading(false))
        }, search.trim() ? 300 : 0)
        return () => clearTimeout(timer)
    }, [page, sort, search])

    function formatDate(dateStr) {
        if (!dateStr) return "Unknown"
        const d = new Date(dateStr)
        const mm = String(d.getMonth() + 1).padStart(2, "0")
        const dd = String(d.getDate()).padStart(2, "0")
        const yyyy = d.getFullYear()
        return `${mm}-${dd}-${yyyy}`
    }

    return (
        <Layout>
            <section className="usersPage">
                <h1>Players</h1>
                <p className="usersSubtitle">{total} registered players</p>

                <div className="usersControls">
                    <input
                        className="usersSearch"
                        type="text"
                        placeholder="search by username or bio..."
                        value={search}
                        onChange={e => setSearch(e.target.value)}
                    />
                    <select
                        className="usersSort"
                        value={sort}
                        onChange={e => setSort(e.target.value)}
                    >
                        {SORT_OPTIONS.map(o => (
                            <option key={o.value} value={o.value}>{o.label}</option>
                        ))}
                    </select>
                </div>

                {loading ? (
                    <p className="usersLoading">loading...</p>
                ) : error ? (
                    <p className="usersError">{error}</p>
                ) : users.length === 0 ? (
                    <p className="usersEmpty">no players found</p>
                ) : (
                    <>
                        <div className="usersGrid">
                            {users.map(u => (
                                <Link
                                    key={u.username}
                                    to={`/users/${encodeURIComponent(u.username)}`}
                                    className="usersCard"
                                >
                                    <Avatar user={u} size="lg" />
                                    <strong className="usersCardName">{u.username}</strong>
                                    {u.achievement_count > 0 && (
                                        <span className="usersCardAchievements">
                                            {u.achievement_count} achievement{u.achievement_count !== 1 ? "s" : ""}
                                        </span>
                                    )}
                                    <span className="usersCardJoined">
                                        Joined {formatDate(u.created_at)}
                                    </span>
                                    {u.bio && (
                                        <p className="usersCardBio">{u.bio}</p>
                                    )}
                                </Link>
                            ))}
                        </div>

                        {pages > 1 && (
                            <div className="usersPagination">
                                <button
                                    className="usersPageBtn"
                                    disabled={page <= 1}
                                    onClick={() => setPage(p => p - 1)}
                                >
                                    Previous
                                </button>
                                <span className="usersPageInfo">
                                    Page {page} of {pages}
                                </span>
                                <button
                                    className="usersPageBtn"
                                    disabled={page >= pages}
                                    onClick={() => setPage(p => p + 1)}
                                >
                                    Next
                                </button>
                            </div>
                        )}
                    </>
                )}
            </section>
        </Layout>
    )
}
