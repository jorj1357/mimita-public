import { useEffect, useState } from "react"
import { useParams, Link } from "react-router-dom"
import Layout from "../components/Layout"
import Avatar from "../components/Avatar"
import Username from "../components/Username"
import { apiRequest } from "../lib/api"

export default function ForumCategory() {
    const { slug } = useParams()
    const [data, setData] = useState(null)
    const [loading, setLoading] = useState(true)
    const [newTitle, setNewTitle] = useState("")
    const [newBody, setNewBody] = useState("")
    const [creating, setCreating] = useState(false)
    const [showForm, setShowForm] = useState(false)

    useEffect(() => {
        setLoading(true)
        apiRequest(`/api/forum/categories/${slug}/threads`)
            .then(d => {
                if (d?.success) setData(d)
            })
            .catch(() => {})
            .finally(() => setLoading(false))
    }, [slug])

    async function handleCreateThread(e) {
        e.preventDefault()
        if (!newTitle.trim() || !newBody.trim() || creating) return
        setCreating(true)
        try {
            const result = await apiRequest("/api/forum/threads", {
                method: "POST",
                body: JSON.stringify({
                    categoryId: data?.category?.id,
                    title: newTitle.trim(),
                    body: newBody.trim()
                })
            })
            if (result?.success && result.thread) {
                setNewTitle("")
                setNewBody("")
                setShowForm(false)
                setData(prev => ({
                    ...prev,
                    threads: [
                        {
                            id: result.thread.id,
                            title: result.thread.title,
                            reply_count: 0,
                            pinned: false,
                            locked: false,
                            last_reply_at: null,
                            created_at: result.thread.created_at,
                            author_id: null,
                            username: "you",
                            avatar_url: "",
                            supporter_tier: "free",
                            role: "user"
                        },
                        ...(prev?.threads || [])
                    ]
                }))
            }
        } catch {}
        setCreating(false)
    }

    function formatDate(dateStr) {
        if (!dateStr) return ""
        const d = new Date(dateStr)
        return d.toLocaleDateString([], { month: "short", day: "numeric", year: "numeric" })
    }

    return (
        <Layout>
            <section className="forumPage">
                <div className="forumHeader">
                    <Link to="/forum" className="forumBack">&larr; Forum</Link>
                    <h1 className="pageHeading">{data?.category?.name || slug}</h1>
                    <p className="forumSubtitle">{data?.category?.description}</p>
                </div>

                <button className="forumNewThreadBtn" onClick={() => setShowForm(!showForm)}>
                    {showForm ? "Cancel" : "New Thread"}
                </button>

                {showForm && (
                    <form className="forumNewThreadForm" onSubmit={handleCreateThread}>
                        <input
                            type="text"
                            value={newTitle}
                            onChange={e => setNewTitle(e.target.value)}
                            placeholder="Thread title"
                            maxLength={200}
                            required
                        />
                        <textarea
                            value={newBody}
                            onChange={e => setNewBody(e.target.value)}
                            placeholder="Write your post..."
                            maxLength={10000}
                            rows={6}
                            required
                        />
                        <button type="submit" disabled={creating}>
                            {creating ? "Creating..." : "Create Thread"}
                        </button>
                    </form>
                )}

                {loading ? (
                    <p>Loading threads...</p>
                ) : !data?.threads?.length ? (
                    <p className="forumEmpty">No threads yet. Start the conversation!</p>
                ) : (
                    <div className="forumThreadList">
                        {data.threads.map(thread => (
                            <Link
                                key={thread.id}
                                to={`/forum/thread/${thread.id}`}
                                className="forumThreadCard"
                            >
                                <div className="forumThreadInfo">
                                    <h3 className="forumThreadTitle">
                                        {thread.pinned && <span className="pinIcon">&#x1F4CC;</span>}
                                        {thread.title}
                                    </h3>
                                    <span className="forumThreadMeta">
                                        by <Username user={thread} size="sm" /> &middot; {formatDate(thread.created_at)}
                                    </span>
                                </div>
                                <div className="forumThreadStats">
                                    <span>{thread.reply_count} {thread.reply_count === 1 ? "reply" : "replies"}</span>
                                    {thread.last_reply_at && (
                                        <span>Last reply {formatDate(thread.last_reply_at)}</span>
                                    )}
                                </div>
                            </Link>
                        ))}
                    </div>
                )}
            </section>
        </Layout>
    )
}
