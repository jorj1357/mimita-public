import { useEffect, useState } from "react"
import { useParams, Link } from "react-router-dom"
import Layout from "../components/Layout"
import Avatar from "../components/Avatar"
import Username from "../components/Username"
import { apiRequest } from "../lib/api"

const QUICK_REACTIONS = ["&#x1F44D;", "&#x2764;&#xFE0F;", "&#x1F602;", "&#x1F60E;", "&#x1F525;"]

export default function ForumThread() {
    const { id } = useParams()
    const [thread, setThread] = useState(null)
    const [posts, setPosts] = useState([])
    const [loading, setLoading] = useState(true)
    const [replyBody, setReplyBody] = useState("")
    const [sending, setSending] = useState(false)
    const [reactions, setReactions] = useState({})

    useEffect(() => {
        apiRequest(`/api/forum/threads/${id}`)
            .then(data => {
                if (data?.success) {
                    setThread(data.thread)
                    setPosts(data.posts || [])
                    const r = {}
                    for (const post of data.posts || []) {
                        r[post.id] = post.reactions || []
                    }
                    setReactions(r)
                }
            })
            .catch(() => {})
            .finally(() => setLoading(false))
    }, [id])

    async function handleReply(e) {
        e.preventDefault()
        if (!replyBody.trim() || sending) return
        setSending(true)
        try {
            const data = await apiRequest(`/api/forum/threads/${id}/posts`, {
                method: "POST",
                body: JSON.stringify({ body: replyBody.trim() })
            })
            if (data?.success && data.post) {
                setPosts(prev => [...prev, data.post])
                setReactions(prev => ({ ...prev, [data.post.id]: [] }))
                setReplyBody("")
                setThread(prev => prev ? { ...prev, reply_count: (prev.reply_count || 0) + 1 } : prev)
            }
        } catch {}
        setSending(false)
    }

    async function handleReact(postId, emoji) {
        try {
            const data = await apiRequest(`/api/forum/posts/${postId}/react`, {
                method: "POST",
                body: JSON.stringify({ emoji })
            })
            if (data?.success) {
                setReactions(prev => {
                    const current = prev[postId] || []
                    if (data.action === "added") {
                        return { ...prev, [postId]: [...current, { userId: 0, emoji }] }
                    }
                    return { ...prev, [postId]: current.filter(r => r.emoji !== emoji) }
                })
            }
        } catch {}
    }

    function formatDate(dateStr) {
        if (!dateStr) return ""
        const d = new Date(dateStr)
        return d.toLocaleDateString([], { month: "short", day: "numeric", year: "numeric", hour: "2-digit", minute: "2-digit" })
    }

    if (loading) return <Layout><section className="forumPage"><p>Loading thread...</p></section></Layout>
    if (!thread) return <Layout><section className="forumPage"><p>Thread not found.</p></section></Layout>

    return (
        <Layout>
            <section className="forumPage">
                <div className="forumHeader">
                    <Link to={`/forum/${thread.category_slug}`} className="forumBack">&larr; {thread.category_name}</Link>
                    <h1 className="pageHeading">{thread.title}</h1>
                    <span className="forumThreadMeta">
                        by <Username user={thread} size="sm" /> &middot; {formatDate(thread.created_at)}
                    </span>
                </div>

                <div className="forumPosts">
                    {posts.map(post => (
                        <div key={post.id} className="forumPost">
                            <div className="forumPostSidebar">
                                <Avatar user={post} size="md" />
                                <Username user={post} size="sm" />
                            </div>
                            <div className="forumPostContent">
                                <p className="forumPostBody">{post.body}</p>
                                <div className="forumPostFooter">
                                    <span className="forumPostTime">{formatDate(post.created_at)}</span>
                                    <div className="forumReactions">
                                        {QUICK_REACTIONS.map(emoji => (
                                            <button
                                                key={emoji}
                                                className="forumReactionBtn"
                                                onClick={() => handleReact(post.id, emoji)}
                                                dangerouslySetInnerHTML={{ __html: emoji }}
                                            />
                                        ))}
                                    </div>
                                    {(reactions[post.id] || []).length > 0 && (
                                        <div className="forumReactionCounts">
                                            {Object.entries(
                                                (reactions[post.id] || []).reduce((acc, r) => {
                                                    acc[r.emoji] = (acc[r.emoji] || 0) + 1
                                                    return acc
                                                }, {})
                                            ).map(([emoji, count]) => (
                                                <span key={emoji} className="forumReactionCount" dangerouslySetInnerHTML={{ __html: `${emoji} ${count}` }} />
                                            ))}
                                        </div>
                                    )}
                                </div>
                            </div>
                        </div>
                    ))}
                </div>

                {!thread.locked ? (
                    <form className="forumReplyForm" onSubmit={handleReply}>
                        <textarea
                            value={replyBody}
                            onChange={e => setReplyBody(e.target.value)}
                            placeholder="Write a reply..."
                            maxLength={10000}
                            rows={4}
                            required
                        />
                        <button type="submit" disabled={!replyBody.trim() || sending}>
                            {sending ? "Posting..." : "Post Reply"}
                        </button>
                    </form>
                ) : (
                    <p className="forumLocked">This thread is locked.</p>
                )}
            </section>
        </Layout>
    )
}
