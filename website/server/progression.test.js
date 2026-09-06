// 09 06 2026, 16 00
/* purpose
 * Exercise real PostgreSQL SQL and rollback semantics in isolated PGlite memory.
 * Verify migrations preserve historical data and host saves cannot bypass membership.
 * Check precision, retries, revision conflicts, read contracts and additive sessions.
 * DOES NOT use DATABASE_URL, production records or a live game executable.
 * DOES NOT claim multi-connection PostgreSQL lock scheduling is emulated.
 * DOES NOT write persistent test databases.
 */
import test from "node:test"
import assert from "node:assert/strict"
import express from "express"
import request from "supertest"
import { PGlite } from "@electric-sql/pglite"
import { MIGRATION_STATEMENTS, runMigrations, pool } from "./db.js"
import { createProgressionRouter, progressionInteger } from "./progression-routes.js"
import { hashToken } from "./authCore.js"
import { sessionSecret } from "./session.js"
import gameApi from "./game-api.js"
import { getVipStateForUser } from "./vip-entitlements.js"

async function isolatedDatabase() {
    const pg = new PGlite()
    let tail = Promise.resolve()
    const query = async (sql, params) => {
        const result = params ? await pg.query(sql, params) : (await pg.exec(sql)).at(-1)
        return { ...result, rowCount: result.rows.length || result.affectedRows || 0 }
    }
    const database = { query, async connect() {
        const previous = tail
        let release
        tail = new Promise(resolve => { release = resolve })
        await previous
        return { query, release }
    } }
    return { pg, database }
}

const counters = (overrides = {}) => ({ gold: "0", totalXp: "0", playerKills: "0", npcKills: "0", deaths: "0", playtimeTicks: "0", ...overrides })

test("integer validation never rounds or accepts coercible invalid values", () => {
    assert.equal(progressionInteger("9223372036854775807"), "9223372036854775807")
    assert.equal(progressionInteger(123), "123")
    for (const value of [-1, 1.2, NaN, Infinity, 9007199254740992, "9223372036854775808", "01", "1e3", "", null, true, {}, "-1"]) {
        assert.throws(() => progressionInteger(value), /invalid_integer/)
    }
})

test("fresh schema, safe adoption of 83 accounts, backfill, constraints and future signup", async () => {
    const { pg, database } = await isolatedDatabase()
    try {
        // Reconstruct the tracked historical bootstrap, not a hand-invented schema.
        for (const sql of MIGRATION_STATEMENTS) await database.query(sql)
        await database.query(`INSERT INTO users(username,username_key,email,password_hash)
            SELECT 'user' || i, 'user' || i, 'user' || i || '@test.invalid', 'fake' FROM generate_series(1,83) i`)
        await database.query("INSERT INTO game_stats(user_id,gold,total_xp,playtime_seconds,lifetime_player_kills) VALUES(1,123,456,91,7)")
        const usersBefore = (await database.query("SELECT * FROM users ORDER BY id")).rows
        await runMigrations(database)
        assert.deepEqual((await database.query("SELECT * FROM users ORDER BY id")).rows, usersBefore)
        assert.equal((await database.query("SELECT count(*)::int AS n FROM game_stats")).rows[0].n, 83)
        const stats = (await database.query("SELECT * FROM game_stats WHERE user_id=1")).rows[0]
        assert.equal(String(stats.gold), "123")
        assert.equal(String(stats.total_xp), "456")
        assert.equal(String(stats.lifetime_player_kills), "7")
        assert.equal(String(stats.playtime_ticks), "5460")
        await runMigrations(database)
        assert.deepEqual((await database.query("SELECT * FROM game_stats WHERE user_id=1")).rows[0], stats)
        assert.deepEqual((await database.query("SELECT version FROM schema_migrations ORDER BY version")).rows.map(r => r.version), [1, 5])
        await assert.rejects(database.query("UPDATE game_stats SET gold=-1 WHERE user_id=1"))
        await assert.rejects(database.query("INSERT INTO game_stats(user_id) VALUES(1)"))
        await database.query("INSERT INTO users(username,username_key,email,password_hash) VALUES('new','new','new@test.invalid','fake')")
        assert.equal((await database.query("SELECT count(*)::int AS n FROM game_stats")).rows[0].n, 84)
    } finally { await pg.close() }
})

