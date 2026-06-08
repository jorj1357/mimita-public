import fs from "fs"
import path from "path"
import nodemailer from "nodemailer"
import { fileURLToPath } from "url"

const __filename = fileURLToPath(import.meta.url)
const __dirname = path.dirname(__filename)

const transporter = nodemailer.createTransport({
    host: process.env.SMTP_HOST,
    port: Number(process.env.SMTP_PORT || 587),
    secure: process.env.SMTP_SECURE === "true",
    auth: process.env.SMTP_USER
        ? {
            user: process.env.SMTP_USER,
            pass: process.env.SMTP_PASS
        }
        : undefined
})

const fromAddress =
    process.env.MAIL_FROM || '"Mimita" <hello@mimita.fun>'

function mailEnabled() {
    return Boolean(process.env.SMTP_HOST)
}

async function sendMail(message) {
    if (!mailEnabled()) {
        console.log("[AUTH] mail=skipped smtp_not_configured")
        return
    }

    await transporter.sendMail({
        from: fromAddress,
        ...message
    })
}

export async function sendAccountWelcomeEmail(email, username) {
    await sendMail({
        to: email,
        subject: "Welcome to Mimita",
        text: `Welcome to Mimita, ${username}.`,
        html: `
            <h1>Welcome to Mimita</h1>
            <p>Your account is ready, ${escapeHtml(username)}.</p>
            <p><a href="https://mimita.fun">Visit Mimita</a></p>
        `
    })
}

export async function sendNewsletterWelcomeEmail(email) {
    const htmlPath = path.join(
        __dirname,
        "emailTemplates",
        "welcome.html"
    )

    await sendMail({
        to: email,
        subject: "Welcome to Mimita",
        html: fs.readFileSync(htmlPath, "utf8")
    })
}

export async function sendPasswordChangeCodeEmail(email, code) {
    await sendMail({
        to: email,
        subject: "Mimita password change code",
        text: `Your Mimita password change code is ${code}. It expires in 10 minutes.`,
        html: `
            <h1>Password change request</h1>
            <p>Your verification code is:</p>
            <p style="font-size:32px;font-weight:bold;letter-spacing:6px">
                ${code}
            </p>
            <p>This code expires in 10 minutes.</p>
        `
    })
}

export async function sendPasswordChangedEmail(
    email,
    changedAt,
    browser,
    ipAddress
) {
    await sendMail({
        to: email,
        subject: "Your Mimita password was changed",
        text:
            `Your password was changed at ${changedAt} ` +
            `from ${browser} / ${ipAddress}.`,
        html: `
            <h1>Your password was changed</h1>
            <p>Time: ${escapeHtml(changedAt)}</p>
            <p>Browser: ${escapeHtml(browser)}</p>
            <p>IP: ${escapeHtml(ipAddress)}</p>
        `
    })
}

function escapeHtml(value) {
    return String(value)
        .replaceAll("&", "&amp;")
        .replaceAll("<", "&lt;")
        .replaceAll(">", "&gt;")
        .replaceAll('"', "&quot;")
        .replaceAll("'", "&#039;")
}
