import { useState } from "react"
import "../App.css"
import "../styles/support.css"
import Layout from "../components/Layout"
import PixelBox from "../components/PixelBox"

export default function Support() {
  const [email, setEmail] = useState("")
  const [subject, setSubject] = useState("")
  const [message, setMessage] = useState("")
  const [sent, setSent] = useState(false)

  async function handleSubmit(e) {
    e.preventDefault()
    try {
      const res = await fetch("/api/support", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ email, subject, message })
      })
      if (res.ok) {
        setSent(true)
        setEmail("")
        setSubject("")
        setMessage("")
      }
    } catch (_) {}
  }

  return (
    <Layout>
      <div className="supportPage">
        <h1 className="supportTitle">SUPPORT</h1>

        <PixelBox>
          {sent ? (
            <p className="supportSent">Your message has been sent. We will get back to you at {email}.</p>
          ) : (
            <form className="supportForm" onSubmit={handleSubmit}>
              <label className="supportLabel">Your Email</label>
              <input className="supportInput" type="email" value={email} onChange={e => setEmail(e.target.value)} required placeholder="you@example.com" />

              <label className="supportLabel">Subject</label>
              <input className="supportInput" type="text" value={subject} onChange={e => setSubject(e.target.value)} required placeholder="What is this about?" />

              <label className="supportLabel">Message</label>
              <textarea className="supportTextarea" value={message} onChange={e => setMessage(e.target.value)} required rows={6} placeholder="Describe your issue in detail..." />

              <button type="submit" className="supportSubmit">Send</button>
            </form>
          )}
        </PixelBox>
      </div>
    </Layout>
  )
}
