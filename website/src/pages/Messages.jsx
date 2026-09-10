import { useEffect, useState, useRef } from "react"
import { Link } from "react-router-dom"
import Layout from "../components/Layout"
import Avatar from "../components/Avatar"
import Username from "../components/Username"
import { apiRequest } from "../lib/api"

export default function Messages() {
    const [conversations, setConversations] = useState([])
    const [loading, setLoading] = useState(true)
    const [activeId, setActiveId] = useState(null)
    const [messages, setMessages] = useState([])
    const [newMessage, setNewMessage] = useState("")
    const [sending, setSending] = useState(false)
    const messagesEnd = useRef()
    const inputRef = useRef()

    useEffect(() => {
        apiRequest("/api/messages/conversations")
            .then(data => {
                if (data?.success) setConversations(data.conversations || [])
            })
            .catch(() => {})
            .finally(() => setLoading(false))
    }, [])

    useEffect(() => {
        if (!activeId) return
        apiRequest(`/api/messages/conversations/${activeId}`)
            .then(data => {
                if (data?.success) setMessages(data.messages || [])
            })
            .catch(() => {})
    }, [activeId])

    useEffect(() => {
        messagesEnd.current?.scrollIntoView({ behavior: "smooth" })
    }, [messages])

    async function handleSend(e) {
        e.preventDefault()
        if (!newMessage.trim() || sending) return
        setSending(true)
        try {
            const data = await apiRequest(`/api/messages/conversations/${activeId}`, {
                method: "POST",
                body: JSON.stringify({ body: newMessage.trim() })
            })
            if (data?.success && data.message) {
                setMessages(prev => [...prev, data.message])
                setNewMessage("")
                setConversations(prev => prev.map(c =>
                    c.id === activeId
                        ? { ...c, last_message: data.message.body, last_message_at: data.message.created_at, last_message_sender_id: data.message.sender_id }
                        : c
                ))
            }
        } catch {}
        setSending(false)
        inputRef.current?.focus()
    }

    function formatTime(dateStr) {
        if (!dateStr) return ""
        const d = new Date(dateStr)
        const now = new Date()
        const diff = now - d
        if (diff < 86400000) {
            return d.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" })
        }
        return d.toLocaleDateString([], { month: "short", day: "numeric" })
    }

    function formatLastMessage(text) {
        if (!text) return ""
        return text.length > 50 ? text.slice(0, 50) + "..." : text
    }

    return (
        <Layout>
            <section className="messagesPage">
                <h1 className="pageHeading">Messages</h1>
                <div className="messagesContainer">
                    <div className="conversationList">
                        <h2 className="conversationListTitle">Conversations</h2>
                        {loading && <p>Loading conversations...</p>}
                        {!loading && conversations.length === 0 && (
                            <p className="conversationEmpty">No conversations yet. Visit a user's profile to start messaging.</p>
                        )}
                        {conversations.map(conv => (
                            <button
                                key={conv.id}
                                className={`conversationItem ${activeId === conv.id ? "active" : ""}`}
                                onClick={() => {
                                    setActiveId(conv.id)
                                    setConversations(prev => prev.map(c =>
                                        c.id === conv.id ? { ...c, unread_count: 0 } : c
                                    ))
                                }}
                            >
                                {conv.type === "dm" && conv.otherUser && (
                                    <Avatar user={conv.otherUser} size="sm" />
                                )}
                                <div className="conversationInfo">
                                    <span className="conversationName">
                                        {conv.type === "dm" && conv.otherUser
                                            ? conv.otherUser.username
                                            : `Conversation #${conv.id}`
                                        }
                                    </span>
                                    <span className="conversationPreview">
                                        {formatLastMessage(conv.last_message)}
                                    </span>
                                </div>
                                <div className="conversationMeta">
                                    <span className="conversationTime">
                                        {formatTime(conv.last_message_at || conv.created_at)}
                                    </span>
                                    {conv.unread_count > 0 && (
                                        <span className="unreadBadge">{conv.unread_count}</span>
                                    )}
                                </div>
                            </button>
                        ))}
                    </div>
                    <div className="messageThread">
                        {activeId ? (
                            <>
                                <div className="messageThreadHeader">
                                    <h3>
                                        {conversations.find(c => c.id === activeId)?.type === "dm"
                                            ? conversations.find(c => c.id === activeId)?.otherUser?.username
                                            : `Conversation #${activeId}`
                                        }
                                    </h3>
                                </div>
                                <div className="messageThreadBody">
                                    {messages.map(msg => (
                                        <div key={msg.id} className={`messageBubble ${msg.sender_id === parseInt(document.cookie.match(/user_id=(\d+)/)?.[1]) ? "own" : ""}`}>
                                            <div className="messageHeader">
                                                <Avatar user={msg} size="xs" />
                                                <Username user={msg} size="sm" />
                                                <span className="messageTime">{formatTime(msg.created_at)}</span>
                                            </div>
                                            <p className="messageBody">{msg.body}</p>
                                        </div>
                                    ))}
                                    <div ref={messagesEnd} />
                                </div>
                                <form className="messageInput" onSubmit={handleSend}>
                                    <input
                                        ref={inputRef}
                                        type="text"
                                        value={newMessage}
                                        onChange={e => setNewMessage(e.target.value)}
                                        placeholder="Type a message..."
                                        maxLength={2000}
                                        disabled={sending}
                                    />
                                    <button type="submit" disabled={!newMessage.trim() || sending}>
                                        {sending ? "..." : "Send"}
                                    </button>
                                </form>
                            </>
                        ) : (
                            <div className="messageThreadEmpty">
                                <p>Select a conversation to start messaging.</p>
                            </div>
                        )}
                    </div>
                </div>
            </section>
        </Layout>
    )
}
