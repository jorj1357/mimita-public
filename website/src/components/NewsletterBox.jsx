import { useState } from "react"
import { apiRequestRaw } from "../lib/api.js"

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

        // stop spam clicking
        if (loading) {

            setMessage(
                `[${getTime()}] [ERROR] request already processing`
            )

            return
        }

        // normalize email
        const cleanedEmail =
            email
            .trim()
            .toLowerCase()

        // empty email
        if (!cleanedEmail) {

            setMessage(
                `[${getTime()}] [ERROR] email required`
            )

            return
        }

        // basic email validation
        if (
            !cleanedEmail.includes("@")
            ||
            !cleanedEmail.includes(".")
        ) {

            setMessage(
                `[${getTime()}] [ERROR] invalid email format`
            )

            return
        }

        try {

            setLoading(true)

            setMessage(
                `[${getTime()}] [STATUS] contacting MiMITA servers...`
            )

            const { response, data } = await apiRequestRaw(
                "/api/newsletter",
                {
                    method: "POST",
                    headers: {
                        "Content-Type": "application/json"
                    },
                    body: JSON.stringify({
                        email: cleanedEmail
                    })
                }
            )

            // too many requests
            if (response.status === 429) {

                setMessage(
                    `[${getTime()}] [ERROR] too many requests`
                )
            }

            // already signed up
            else if (data?.alreadySubscribed) {

                setMessage(
                    `[${getTime()}] [INFO] email already signed up`
                )
            }

            // success
            else if (data?.success) {

                setMessage(
                    `[${getTime()}] [SUCCESS] joined newsletter`
                )

                setEmail("")
            }

            // backend custom message
            else if (data?.message) {

                setMessage(
                    `[${getTime()}] [ERROR] ${data.message}`
                )
            }

            // unknown
            else {

                setMessage(
                    `[${getTime()}] [ERROR] unknown signup failure`
                )
            }

        }
        catch (err) {

            console.log(err)

            setMessage(
                `[${getTime()}] [ERROR] unable to contact MiMITA servers`
            )

        }
        finally {

            setLoading(false)
        }
    }

    return (

        <div className="newsletterBox">

            <input
                type="email"
                placeholder="enter email"
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