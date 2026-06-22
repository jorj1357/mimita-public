import { useState } from "react"
import { useLocation } from "react-router-dom"
import { apiRequest } from "../lib/api.js"

const PRESETS = [
    "Cool Site",
    "Bad Site",
    "Found A Bug",
    "Confusing Layout",
    "What's The Point?",
    "I Like This",
    "I Don't Like This"
]

const ROOT_CLASS = "feedbackBoxComponent"

export default function FeedbackBox({ pageName }) {
    const location = useLocation()
    const [selectedPresets, setSelectedPresets] = useState([])
    const [customText, setCustomText] = useState("")
    const [contactInfo, setContactInfo] = useState("")
    const [submitted, setSubmitted] = useState(false)
    const [message, setMessage] = useState("")
    const [loading, setLoading] = useState(false)

    function togglePreset(preset) {
        setSelectedPresets(current =>
            current.includes(preset)
                ? current.filter(p => p !== preset)
                : [...current, preset]
        )
    }

    async function handleSubmit() {
        if (!customText.trim() && selectedPresets.length === 0) {
            setMessage("select a preset or write feedback")
            return
        }
        setLoading(true)
        setMessage("")
        try {
            await apiRequest("/api/admin/feedback", {
                method: "POST",
                body: JSON.stringify({
                    selectedPresets,
                    customFeedback: customText.slice(0, 200),
                    contactInfo: contactInfo.trim(),
                    pageUrl: location.pathname,
                    userId: null
                })
            })
            setSubmitted(true)
            setSelectedPresets([])
            setCustomText("")
            setContactInfo("")
        }
        catch (err) {
            setMessage(err.message || "submit failed")
        }
        finally {
            setLoading(false)
        }
    }

    if (submitted) {
        return (
            <div className={ROOT_CLASS}>
                <p className="feedbackBoxThanks">thank you for your feedback!</p>
            </div>
        )
    }

    return (
        <div className={ROOT_CLASS}>
            <h3 className="feedbackBoxTitle">feedback</h3>
            {pageName && <p className="feedbackBoxPage">{pageName}</p>}
            <div className="feedbackBoxPresets">
                {PRESETS.map(preset => (
                    <button
                        key={preset}
                        type="button"
                        className={`feedbackBoxPreset${selectedPresets.includes(preset) ? " active" : ""}`}
                        onClick={() => togglePreset(preset)}
                    >
                        {preset}
                    </button>
                ))}
            </div>
            <textarea
                className="feedbackBoxTextarea"
                placeholder="write feedback here (200 characters max)..."
                maxLength={200}
                value={customText}
                onChange={e => setCustomText(e.target.value)}
            />
            <input
                className="feedbackBoxContact"
                type="text"
                placeholder="contact info (optional - email, discord, etc)"
                value={contactInfo}
                onChange={e => setContactInfo(e.target.value)}
            />
            <button
                className="feedbackBoxSubmit"
                onClick={handleSubmit}
                disabled={loading}
            >
                {loading ? "sending..." : "submit feedback"}
            </button>
            {message && <p className="feedbackBoxMessage">{message}</p>}
        </div>
    )
}
