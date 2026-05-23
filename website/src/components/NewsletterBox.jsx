import { useState } from "react"

export default function NewsletterBox() {

    const [email, setEmail] = useState("")

    const [message, setMessage] = useState(
        "[SYSTEM] waiting for input"
    )

    const [loading, setLoading] = useState(false)

    function getTime() {

        return new Date().toLocaleTimeString(
            [],
            {
                hour12: false
            }
        )
    }

    async function submitEmail() {

        if (!email) {

            setMessage(
                `[${getTime()}] [ERROR] email required`
            )

            return
        }

        try {

            setLoading(true)

            setMessage(
                `[${getTime()}] [STATUS] signing up...`
            )

            const response = await fetch(
                "http://localhost:3002/api/newsletter",
                {
                    method: "POST",

                    headers: {
                        "Content-Type": "application/json"
                    },

                    body: JSON.stringify({
                        email
                    })
                }
            )

            const data = await response.json()

            if (response.status === 429) {

                setMessage(
                    `[${getTime()}] [ERROR] too many requests`
                )
            }

            else if (data.alreadySubscribed) {

                setMessage(
                    `[${getTime()}] [INFO] email already signed up`
                )
            }

            else if (data.success) {

                setMessage(
                    `[${getTime()}] [SUCCESS] joined newsletter`
                )

                setEmail("")
            }

            else {

                setMessage(
                    `[${getTime()}] [ERROR] signup failed`
                )
            }

        } catch (err) {

            console.log(err)

            setMessage(
                `[${getTime()}] [ERROR] server offline`
            )

        } finally {

            setLoading(false)
        }
    }

    return (

        <div className="newsletterBox">

            <input
                type="email"
                placeholder="email"
                value={email}
                disabled={loading}
                onChange={(e) => setEmail(e.target.value)}
                className="newsletterInput"
            />

            <button
                onClick={submitEmail}
                disabled={loading}
                className="newsletterButton"
            >
                {
                    loading
                    ? "SIGNING UP..."
                    : "JOIN NEWSLETTER"
                }
            </button>

            <p className="newsletterMessage">
                {message}
            </p>

        </div>
    )
}