test("fresh versioned bootstrap and progression HTTP transactions", async t => {
    const { pg, database } = await isolatedDatabase()
    const originalQuery = pool.query
    const originalConnect = pool.connect
    try {
        await runMigrations(database)
        await database.query(`INSERT INTO users(username,username_key,email,password_hash)
            SELECT 'user' || i, 'user' || i, 'user' || i || '@test.invalid', 'fake' FROM generate_series(1,4) i`)
        const app = express()
        app.use(express.json())
        app.use("/api/progression", createProgressionRouter({ database,
            authenticateMw: (req, res, next) => { req.user = { id: req.headers.authorization.slice(7) }; next() }, log() {} }))
        app.use((error, req, res, next) => { void next; res.status(500).json({ success: false }) })
        const post = (path, body, host = "1") => request(app).post(`/api/progression/${path}`).set("Authorization", `Bearer ${host}`).send(body)
        const register = (sessionId, players, host = "1", roomCode = "ROOM") => post("session", { sessionId, roomCode, players }, host)
        const batch = (batchId, players, sessionId = "session-a", host = "1") => post("batch", { sessionId, batchId, players }, host)
        const row = (revision, changes = {}, userId = "1") => ({ userId, revision, ...counters(changes) })
        const totals = async (id = 1) => (await database.query("SELECT * FROM game_stats WHERE user_id=$1", [id])).rows[0]

        await t.test("bulk VIP uses four queries and recomputes expiration at request time", async () => {
            await database.query(`INSERT INTO vip_entitlements(user_id,tier,source,status,starts_at,expires_at)
                VALUES(1,'vip','stripe','active','2026-08-01T00:00:00Z','2026-09-01T00:00:00Z'),
                      (2,'super_vip','stripe','active','2026-08-01T00:00:00Z','2026-10-01T00:00:00Z')`)
            let count = 0
            const query = (...args) => { count++; return database.query(...args) }
            const users = [{ id: "1", role: "user" }, { id: "2", role: "user" }]
            const states = await getVipStateForUser(users, query, new Date("2026-09-06T12:00:00Z"))
            assert.equal(count, 4)
            assert.deepEqual(states.map(state => state.active_tier), ["free", "super_vip"])
            const expired = await getVipStateForUser(users, query, new Date("2026-10-01T00:00:00Z"))
            assert.deepEqual(expired.map(state => state.active_tier), ["free", "free"])
        })

        await t.test("bearer required and self registration loads existing global totals", async () => {
            assert.equal((await request(app).post("/api/progression/session").send({})).status, 401)
            await database.query("UPDATE game_stats SET gold=500 WHERE user_id=1")
            const response = await register("session-a", [{ userId: "1" }])
            assert.equal(response.status, 200)
            assert.equal(response.body.players[0].gold, "500")
            assert.equal(response.body.players[0].cumulative.gold, "0")
            assert.equal((await register("session-a", [], "2")).status, 403)
            assert.equal((await register("session-a", [], "1", "OTHER")).status, 409)
        })
        await t.test("remote ticket checks identity, room, expiry and durable unique membership", async () => {
            const ticket = "isolated-join-ticket"
            await database.query(`INSERT INTO vip_join_tickets(token_hash,user_id,room_code,expires_at,used_at,last_result)
                VALUES($1,2,'ROOM',NOW()+INTERVAL '5 minutes',NOW(),'{"account_id":"2"}')`, [hashToken(ticket, sessionSecret)])
            assert.equal((await register("session-a", [{ userId: "2" }])).status, 403)
            assert.equal((await register("session-a", [{ userId: "3", joinTicket: ticket }])).status, 403)
            assert.equal((await register("wrong-room", [{ userId: "2", joinTicket: ticket }], "1", "OTHER")).status, 403)
            assert.equal((await register("session-a", [{ userId: "2", joinTicket: ticket }])).status, 200)
            assert.equal((await register("session-b", [{ userId: "2", joinTicket: ticket }], "3")).status, 409)
            await database.query("UPDATE vip_join_tickets SET expires_at=NOW()-INTERVAL '1 second'")
            assert.equal((await register("session-a", [{ userId: "2" }])).status, 200)
            assert.equal((await register("expired", [{ userId: "2", joinTicket: ticket }])).status, 403)
        })
        await t.test("whole batch rollback when later player fails membership", async () => {
            const response = await batch("invalid", [row("1", { gold: "50" }), row("1", { deaths: "1" }, "4")])
            assert.equal(response.status, 403)
            assert.equal(String((await totals()).gold), "500")
            assert.equal((await database.query("SELECT count(*)::int n FROM processed_events")).rows[0].n, 0)
        })
        await t.test("lost confirmation retry, immutable batch conflict and same revision comparison", async () => {
            const players = [row("1", { gold: "50", totalXp: "110", playerKills: "1", npcKills: "1", playtimeTicks: "3600" }), row("1", { deaths: "1" }, "2")]
            const first = await batch("first", players)
            assert.equal(first.status, 200)
            assert.ok(first.body.confirmedAt)
            assert.equal(first.body.players[0].gold, "550")
            assert.deepEqual((await batch("first", players)).body, first.body)
            assert.equal((await batch("first", [row("1", { gold: "51" })])).status, 409)
            assert.equal((await batch("revision-conflict", [row("1", { gold: "50" })])).status, 409)
            assert.equal((await batch("equivalent", players)).status, 200)
            assert.equal(String((await totals()).gold), "550")
            assert.equal(String((await totals()).lifetime_npc_kills), "1")
            assert.equal(String((await totals()).lifetime_player_kills), "1")
        })
        await t.test("higher cumulative revisions, stale delivery and session restart recovery", async () => {
            const next = row("3", { gold: "100", totalXp: "210", playerKills: "2", npcKills: "1", playtimeTicks: "7200" })
            assert.equal((await batch("third", [next])).status, 200)
            assert.equal((await batch("late-second", [row("2", { gold: "60", totalXp: "120", playerKills: "1", npcKills: "1", playtimeTicks: "4000" })])).status, 409)
            assert.equal((await batch("stale-increase", [row("2", { gold: "101" })])).status, 409)
            assert.equal((await batch("decrease", [row("4")])).status, 409)
            const resume = await register("session-a", [{ userId: "1" }])
            assert.equal(resume.body.players[0].revision, "3")
            assert.equal(resume.body.players[0].gold, "600")
            assert.equal(resume.body.players[0].cumulative.gold, "100")
        })
        await t.test("two host sessions add independently without replacing prior gains", async () => {
            await register("second-host", [{ userId: "1" }])
            const responses = await Promise.all([
                batch("host-b-gain", [row("1", { gold: "80" })], "second-host"),
                batch("host-a-gain", [row("4", { gold: "150", totalXp: "210", playerKills: "2", npcKills: "1", playtimeTicks: "7200" })])
            ])
            assert.deepEqual(responses.map(r => r.status), [200, 200])
            assert.equal(String((await totals()).gold), "730")
        })
        await t.test("unsafe integers, duplicates and overflow reject without partial totals", async () => {
            assert.equal((await batch("unsafe", [row("5", { gold: 9007199254740992 })])).status, 400)
            assert.equal((await batch("duplicates", [row("5"), row("5")])).status, 400)
            const response = await batch("overflow", [row("5", { gold: "9223372036854775807", totalXp: "210", playerKills: "2", npcKills: "1", playtimeTicks: "7200" })])
            assert.equal(response.status, 500)
            assert.deepEqual(response.body, { success: false, error: "progression_save_failed" })
            assert.equal(String((await totals()).gold), "730")
        })
        await t.test("public profiles and five boards expose exact totals; old write routes retired", async () => {
            // Explicitly replace the real pool before mounting the real HTTP owner.
            pool.query = database.query
            pool.connect = database.connect
            const publicApp = express()
            publicApp.use(express.json())
            publicApp.use("/api", gameApi)
            publicApp.use((error, req, res, next) => { void next; res.status(500).json({ error: error.message }) })
            await database.query("UPDATE game_stats SET total_xp=9007199254740993 WHERE user_id IN (1,2)")
            const profile = await request(publicApp).get("/api/profile/1")
            assert.equal(profile.status, 200, JSON.stringify(profile.body))
            assert.equal(profile.body.profile.totalXp, "9007199254740993")
            assert.equal(profile.body.profile.playtimeTicks, "7200")
            for (const type of ["xp", "gold", "playtime", "kills", "deaths"]) {
                const response = await request(publicApp).get(`/api/leaderboard?type=${type}`)
                assert.equal(response.status, 200, JSON.stringify(response.body))
                assert.ok(Object.hasOwn(response.body.leaderboard[0], "lifetime_player_kills"))
                if (type === "xp") assert.deepEqual(response.body.leaderboard.slice(0, 2).map(r => String(r.id)), ["1", "2"])
            }
            await database.query("INSERT INTO sessions(user_id,token_hash,expires_at) VALUES(1,$1,NOW()+INTERVAL '1 hour')", [hashToken("test-token", sessionSecret)])
            for (const path of ["/api/stats", "/api/stats/ingest"]) {
                assert.equal((await request(publicApp).post(path).set("Authorization", "Bearer test-token").send({ gold: 999999 })).status, 410)
            }
            assert.equal((await request(publicApp).post("/api/progression/batch").set("Authorization", "Bearer invalid").send({})).status, 401)
            await database.query("INSERT INTO sessions(user_id,token_hash,expires_at) VALUES(2,$1,NOW()+INTERVAL '1 hour')", [hashToken("other-token", sessionSecret)])
            const mixed = await request(publicApp).post("/api/progression/session")
                .set("Authorization", "Bearer test-token").set("Cookie", "mimita_session=other-token")
                .send({ sessionId: "bearer-wins", roomCode: "ROOM", players: [{ userId: "1" }] })
            assert.equal(mixed.status, 200)
            assert.equal(String((await database.query("SELECT host_user_id FROM progression_sessions WHERE session_id='bearer-wins'")).rows[0].host_user_id), "1")
        })
    } finally {
        pool.query = originalQuery
        pool.connect = originalConnect
        await pg.close()
    }
})

