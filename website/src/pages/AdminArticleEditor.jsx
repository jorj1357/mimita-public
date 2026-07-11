import { useState, useEffect, useRef, useCallback } from "react"
import { useNavigate, Link } from "react-router-dom"
import { apiRequest } from "../lib/api.js"
import Markdown from "react-markdown"
import remarkGfm from "remark-gfm"
import remarkBreaks from "remark-breaks"
import rehypeRaw from "rehype-raw"
import Layout from "../components/Layout"

export default function AdminArticleEditor() {
    const navigate = useNavigate()
    const [articles, setArticles] = useState([])
    const [loading, setLoading] = useState(true)
    const [error, setError] = useState("")
    const [editing, setEditing] = useState(null)
    const [slug, setSlug] = useState("")
    const [title, setTitle] = useState("")
    const [content, setContent] = useState("")
    const [description, setDescription] = useState("")
    const [tags, setTags] = useState("")
    const [date, setDate] = useState(new Date().toISOString().split("T")[0])
    const [author, setAuthor] = useState("")
    const [published, setPublished] = useState(true)
    const [saving, setSaving] = useState(false)
    const [message, setMessage] = useState("")
    const [showPreview, setShowPreview] = useState(false)
    const textareaRef = useRef(null)
    const fetchedRef = useRef(false)

    useEffect(() => {
        if (fetchedRef.current) return
        fetchedRef.current = true
        fetchArticles()
    }, [])

    async function fetchArticles() {
        setLoading(true)
        setError("")
        try {
            const data = await apiRequest("/api/admin/articles")
            if (data?.success) {
                setArticles(data.articles || [])
            } else {
                setError(data?.message || "failed to load")
            }
        } catch (e) {
            if (e.message?.includes("401")) { navigate("/admin/login"); return }
            if (e.message?.includes("403")) { navigate("/admin/no-permission"); return }
            setError(e.message || "failed to load articles")
        } finally {
            setLoading(false)
        }
    }

    async function loadArticle(slug) {
        try {
            const data = await apiRequest(`/api/admin/articles/${slug}`)
            if (data?.success && data?.article) {
                const a = data.article
                setEditing(a.slug)
                setSlug(a.slug)
                setTitle(a.title || "")
                setContent(a.content || "")
                setDescription(a.description || "")
                setTags((a.tags || []).join(", "))
                setDate(a.date || "")
                setAuthor(a.author || "")
                setPublished(a.published !== false)
                setMessage("")
            }
        } catch (e) {
            setMessage("error loading article: " + (e.message || ""))
        }
    }

    function newArticle() {
        setEditing(null)
        setSlug("")
        setTitle("")
        setContent("")
        setDescription("")
        setTags("")
        setDate(new Date().toISOString().split("T")[0])
        setAuthor("")
        setPublished(true)
        setMessage("")
    }

    function slugify(str) {
        return str.toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-|-$/g, "")
    }

    function titleChanged(val) {
        setTitle(val)
        if (!editing && !slug) {
            setSlug(slugify(val))
        }
    }

    async function handleSave(e) {
        e.preventDefault()
        if (!slug || !title) {
            setMessage("slug and title are required")
            return
        }
        setSaving(true)
        setMessage("")
        const safeSlug = slugify(slug)
        setSlug(safeSlug)
        try {
            const tagList = tags.split(",").map(t => t.trim()).filter(Boolean)
            const data = await apiRequest("/api/admin/articles", {
                method: "POST",
                body: JSON.stringify({
                    slug: safeSlug,
                    title,
                    description,
                    date,
                    author: author || "admin",
                    tags: tagList,
                    content,
                    published
                })
            })
            if (data?.success) {
                setMessage("saved!")
                setEditing(slug)
                fetchArticles()
            } else {
                setMessage(data?.message || "save failed")
            }
        } catch (e) {
            setMessage("error: " + (e.message || ""))
        } finally {
            setSaving(false)
        }
    }

    async function handleDelete(slug) {
        if (!confirm(`delete "${slug}"? this cannot be undone.`)) return
        try {
            const data = await apiRequest(`/api/admin/articles/${slug}`, { method: "DELETE" })
            if (data?.success) {
                if (editing === slug) newArticle()
                fetchArticles()
                setMessage("deleted")
            }
        } catch (e) {
            setMessage("delete failed: " + (e.message || ""))
        }
    }

    // Pre-process content: convert [rainbow] tags to HTML spans
    function renderContent(md) {
        if (!md) return ""
        return md.replace(/\[rainbow\](.*?)\[\/rainbow\]/gs,
            '<span class="rainbow-text">$1</span>')
    }

    function wrapSelection(before, after) {
        const ta = textareaRef.current
        if (!ta) return
        const start = ta.selectionStart
        const end = ta.selectionEnd
        const selected = content.substring(start, end)
        if (selected) {
            setContent(content.slice(0, start) + before + selected + after + content.slice(end))
            requestAnimationFrame(() => {
                ta.focus()
                ta.selectionStart = start + before.length
                ta.selectionEnd = start + before.length + selected.length
            })
        } else {
            setContent(c => c + before + after)
        }
    }

    function prefixLines(prefix) {
        const ta = textareaRef.current
        if (!ta) return
        const start = ta.selectionStart
        const end = ta.selectionEnd
        const selected = content.substring(start, end)
        if (selected) {
            const prefixed = selected.split("\n").map(l => prefix + l).join("\n")
            setContent(content.slice(0, start) + prefixed + content.slice(end))
            requestAnimationFrame(() => {
                ta.focus()
                ta.selectionStart = start
                ta.selectionEnd = start + prefixed.length
            })
        } else {
            setContent(c => c + "\n" + prefix)
        }
    }

    function insertAtCursor(text) {
        const ta = textareaRef.current
        if (!ta) return
        const start = ta.selectionStart
        setContent(content.slice(0, start) + text + content.slice(ta.selectionEnd))
        requestAnimationFrame(() => {
            ta.focus()
            ta.selectionStart = ta.selectionEnd = start + text.length
        })
    }

    if (loading) {
        return (
            <Layout>
                <div className="adminEditorPage">
                    <p className="aboutSmall">loading...</p>
                </div>
            </Layout>
        )
    }

    return (
        <Layout>
            <div className="adminEditorPage">
                <h1 className="adminEditorTitle">article editor</h1>

                {error && <p className="adminEditorError">{error}</p>}

                <div className="adminEditorSplit">
                    <div className="adminEditorSidebar">
                        <h2 className="adminEditorSectionTitle">articles</h2>
                        <button className="adminEditorNewBtn" onClick={newArticle}>
                            + new article
                        </button>
                        <div className="adminEditorArticleList">
                            {articles.map(a => (
                                <div key={a.slug}
                                    className={"adminEditorListItem" + (editing === a.slug ? " active" : "")}
                                    onClick={() => loadArticle(a.slug)}>
                                    <span className="adminEditorListItemTitle">{a.title}</span>
                                    <span className="adminEditorListItemDate">{a.date}</span>
                                    <button className="adminEditorDeleteBtn"
                                        onClick={(e) => { e.stopPropagation(); handleDelete(a.slug) }}
                                        title="delete">x</button>
                                </div>
                            ))}
                        </div>
                    </div>

                    <div className="adminEditorMain">
                        {message && <p className="adminEditorMessage">{message}</p>}

                        <form className="adminEditorForm" onSubmit={handleSave}>
                            <div className="adminEditorField">
                                <label>slug</label>
                                <input type="text" value={slug}
                                    onChange={e => setSlug(e.target.value)}
                                    onBlur={e => setSlug(slugify(e.target.value))}
                                    placeholder="my-article-slug" required />
                            </div>

                            <div className="adminEditorField">
                                <label>title</label>
                                <input type="text" value={title}
                                    onChange={e => titleChanged(e.target.value)}
                                    placeholder="Article Title" required />
                            </div>

                            <div className="adminEditorField">
                                <label>description</label>
                                <input type="text" value={description}
                                    onChange={e => setDescription(e.target.value)}
                                    placeholder="Short description for card preview" />
                            </div>

                            <div className="adminEditorRow">
                                <div className="adminEditorField">
                                    <label>date</label>
                                    <input type="date" value={date}
                                        onChange={e => setDate(e.target.value)} />
                                </div>
                                <div className="adminEditorField">
                                    <label>author</label>
                                    <input type="text" value={author}
                                        onChange={e => setAuthor(e.target.value)}
                                        placeholder="admin" />
                                </div>
                            </div>

                            <div className="adminEditorField">
                                <label>tags (comma separated)</label>
                                <input type="text" value={tags}
                                    onChange={e => setTags(e.target.value)}
                                    placeholder="update, replay, feature" />
                            </div>

                            <div className="adminEditorField">
                                <label>
                                    <input type="checkbox" checked={published}
                                        onChange={e => setPublished(e.target.checked)} />
                                    {" "}published
                                </label>
                            </div>

                            <div className="adminEditorField">
                                <label>
                                    content (markdown)
                                    <span className="adminEditorHint">
                                        {" "}use [rainbow]text[/rainbow] for rainbow effect
                                    </span>
                                </label>
                                <div className="adminEditorToolbar">
                                    <button type="button" onClick={() => wrapSelection("**", "**")}
                                        className="adminEditorToolBtn">B</button>
                                    <button type="button" onClick={() => wrapSelection("*", "*")}
                                        className="adminEditorToolBtn">I</button>
                                    <button type="button" onClick={() => wrapSelection("[rainbow]", "[/rainbow]")}
                                        className="adminEditorToolBtn rainbowToolBtn">🌈</button>
                                    <button type="button"
                                        onClick={() => prefixLines("- ")}
                                        className="adminEditorToolBtn">• list</button>
                                    <button type="button"
                                        onClick={() => prefixLines("## ")}
                                        className="adminEditorToolBtn">H</button>
                                    <button type="button"
                                        onClick={() => insertAtCursor("\n---\n")}
                                        className="adminEditorToolBtn">—</button>
                                    <button type="button"
                                        onClick={() => wrapSelection("\n```\n", "\n```\n")}
                                        className="adminEditorToolBtn">&lt;/&gt;</button>
                                    <button type="button"
                                        onClick={() => prefixLines("> ")}
                                        className="adminEditorToolBtn">&ldquo;</button>
                                    <button type="button"
                                        onClick={() => wrapSelection("[", "](url)")}
                                        className="adminEditorToolBtn">🔗</button>
                                    <button type="button"
                                        onClick={() => setShowPreview(p => !p)}
                                        className={"adminEditorToolBtn" + (showPreview ? " active" : "")}>
                                        {showPreview ? "edit" : "preview"}
                                    </button>
                                </div>
                                {showPreview ? (
                                    <div className="adminEditorPreview">
                                        <Markdown remarkPlugins={[remarkGfm, remarkBreaks]} rehypePlugins={[rehypeRaw]}>
                                            {renderContent(content)}
                                        </Markdown>
                                    </div>
                                ) : (
                                    <textarea ref={textareaRef} className="adminEditorTextarea"
                                        value={content}
                                        onChange={e => setContent(e.target.value)}
                                        placeholder="Write your article in markdown..."
                                        rows={20} />
                                )}
                            </div>

                            <button type="submit" className="adminEditorSaveBtn"
                                disabled={saving}>
                                {saving ? "saving..." : "save article"}
                            </button>
                        </form>
                    </div>
                </div>
            </div>
        </Layout>
    )
}
