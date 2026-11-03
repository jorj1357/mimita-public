import { Router } from "express"
import { readFileSync, statfsSync } from "node:fs"

const WARNING_USED_PERCENT = 80
const CRITICAL_USED_PERCENT = 90
const MIN_READY_FREE_BYTES = 5 * 1024 * 1024 * 1024
const BACKUP_STATUS_PATH = "/var/lib/mimita-database-backup/status"
const MAX_BACKUP_AGE_SECONDS = 26 * 60 * 60

export function hasFreshVerifiedBackup(statusText, nowMs = Date.now()) {
    const match = /^(success|failure) ([0-9]+)$/.exec(statusText.trim())
    if (!match || match[1] !== "success") return false
    const timestamp = Number(match[2])
    const nowSeconds = Math.floor(nowMs / 1000)
    return Number.isSafeInteger(timestamp) && timestamp <= nowSeconds &&
        nowSeconds - timestamp <= MAX_BACKUP_AGE_SECONDS
}

function readStorage(rootPath, statfs) {
    const stats = statfs(rootPath)
    const totalBytes = stats.blocks * stats.bsize
    const availableBytes = stats.bavail * stats.bsize
    if (!Number.isFinite(totalBytes) || totalBytes <= 0 || !Number.isFinite(availableBytes)) {
        throw new Error("storage-status-unavailable")
    }
    return {
        usedPercent: 100 * (1 - availableBytes / totalBytes),
        availableBytes
    }
}

function status(res, available) {
    return res.status(available ? 200 : 503).json({
        status: available ? "ok" : "unavailable"
    })
}

export function createHealthRouter({
    pool,
    rootPath = "/",
    statfs = statfsSync,
    backupStatusPath = BACKUP_STATUS_PATH,
    now = Date.now
} = {}) {
    const router = Router()

    router.get("/ready", async (_req, res) => {
        try {
            await pool.query({ text: "SELECT 1", query_timeout: 3000 })
            const storage = readStorage(rootPath, statfs)
            return status(res,
                storage.availableBytes >= MIN_READY_FREE_BYTES &&
                storage.usedPercent < WARNING_USED_PERCENT)
        } catch {
            return status(res, false)
        }
    })

    router.get("/storage-warning", (_req, res) => {
        try {
            return status(res,
                readStorage(rootPath, statfs).usedPercent < WARNING_USED_PERCENT)
        } catch {
            return status(res, false)
        }
    })

    router.get("/storage-critical", (_req, res) => {
        try {
            return status(res,
                readStorage(rootPath, statfs).usedPercent < CRITICAL_USED_PERCENT)
        } catch {
            return status(res, false)
        }
    })

    router.get("/backup", (_req, res) => {
        try {
            return status(res, hasFreshVerifiedBackup(
                readFileSync(backupStatusPath, "utf8"), now()))
        } catch {
            return status(res, false)
        }
    })

    return router
}
