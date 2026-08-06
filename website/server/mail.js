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

export async function sendMail(message) {
    if (!mailEnabled()) {
        console.log("[AUTH] mail=skipped smtp_not_configured")
        return
    }

    return await transporter.sendMail({
        from: fromAddress,
        ...message
    })
}

export async function sendAccountWelcomeEmail(email, username, verificationToken) {
    const origin = process.env.APP_ORIGIN || "https://mimita.fun"
    const verifyUrl = `${origin}/api/auth/verify-email/${verificationToken}`
    await sendMail({
        to: email,
        subject: "Welcome to Mimita — verify your email",
        text: `Welcome to Mimita, ${username}. Verify your email: ${verifyUrl}`,
        html: `
            <h1>Welcome to Mimita</h1>
            <p>Your account is ready, ${escapeHtml(username)}.</p>
            <p>Verify your email address:</p>
            <p><a href="${escapeHtml(verifyUrl)}" style="display:inline-block;padding:12px 24px;background:#40e0d0;color:#000;text-decoration:none;border-radius:4px">Verify Email</a></p>
            <p>Or copy this link: ${escapeHtml(verifyUrl)}</p>
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

export async function sendPasswordResetCodeEmail(email, code) {
    await sendMail({
        to: email,
        subject: "Mimita password reset code",
        text: `Your Mimita password reset code is ${code}. It expires in 10 minutes.`,
        html: `
            <h1>Password reset request</h1>
            <p>Your reset code is:</p>
            <p style="font-size:32px;font-weight:bold;letter-spacing:6px">
                ${code}
            </p>
            <p>This code expires in 10 minutes.</p>
            <p>If you did not request this, you can safely ignore this email.</p>
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

export async function sendDataDeletionRequestEmail(request) {
    const requestedAt = request.requestedAt || new Date().toISOString()
    const accountId = request.accountId || "not provided"
    const username = request.username || "not provided"
    const email = request.email || "not provided"
    const anonymousId = request.anonymousId || "not provided"

    await sendMail({
        to: "hello@mimita.fun",
        subject: "Data Deletion Request",
        text:
            `Data deletion request\n\n` +
            `Account id: ${accountId}\n` +
            `Username: ${username}\n` +
            `Email: ${email}\n` +
            `Anonymous analytics id: ${anonymousId}\n` +
            `Timestamp: ${requestedAt}\n`,
        html: `
            <h1>Data Deletion Request</h1>
            <p><strong>Account id:</strong> ${escapeHtml(accountId)}</p>
            <p><strong>Username:</strong> ${escapeHtml(username)}</p>
            <p><strong>Email:</strong> ${escapeHtml(email)}</p>
            <p><strong>Anonymous analytics id:</strong> ${escapeHtml(anonymousId)}</p>
            <p><strong>Timestamp:</strong> ${escapeHtml(requestedAt)}</p>
        `
    })
}

export async function sendSupportNotificationEmail({ requestId, topic, username, email, subject, message, bannerOrderId, createdAt }) {
    const origin = process.env.APP_ORIGIN || "https://mimita.fun"
    const adminUrl = `${origin}/admin/support`
    const text =
        `New support request #${requestId}\n\n` +
        `Topic: ${topic}\n` +
        `Sender: ${username}\n` +
        `Email: ${email}\n` +
        `Subject: ${subject}\n` +
        `Message: ${message}\n` +
        `Related banner order: ${bannerOrderId || "none"}\n` +
        `Timestamp: ${createdAt || new Date().toISOString()}\n` +
        `Admin: ${adminUrl}\n`

    await sendMail({
        to: "hello@mimita.fun",
        subject: `Support request #${requestId}: ${subject.slice(0, 80)}`,
        text,
        html: `
            <h1>New support request</h1>
            <p><strong>Request id:</strong> ${escapeHtml(String(requestId))}</p>
            <p><strong>Topic:</strong> ${escapeHtml(topic)}</p>
            <p><strong>Sender:</strong> ${escapeHtml(username)}</p>
            <p><strong>Email:</strong> ${escapeHtml(email)}</p>
            <p><strong>Subject:</strong> ${escapeHtml(subject)}</p>
            <p><strong>Message:</strong> ${escapeHtml(message)}</p>
            <p><strong>Related banner order:</strong> ${bannerOrderId ? escapeHtml(String(bannerOrderId)) : "none"}</p>
            <p><strong>Timestamp:</strong> ${escapeHtml(createdAt || new Date().toISOString())}</p>
            <p><a href="${escapeHtml(adminUrl)}">Open the admin support dashboard</a></p>
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

const VIP_TIER_LABELS = {
    vip: "VIP",
    super_vip: "Super VIP",
    ultra_vip: "Ultra VIP"
}

const VIP_PURCHASE_LABELS = {
    one_month: "1 month",
    twelve_month: "12 months",
    monthly_subscription: "monthly subscription"
}

export async function sendVipPurchaseEmail({
    email,
    username = "",
    tier = "vip",
    purchaseType = "one_month",
    amountCents = 0,
    currency = "usd",
    orderId = "",
    paidAt = new Date()
} = {}) {
    const tierLabel = VIP_TIER_LABELS[tier] || tier
    const purchaseLabel = VIP_PURCHASE_LABELS[purchaseType] || purchaseType
    const amount = `$${(Number(amountCents) / 100).toFixed(2)} ${String(currency).toUpperCase()}`
    const time = new Date(paidAt).toLocaleString("en-US", {
        year: "numeric",
        month: "2-digit",
        day: "2-digit",
        hour: "2-digit",
        minute: "2-digit",
        second: "2-digit"
    })
    const origin = process.env.APP_ORIGIN || "https://mimita.fun"
    const vipUrl = `${origin}/vip`

    const text =
        `Your MiMITA VIP is active!\n\n` +
        `Hi ${username},\n` +
        `You now have ${tierLabel} VIP.\n\n` +
        `Tier: ${tierLabel}\n` +
        `Purchase: ${purchaseLabel}\n` +
        `Amount paid: ${amount}\n` +
        `Date/time: ${time}\n` +
        `Order id: ${orderId}\n\n` +
        `Customize your name color here: ${vipUrl}\n` +
        `Thanks for supporting MiMITA!`

    await sendMail({
        to: email,
        subject: "Your MiMITA VIP is active!",
        text,
        html: `
            <h1>Your MiMITA VIP is active!</h1>
            <p>Hi ${escapeHtml(username)}, you now have <strong>${escapeHtml(tierLabel)}</strong> VIP.</p>
            <table style="border-collapse:collapse">
                <tr><td style="padding:4px 12px 4px 0"><strong>Tier</strong></td><td style="padding:4px 0">${escapeHtml(tierLabel)}</td></tr>
                <tr><td style="padding:4px 12px 4px 0"><strong>Purchase</strong></td><td style="padding:4px 0">${escapeHtml(purchaseLabel)}</td></tr>
                <tr><td style="padding:4px 12px 4px 0"><strong>Amount paid</strong></td><td style="padding:4px 0">${escapeHtml(amount)}</td></tr>
                <tr><td style="padding:4px 12px 4px 0"><strong>Date/time</strong></td><td style="padding:4px 0">${escapeHtml(time)}</td></tr>
                <tr><td style="padding:4px 12px 4px 0"><strong>Order id</strong></td><td style="padding:4px 0">${escapeHtml(String(orderId))}</td></tr>
            </table>
            <p><a href="${escapeHtml(vipUrl)}" style="display:inline-block;padding:12px 24px;background:#40e0d0;color:#000;text-decoration:none;border-radius:4px">Customize your name color</a></p>
            <p>Thanks for supporting MiMITA!</p>
        `
    })
}
