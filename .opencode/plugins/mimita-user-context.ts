import { Plugin } from "@opencode/plugin"
import { spawn } from "node:child_process"
import { createHash } from "node:crypto"

const DEFAULT_AGENT = "opencode"

export default Plugin.define({
  id: "mimita.user-context",
  async setup(ctx) {
    const root = ctx.location.directory
    const script = `${root}/devscripts/agent-coord.py`
    const agent = process.env.MIMITA_AGENT_ID || DEFAULT_AGENT
    const priority = process.env.MIMITA_USER_CONTEXT_PRIORITY || "normal"

    const registration = await ctx.session.hook("prompt", (event) => {
      const text = String(event.prompt?.text || "").trim()
      if (!text) return
      const session = String(event.sessionID || "")
      const digest = createHash("sha256").update(text).digest("hex")
      const sourceRef = `${session}:${digest}`
      const child = spawn(
        process.env.MIMITA_PYTHON || "python",
        [
          script,
          "--agent",
          agent,
          "context",
          "add",
          text,
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
    })

    return async () => {
      await registration.dispose()
    }
  },
})