test("unsafe historical data rolls migrations back and partial tick columns retain existing values", async () => {
    const { pg, database } = await isolatedDatabase()
    try {
        for (const sql of MIGRATION_STATEMENTS) await database.query(sql)
        await database.query(`INSERT INTO users(username,username_key,email,password_hash)
            VALUES('old','old','old@test.invalid','fake'),('other','other','other@test.invalid','fake')`)
        await database.query("ALTER TABLE game_stats ADD COLUMN playtime_ticks BIGINT")
        await database.query("INSERT INTO game_stats(user_id,gold,playtime_seconds,playtime_ticks) VALUES(1,-1,90,77),(2,0,3,NULL)")
        await assert.rejects(runMigrations(database))
        assert.equal((await database.query("SELECT to_regclass('schema_migrations') AS table_name")).rows[0].table_name, null)
        assert.equal((await database.query("SELECT to_regclass('progression_sessions') AS table_name")).rows[0].table_name, null)
        assert.equal(String((await database.query("SELECT gold FROM game_stats WHERE user_id=1")).rows[0].gold), "-1")
        await database.query("UPDATE game_stats SET gold=0 WHERE user_id=1")
        await runMigrations(database)
        assert.deepEqual((await database.query("SELECT playtime_ticks FROM game_stats ORDER BY user_id")).rows.map(row => String(row.playtime_ticks)), ["77", "180"])
    } finally { await pg.close() }
})
