import { spawn } from "node:child_process"
import { createHash } from "node:crypto"

const DEFAULT_AGENT = "opencode"

function textFromParts(parts) {
  return (parts || [])
    .filter((part) => part?.type === "text" && typeof part.text === "string")
    .map((part) => part.text)
    .join("\n")
    .trim()
}

function relay(directory, sessionID, messageID, text) {
  const value = String(text || "").trim()
  if (!value) return
  const root = String(directory || process.cwd())
  const script = `${root}/devscripts/agent-coord.py`
  const agent = process.env.MIMITA_AGENT_ID || DEFAULT_AGENT
  const priority = process.env.MIMITA_USER_CONTEXT_PRIORITY || "normal"
  const session = String(sessionID || "")
  const digest = createHash("sha256").update(value).digest("hex")
  const sourceRef = messageID
    ? `${session}:${messageID}`
    : `${session}:${digest}`
  const child = spawn(
    process.env.MIMITA_PYTHON || "python",
    [
      script,
      "--agent",
      agent,
      "context",
      "add",
      value,
      "--source",
      "user",
      "--session",
      session,
      "--source-ref",
      sourceRef,
      "--priority",
      priority,
    ],
    {
      cwd: root,
      detached: true,
      stdio: "ignore",
      windowsHide: true,
    },
  )
  child.on("error", (error) => {
    console.error("[mimita-user-context] relay failed:", error.message)
  })
  child.unref()
}

const plugin = {
  id: "mimita.user-context",
  async setup(ctx) {
    await ctx.session.hook("prompt", (event) => {
      relay(
        ctx.location.directory,
        event.sessionID,
        event.messageID,
        event.prompt?.text,
      )
    })
  },
  async server(input) {
    return {
      "chat.message": async (event, output) => {
        relay(
          input.directory,
          event.sessionID,
          event.messageID,
          textFromParts(output.parts),
        )
      },
    }
  },
}

export default plugin
