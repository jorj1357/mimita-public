import { Router } from "express"
import { statfsSync } from "node:fs"

const WARNING_USED_PERCENT = 80
const CRITICAL_USED_PERCENT = 90
const MIN_READY_FREE_BYTES = 5 * 1024 * 1024 * 1024

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

export function createHealthRouter({ pool, rootPath = "/", statfs = statfsSync } = {}) {
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

    return router
}
