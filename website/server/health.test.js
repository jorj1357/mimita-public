import test from "node:test"
import assert from "node:assert/strict"
import { mkdtempSync, rmSync, writeFileSync } from "node:fs"
import { tmpdir } from "node:os"
import { join } from "node:path"
import express from "express"
import request from "supertest"
import { createHealthRouter, hasFreshVerifiedBackup } from "./health.js"

function createApp({
    dbOk = true,
    usedPercent = 10,
    storageAvailable = true,
    backupStatusPath,
    now
} = {}) {
    const app = express()
    const pool = {
        query: async () => {
            if (!dbOk) throw Object.assign(new Error("private database detail"), {
                code: "ECONNREFUSED"
            })
            return { rows: [{ "?column?": 1 }] }
        }
    }
    const statfs = () => {
        if (!storageAvailable) throw new Error("private filesystem detail")
        const blocks = 10_000
        const bsize = 1024 * 1024
        return {
            blocks,
            bsize,
            bavail: Math.floor(blocks * (1 - usedPercent / 100))
        }
    }
    app.use("/api/health", createHealthRouter({ pool, statfs, backupStatusPath, now }))
    return app
}

test("readiness is healthy when PostgreSQL and disk headroom are healthy", async () => {
    const response = await request(createApp()).get("/api/health/ready")
    assert.equal(response.status, 200)
    assert.deepEqual(response.body, { status: "ok" })
})

test("readiness is unavailable when the database is down without exposing details", async () => {
    const response = await request(createApp({ dbOk: false })).get("/api/health/ready")
    assert.equal(response.status, 503)
    assert.deepEqual(response.body, { status: "unavailable" })
})

test("readiness fails below the free-space floor or warning threshold", async () => {
    assert.equal((await request(createApp({ usedPercent: 80 })).get("/api/health/ready")).status, 503)
    assert.equal((await request(createApp({ usedPercent: 77 })).get("/api/health/ready")).status, 503)
})

test("storage endpoints distinguish warning and critical thresholds", async () => {
    const app = createApp({ usedPercent: 85 })
    assert.equal((await request(app).get("/api/health/storage-warning")).status, 503)
    assert.equal((await request(app).get("/api/health/storage-critical")).status, 200)
    const critical = createApp({ usedPercent: 95 })
    assert.equal((await request(critical).get("/api/health/storage-critical")).status, 503)
})

test("storage check failures are reported without exposing filesystem errors", async () => {
    const response = await request(createApp({ storageAvailable: false }))
        .get("/api/health/storage-warning")
    assert.equal(response.status, 503)
    assert.deepEqual(response.body, { status: "unavailable" })
})

test("backup status accepts only a recent successful verified upload", () => {
    const now = Date.UTC(2026, 10, 2, 15, 0, 0)
    const current = Math.floor(now / 1000)
    assert.equal(hasFreshVerifiedBackup(`success ${current}`, now), true)
    assert.equal(hasFreshVerifiedBackup(`success ${current - 26 * 60 * 60}`, now), true)
    assert.equal(hasFreshVerifiedBackup(`success ${current - 26 * 60 * 60 - 1}`, now), false)
    assert.equal(hasFreshVerifiedBackup(`failure ${current}`, now), false)
    assert.equal(hasFreshVerifiedBackup(`success ${current + 1}`, now), false)
    assert.equal(hasFreshVerifiedBackup("private backup detail", now), false)
})

test("backup health endpoint exposes only healthy or unavailable", async () => {
    const dir = mkdtempSync(join(tmpdir(), "mimita-backup-health-"))
    const statusPath = join(dir, "status")
    const now = Date.UTC(2026, 10, 2, 15, 0, 0)
    try {
        writeFileSync(statusPath, `success ${Math.floor(now / 1000)}\n`)
        const healthy = await request(createApp({ backupStatusPath: statusPath, now: () => now }))
            .get("/api/health/backup")
        assert.equal(healthy.status, 200)
        assert.deepEqual(healthy.body, { status: "ok" })

        writeFileSync(statusPath, `failure ${Math.floor(now / 1000)}\n`)
        const failed = await request(createApp({ backupStatusPath: statusPath, now: () => now }))
            .get("/api/health/backup")
        assert.equal(failed.status, 503)
        assert.deepEqual(failed.body, { status: "unavailable" })
    } finally {
        rmSync(dir, { recursive: true, force: true })
    }
})

test("backup health is unavailable when its status file is missing", async () => {
    const response = await request(createApp({ backupStatusPath: "missing-backup-status" }))
        .get("/api/health/backup")
    assert.equal(response.status, 503)
    assert.deepEqual(response.body, { status: "unavailable" })
})
