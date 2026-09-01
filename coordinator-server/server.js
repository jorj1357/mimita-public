// 09 01 2026, 00 00
/* purpose
* Owns public community-server room registration, discovery, and ICE signaling.
* Announces newly registered community servers once to the configured MiMITA Discord webhook.
* Keeps webhook credentials in the coordinator environment and never sends them to clients.
* Does NOT host game simulation, authenticate gameplay packets, or own client UI.
* Does NOT edit, delete, or send offline updates for Discord announcements.
*/

const http = require("http");
const https = require("https");
const crypto = require("crypto");

const fs = require("fs");
const path = require("path");

// Load env.sh for TURN secret if not already in environment
const envPath = "/root/mimita-coordinator/env.sh";
if (!process.env.MIMITA_TURN_SECRET && fs.existsSync(envPath)) {
    const envContent = fs.readFileSync(envPath, "utf8");
    const match = envContent.match(/^MIMITA_TURN_SECRET=(.+)$/m);
    if (match) {
        process.env.MIMITA_TURN_SECRET = match[1].trim();
    }
}
if (!process.env.MIMITA_DISCORD_WEBHOOK_URL && fs.existsSync(envPath)) {
    const envContent = fs.readFileSync(envPath, "utf8");
    const match = envContent.match(/^MIMITA_DISCORD_WEBHOOK_URL=(.+)$/m);
    if (match) process.env.MIMITA_DISCORD_WEBHOOK_URL = match[1].trim().replace(/^['\"]|['\"]$/g, "");
}

const PORT = process.env.COORDINATOR_PORT || 3001;
const ROOM_TIMEOUT_MS = 30000;
const REQUEST_TIMEOUT_MS = 60000;
const COMPLETED_REQUEST_RETENTION_MS = 60000;
const TURN_SHARED_SECRET = process.env.MIMITA_TURN_SECRET || "";

const rooms = new Map();

// ── Duels queue / matchmaking state ──────────────────────────────────
// queueTickets: ticket_id -> {profile_id, name, prefer_opponent, maps,
//   joined_at, last_poll, matched_match_id}
// matches: match_id -> {host_ticket_id, client_ticket_id, map,
//   host_room_code, state, created_at}
const queueTickets = new Map();
const matches = new Map();
const QUEUE_TICKET_TIMEOUT_MS = 60000;
const MATCH_CONFIRM_TIMEOUT_MS = 60000;
const DEFAULT_DUEL_MAPS = ["mimita-duels-map-v3", "atdm", "funworld"];

function pickDuelMap(aMaps, bMaps) {
    const a = Array.isArray(aMaps) ? aMaps : [];
    const b = Array.isArray(bMaps) ? bMaps : [];
    const pool = a.length > 0 && b.length > 0 ? a.filter(m => b.includes(m)) : [];
    const source = pool.length > 0 ? pool : (a.length > 0 ? a : (b.length > 0 ? b : DEFAULT_DUEL_MAPS));
    return source[Math.floor(Math.random() * source.length)];
}

// Pair waiting tickets into matches. Prefers prefer_opponent pairs, else FIFO.
function tryMatchQueue() {
    const waiting = [];
    for (const [ticketId, ticket] of queueTickets) {
        // Only matchable once the player's own duel server is up (has a code).
        if (!ticket.matched_match_id && ticket.room_code)
            waiting.push(ticketId);
    }
    while (waiting.length >= 2) {
        let aId = null;
        let bId = null;
        // 1) preference pairs
        for (const id of waiting) {
            const t = queueTickets.get(id);
            if (t.prefer_opponent) {
            const target = queueTickets.get(t.prefer_opponent);
            if (target && !target.matched_match_id && target.room_code &&
                (target.profile_id === t.prefer_opponent ||
                 target.name === t.prefer_opponent)) {
                    aId = id;
                    bId = t.prefer_opponent;
                    break;
                }
            }
        }
        // 2) FIFO
        if (aId === null) {
            aId = waiting[0];
            bId = waiting[1];
        }
        const a = queueTickets.get(aId);
        const b = queueTickets.get(bId);
        waiting.splice(waiting.indexOf(aId), 1);
        waiting.splice(waiting.indexOf(bId), 1);

        const matchId = generateId();
        // The first person to queue (host) decides the map both players use.
        const map = (a.map || "").length > 0 ? a.map : pickDuelMap(a.maps, b.maps);
        a.matched_match_id = matchId;
        b.matched_match_id = matchId;
        matches.set(matchId, {
            match_id: matchId,
            host_ticket_id: aId,
            client_ticket_id: bId,
            host_name: a.name,
            client_name: b.name,
            map,
            host_room_code: a.room_code || "",
            state: "assigning",
            created_at: Date.now()
        });
        console.log("[DUEL MATCH] match=" + matchId.substring(0, 8) +
            " host=\"" + a.name + "\" client=\"" + b.name + "\" map=" + map);
    }
}

function cancelMatch(matchId) {
    const match = matches.get(matchId);
    if (!match) return;
    match.state = "cancelled";
    for (const ticketId of [match.host_ticket_id, match.client_ticket_id]) {
        const ticket = queueTickets.get(ticketId);
        if (ticket && ticket.matched_match_id === matchId)
            ticket.matched_match_id = null;
    }
    matches.delete(matchId);
    console.log("[DUEL MATCH] cancelled match=" + matchId.substring(0, 8));
}

function cleanupQueue() {
    const now = Date.now();
    for (const [ticketId, ticket] of queueTickets) {
        if (now - (ticket.last_poll || ticket.joined_at) > QUEUE_TICKET_TIMEOUT_MS) {
            if (ticket.matched_match_id)
                cancelMatch(ticket.matched_match_id);
            queueTickets.delete(ticketId);
            console.log("[DUEL QUEUE] dropped stale ticket=" + ticketId.substring(0, 8));
        }
    }
    for (const [matchId, match] of matches) {
        if (match.state !== "ready" &&
            now - match.created_at > MATCH_CONFIRM_TIMEOUT_MS) {
            cancelMatch(matchId);
        }
    }
}
setInterval(cleanupQueue, 10000);

function generateCode() {
    const chars = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    let code;
    do {
        code = "";
        for (let i = 0; i < 7; i++)
            code += chars[Math.floor(Math.random() * chars.length)];
    } while (rooms.has(code));
    return code;
}

function generateToken() {
    return crypto.randomBytes(24).toString("hex");
}

function generateId() {
    return crypto.randomBytes(12).toString("hex");
}

// Clean up expired rooms and requests
function cleanExpired() {
    const now = Date.now();
    for (const [code, room] of rooms) {
        if (now - room.last_heartbeat > ROOM_TIMEOUT_MS)
            rooms.delete(code);
    }
}
setInterval(cleanExpired, 10000);

function getClientIp(req) {
    const forwarded = req.headers["x-forwarded-for"];
    if (forwarded) return forwarded.split(",")[0].trim();
    return req.socket.remoteAddress || "0.0.0.0";
}

function json(res, code, data) {
    res.writeHead(code, {
        "Content-Type": "application/json",
        "Access-Control-Allow-Origin": "*",
        "Access-Control-Allow-Methods": "POST, GET, OPTIONS",
        "Access-Control-Allow-Headers": "Content-Type"
    });
    res.end(JSON.stringify(data));
}

function readBody(req) {
    return new Promise((resolve, reject) => {
        let body = "";
        req.on("data", chunk => body += chunk);
        req.on("end", () => {
            try { resolve(JSON.parse(body)); }
            catch (e) { reject(new Error("invalid-json")); }
        });
        req.on("error", reject);
    });
}

function sdpHasUfragPwd(sdp) {
    return sdp.includes("a=ice-ufrag:") && sdp.includes("a=ice-pwd:");
}

// Generate TURN REST credentials using coturn's scheme:
// username = <unix-timestamp>:<username>
// credential = base64(HMAC-SHA1(shared_secret, username))
function generateTurnCredentials() {
    // The TURN relay is coTURN on the VPS (UDP/TCP 3478). The coordinator API is
    // proxied through nginx at mimita.fun, but Cloudflare/nginx only front HTTP —
    // they do NOT route UDP 3478. The relay must advertise its raw reachable IP
    // so ICE candidate gathering can actually reach it. Returning mimita.fun here
    // made every server/client hang on "gather timeout" and fail to connect.
    const host = "107.191.48.226";
    const port = 3478;
    if (!TURN_SHARED_SECRET) {
        return { ok: false };
    }
    const expiry = Math.floor(Date.now() / 1000) + 3600; // 1 hour
    const user = "mimita";
    const username = expiry + ":" + user;
    const hmac = crypto.createHmac("sha1", TURN_SHARED_SECRET).update(username).digest("base64");
    return {
        ok: true,
        host,
        port,
        username,
        credential: hmac,
        expires_at: expiry
    };
}

function roomToLookup(room) {
    const expired = Date.now() - room.last_heartbeat > ROOM_TIMEOUT_MS;
    return {
        exists: true,
        server_name: room.server_name,
        map: room.map,
        gamemode: room.gamemode,
        players: room.players,
        max_players: room.max_players,
        password_protected: !!room.password_protected,
        status: expired ? "offline" : "online",
        public_ip: room.public_ip,
        port: room.port,
        is_ice: room.room_type === "ice"
    };
}

// Public server-browser entry: expands room metadata with live uptime.
function roomToBrowserEntry(room) {
    const now = Date.now();
    return {
        code: room.code,
        server_name: room.server_name,
        host_player_name: room.host_player_name || "",
        map: room.map,
        gamemode: room.gamemode,
        players: room.players,
        max_players: room.max_players,
        password_protected: !!room.password_protected,
        uptime_seconds: Math.floor((now - (room.started_at || room.last_heartbeat)) / 1000),
        public_ip: room.public_ip,
        port: room.port,
        is_ice: room.room_type === "ice"
    };
}

function announceDiscordServerLive(room) {
    if (room.discord_notification === false) {
        console.log("[DISCORD SERVER ANNOUNCE] disabled code=" + room.code);
        return;
    }
    const webhookUrl = process.env.MIMITA_DISCORD_WEBHOOK_URL || "";
    if (!webhookUrl) {
        console.log("[DISCORD SERVER ANNOUNCE] skipped code=" + room.code + " reason=webhook-not-configured");
        return;
    }

    let target;
    try {
        target = new URL(webhookUrl);
    } catch (error) {
        console.error("[DISCORD SERVER ANNOUNCE] invalid webhook URL: " + error.message);
        return;
    }
    const startedAtMs = room.started_at || Date.now();
    const startedAtSeconds = Math.floor(startedAtMs / 1000);
    const utcStartedAt = new Date(startedAtMs).toISOString();
    const payload = JSON.stringify({
        content: "🟢 MiMITA server started\n\n" +
            "Server: " + room.server_name + "\n" +
            "Host: " + (room.host_player_name || "Unknown") + "\n" +
            "Players: " + room.players + "/" + room.max_players + "\n" +
            "Map: " + room.map + "\n" +
            "Mode: " + room.gamemode + "\n" +
            "Started: <t:" + startedAtSeconds + ":F>\n" +
            "Relative: <t:" + startedAtSeconds + ":R>\n" +
            "UTC: " + utcStartedAt + "\n" +
            "Join code: " + room.code
    });
    const client = target.protocol === "https:" ? https : http;
    const request = client.request(target, {
        method: "POST",
        headers: {
            "Content-Type": "application/json",
            "Content-Length": Buffer.byteLength(payload)
        },
        timeout: 5000
    }, response => {
        response.resume();
        console.log("[DISCORD SERVER ANNOUNCE] code=" + room.code + " status=" + response.statusCode);
    });
    request.on("error", error =>
        console.error("[DISCORD SERVER ANNOUNCE] code=" + room.code + " failed=" + error.message));
    request.end(payload);
}

const routes = {
    // ── Standard room registration ──
    "/api/coordinator/register": async (req, res) => {
        const body = await readBody(req);
        const code = generateCode();
        const joinToken = generateToken();
        const publicIp = body.public_ip || getClientIp(req);
        const port = body.port || 1357;
        rooms.set(code, {
            code,
            room_type: "normal",
            host_session_id: body.host_session_id || "",
            server_name: body.server_name || "MiMITA Server",
            public_ip: publicIp,
            port,
            map: body.map || "funworldv3",
            gamemode: body.gamemode || "sandbox",
            players: body.players || 0,
            max_players: body.max_players || 32,
            password_protected: !!body.password_protected,
            last_heartbeat: Date.now(),
            join_tokens: [joinToken],
            used_tokens: [],
            connections: new Map()
        });
        console.log("[REGISTER] code=" + code);
        json(res, 200, { code, join_token: joinToken });
    },

    "/api/coordinator/heartbeat": async (req, res) => {
        const body = await readBody(req);
        const room = rooms.get(body.code);
        if (!room) return json(res, 404, { error: "room-not-found" });
        room.last_heartbeat = Date.now();
        if (body.players !== undefined) room.players = body.players;
        json(res, 200, { ok: true });
    },

    "/api/coordinator/leave": async (req, res) => {
        const body = await readBody(req);
        if (!rooms.has(body.code)) return json(res, 404, { error: "room-not-found" });
        rooms.delete(body.code);
        console.log("[LEAVE] code=" + body.code);
        json(res, 200, { ok: true });
    },

    "/api/coordinator/lookup": async (req, res) => {
        const body = await readBody(req);
        const room = rooms.get(body.code);
        if (!room) return json(res, 200, { exists: false });
        json(res, 200, roomToLookup(room));
    },

    "/api/coordinator/join": async (req, res) => {
        const body = await readBody(req);
        const room = rooms.get(body.code);
        if (!room) return json(res, 404, { error: "room-not-found" });
        if (Date.now() - room.last_heartbeat > ROOM_TIMEOUT_MS)
            return json(res, 410, { error: "room-gone" });
        if (room.players >= room.max_players)
            return json(res, 403, { error: "room-full" });
        const joinToken = generateToken();
        room.join_tokens.push(joinToken);
        const clientIp = getClientIp(req);
        const isLocalHost = clientIp === "127.0.0.1" || clientIp === "::1" || clientIp === room.public_ip;
        json(res, 200, {
            join_token: joinToken,
            server_ip: isLocalHost ? "127.0.0.1" : room.public_ip,
            server_port: room.port,
            server_name: room.server_name,
            server_code: room.code
        });
    },

    "/api/coordinator/join-validate": async (req, res) => {
        const body = await readBody(req);
        const room = rooms.get(body.code);
        if (!room) return json(res, 404, { error: "room-not-found" });
        if (room.used_tokens.includes(body.join_token)) {
            return json(res, 200, { valid: true }); // idempotent
        }
        const idx = room.join_tokens.indexOf(body.join_token);
        if (idx === -1) return json(res, 403, { error: "invalid-token" });
        room.join_tokens.splice(idx, 1);
        room.used_tokens.push(body.join_token);
        json(res, 200, { valid: true });
    },

    // ── ICE Host: creates room with ICE type ──
    "/api/coordinator/ice/host": async (req, res) => {
        const body = await readBody(req);
        const hostSessionId = body.host_session_id || generateId();
        const code = generateCode();
        const joinToken = generateToken();
        rooms.set(code, {
            code,
            room_type: "ice",
            host_session_id: hostSessionId,
            host_description: body.ice_description || "",
            server_name: body.server_name || "MiMITA Server",
            host_player_name: body.host_player_name || "",
            public_ip: getClientIp(req),
            port: body.port || 1357,
            map: body.map || "funworldv3",
            gamemode: body.gamemode || "sandbox",
            players: body.players || 0,
            max_players: body.max_players || 999,
            password_protected: !!body.password_protected,
            discord_notification: body.discord_notification !== false,
            started_at: Date.now(),
            last_heartbeat: Date.now(),
            join_tokens: [joinToken],
            used_tokens: [],
            connections: new Map()    // requestId -> connection object
        });
        console.log("[ICE ROOM REGISTER] code=" + code + " name=\"" + (body.server_name || "") + "\" players=" + (body.players || 0) + " descBytes=" + (body.ice_description || "").length);
        announceDiscordServerLive(rooms.get(code));
        json(res, 200, {
            ok: true,
            room_code: code,
            host_session_id: hostSessionId,
            join_token: joinToken
        });
    },

    // ── Server browser list ──
    "/api/coordinator/list": async (req, res) => {
        const now = Date.now();
        const servers = [];
        for (const [code, room] of rooms) {
            if (now - room.last_heartbeat > ROOM_TIMEOUT_MS)
                continue;
            servers.push(roomToBrowserEntry(room));
        }
        console.log("[LIST] rooms=" + servers.length);
        json(res, 200, { ok: true, servers });
    },

    // ── ICE Lookup (non-mutating) ──
    "/api/coordinator/ice/lookup": async (req, res) => {
        const body = await readBody(req);
        const room = rooms.get(body.code);
        if (!room) return json(res, 200, { exists: false });
        json(res, 200, roomToLookup(room));
    },

    // ── ICE Begin Join ──
    "/api/coordinator/ice/begin-join": async (req, res) => {
        const body = await readBody(req);
        const room = rooms.get(body.room_code);
        if (!room || room.room_type !== "ice")
            return json(res, 200, { ok: false, error: "room-not-found" });
        if (Date.now() - room.last_heartbeat > ROOM_TIMEOUT_MS)
            return json(res, 200, { ok: false, error: "room-expired" });

        const sdp = body.ice_description || "";
        if (!sdpHasUfragPwd(sdp))
            return json(res, 200, { ok: false, error: "invalid-sdp" });

        const requestId = generateId();
        const joinToken = generateToken();
        const conn = {
            requestId,
            client_session_id: body.client_session_id || generateId(),
            client_ice_description: sdp,
            host_peer_sdp: null,
            join_token: joinToken,
            status: "pending",
            created_at: Date.now()
        };
        room.connections.set(requestId, conn);
        room.join_tokens.push(joinToken);
        console.log("[ICE JOIN BEGIN] code=" + body.room_code + " req=" + requestId.substring(0,12) + "... sdpBytes=" + sdp.length);

        json(res, 200, {
            ok: true,
            request_id: requestId,
            join_token: joinToken,
            host_ice_description: ""
        });
    },

    // ── ICE Host Poll ──
    "/api/coordinator/ice/host-poll": async (req, res) => {
        const body = await readBody(req);
        const room = rooms.get(body.room_code);
        if (!room || room.room_type !== "ice")
            return json(res, 200, { ok: false, error: "room-not-found" });
        room.last_heartbeat = Date.now();
        if (body.host_session_id !== room.host_session_id)
            return json(res, 403, { error: "host-not-authorized" });
        if (body.players !== undefined && Number.isFinite(body.players))
            room.players = Math.max(0, Math.floor(body.players));

        // Find first pending request
        let found = null;
        const now = Date.now();
        for (const [rid, conn] of room.connections) {
            if (
                conn.status === "complete" &&
                conn.completed_at &&
                now - conn.completed_at > COMPLETED_REQUEST_RETENTION_MS
            ) {
                room.connections.delete(rid);

                console.log(
                    "[ICE REQUEST CLEANUP] code=" +
                    body.room_code +
                    " req=" +
                    rid.substring(0, 12) +
                    "... reason=completed-retention-expired"
                );

                continue;
            }

            if (
                conn.status !== "complete" &&
                now - conn.created_at > REQUEST_TIMEOUT_MS
            ) {
                room.connections.delete(rid);

                console.log(
                    "[ICE REQUEST CLEANUP] code=" +
                    body.room_code +
                    " req=" +
                    rid.substring(0, 12) +
                    "... reason=request-timeout"
                );

                continue;
            }

            if (conn.status === "pending") {
                found = { rid, conn };
                break;
            }
        }

        if (found) {
            console.log("[ICE HOST POLL] code=" + body.room_code + " req=" + found.rid.substring(0,12) + "... pending");
            json(res, 200, {
                ok: true,
                has_request: true,
                request_id: found.rid,
                client_session_id: found.conn.client_session_id,
                client_ice_description: found.conn.client_ice_description
            });
        } else {
            json(res, 200, {
                ok: true,
                has_request: false
            });
        }
    },

    // ── ICE Host Answer ──
    "/api/coordinator/ice/host-answer": async (req, res) => {
        const body = await readBody(req);
        const room = rooms.get(body.room_code);
        if (!room || room.room_type !== "ice")
            return json(res, 200, { ok: false, error: "room-not-found" });
        if (body.host_session_id !== room.host_session_id)
            return json(res, 403, { error: "host-not-authorized" });

        const conn = room.connections.get(body.request_id);
        if (!conn)
            return json(res, 200, { ok: false, error: "request-not-found" });
        if (conn.status !== "pending")
            return json(res, 200, { ok: false, error: "request-already-answered" });

        conn.host_peer_sdp = body.host_peer_sdp || "";
        conn.status = "answered";
        console.log("[ICE HOST ANSWER] code=" + body.room_code + " req=" + body.request_id.substring(0,12) + "... sdpBytes=" + (body.host_peer_sdp || "").length);
        json(res, 200, { ok: true });
    },

    // ── ICE Client Poll ──
    "/api/coordinator/ice/client-poll": async (req, res) => {
        const body = await readBody(req);
        const room = rooms.get(body.room_code);
        if (!room || room.room_type !== "ice")
            return json(res, 200, { ok: true, status: "failed", error: "room-not-found" });

        const conn = room.connections.get(body.request_id);
        if (!conn)
            return json(res, 200, { ok: true, status: "failed", error: "request-not-found" });
        if (Date.now() - conn.created_at > REQUEST_TIMEOUT_MS) {
            room.connections.delete(body.request_id);
            return json(res, 200, { ok: true, status: "expired", error: "request-expired" });
        }

        if (
            (conn.status === "answered" || conn.status === "complete") &&
            conn.host_peer_sdp
        ) {
            console.log(
                "[ICE CLIENT POLL] req=" +
                body.request_id.substring(0, 12) +
                "... answer-ready status=" +
                conn.status
            );
            console.log("[ICE CLIENT POLL] req=" + body.request_id.substring(0,12) + "... answered");
            json(res, 200, {
                ok: true,
                status: "answered",
                host_ice_description: conn.host_peer_sdp
            });
        } else {
            json(res, 200, {
                ok: true,
                status: "pending",
                host_ice_description: ""
            });
        }
    },

    // ── ICE Request Complete ──
    "/api/coordinator/ice/request-complete": async (req, res) => {
        const body = await readBody(req);
        const room = rooms.get(body.room_code);

        if (room) {
            const conn = room.connections.get(body.request_id);

            if (conn) {
                // Do not delete immediately. On fast same-PC ICE connections,
                // the host can connect and call request-complete before the
                // joining client performs its next HTTP client-poll.
                conn.status = "complete";
                conn.completed_at = Date.now();

                console.log(
                    "[ICE REQUEST COMPLETE] code=" +
                    body.room_code +
                    " req=" +
                    (body.request_id || "").substring(0, 12) +
                    "... retained=1 retentionMs=" +
                    COMPLETED_REQUEST_RETENTION_MS
                );
            } else {
                console.log(
                    "[ICE REQUEST COMPLETE] code=" +
                    body.room_code +
                    " req=" +
                    (body.request_id || "").substring(0, 12) +
                    "... found=0"
                );
            }
        }

        json(res, 200, { ok: true });
    },

    // ── ICE Validate Join ──
    "/api/coordinator/ice/validate-join": async (req, res) => {
        const body = await readBody(req);
        const room = rooms.get(body.room_code);
        if (!room) return json(res, 404, { error: "room-not-found" });

        // Check used tokens first for idempotency
        if (room.used_tokens.includes(body.join_token)) {
            return json(res, 200, { valid: true });
        }
        const idx = room.join_tokens.indexOf(body.join_token);
        if (idx === -1) return json(res, 403, { error: "invalid-token" });
        room.join_tokens.splice(idx, 1);
        room.used_tokens.push(body.join_token);
        json(res, 200, { valid: true });
    },

    // ── ICE Done ──
    "/api/coordinator/ice/done": async (req, res) => {
        const body = await readBody(req);
        const room = rooms.get(body.room_code);
        if (room) {
            console.log("[ICE DONE] code=" + body.room_code);
            rooms.delete(body.room_code);
        }
        json(res, 200, { ok: true });
    },

    // ── TURN Credentials ──
    "/api/coordinator/turn-credentials": async (req, res) => {
        const creds = generateTurnCredentials();
        if (!creds.ok) {
            json(res, 200, { ok: false });
        } else {
            json(res, 200, {
                ok: true,
                host: creds.host,
                port: creds.port,
                username: creds.username,
                credential: creds.credential,
                expires_at: creds.expires_at
            });
        }
    },

    // ── Duels queue / matchmaking ──
    "/api/duels/queue/join": async (req, res) => {
        const body = await readBody(req);
        const profileId = String(body.profile_id || "").substring(0, 64);
        const name = String(body.name || "Player").substring(0, 32);
        if (!profileId) return json(res, 200, { ok: false, error: "missing-profile" });

        // Each game instance has a unique session_id; use it as the ticket key
        // so two instances (even the same account / guest name) become two
        // separate tickets and can match each other. Re-joining with the same
        // session_id (same instance re-queueing) reuses its ticket.
        const sessionId = body.session_id ? String(body.session_id).substring(0, 80) : "";
        const ticketId = sessionId || generateId();
        if (!queueTickets.has(ticketId))
            queueTickets.set(ticketId, {});

        const ticket = queueTickets.get(ticketId);
        ticket.ticket_id = ticketId;
        ticket.profile_id = profileId;
        ticket.name = name;
        ticket.prefer_opponent = body.prefer_opponent ? String(body.prefer_opponent) : "";
        ticket.maps = Array.isArray(body.maps) ? body.maps.map(String) : [];
        ticket.map = body.map ? String(body.map) : "";
        ticket.room_code = body.room_code ? String(body.room_code) : "";
        ticket.joined_at = Date.now();
        ticket.last_poll = Date.now();
        ticket.matched_match_id = null;

        console.log("[DUEL QUEUE] join ticket=" + ticketId.substring(0, 8) +
            " player=\"" + name + "\" code=\"" + (ticket.room_code || "-") + "\" maps=" + ticket.maps.length);
        tryMatchQueue();
        json(res, 200, { ok: true, ticket_id: ticketId });
    },

    "/api/duels/queue/leave": async (req, res) => {
        const body = await readBody(req);
        const ticket = queueTickets.get(body.ticket_id);
        if (ticket && ticket.matched_match_id)
            cancelMatch(ticket.matched_match_id);
        queueTickets.delete(body.ticket_id);
        json(res, 200, { ok: true });
    },

    "/api/duels/queue/poll": async (req, res) => {
        const body = await readBody(req);
        const ticket = queueTickets.get(body.ticket_id);
        if (!ticket)
            return json(res, 200, { ok: false, status: "error", error: "ticket-not-found" });
        ticket.last_poll = Date.now();

        const matchId = ticket.matched_match_id;
        if (!matchId)
            return json(res, 200, { ok: true, status: "waiting", position: 1 });

        const match = matches.get(matchId);
        if (!match || match.state === "cancelled") {
            ticket.matched_match_id = null;
            return json(res, 200, { ok: true, status: "waiting", position: 1 });
        }

        if (match.host_ticket_id === body.ticket_id) {
            return json(res, 200, {
                ok: true,
                status: "matched_host",
                match_id: matchId,
                map: match.map,
                opponent_name: match.client_name
            });
        }

        if (match.host_room_code) {
            return json(res, 200, {
                ok: true,
                status: "match_ready",
                match_id: matchId,
                room_code: match.host_room_code,
                map: match.map,
                opponent_name: match.host_name
            });
        }

        return json(res, 200, {
            ok: true,
            status: "waiting_for_host",
            match_id: matchId,
            map: match.map,
            opponent_name: match.host_name
        });
    },

    "/api/duels/queue/host-ready": async (req, res) => {
        const body = await readBody(req);
        const match = matches.get(body.match_id);
        if (!match)
            return json(res, 200, { ok: false, error: "match-not-found" });
        match.host_room_code = String(body.room_code || "");
        match.state = "ready";
        console.log("[DUEL MATCH] ready match=" + body.match_id.substring(0, 8) + " code=" + match.host_room_code);
        json(res, 200, { ok: true });
    }
};

const server = http.createServer((req, res) => {
    if (req.method === "OPTIONS") {
        res.writeHead(204, {
            "Access-Control-Allow-Origin": "*",
            "Access-Control-Allow-Methods": "POST, GET, OPTIONS",
            "Access-Control-Allow-Headers": "Content-Type"
        });
        return res.end();
    }

    const handler = routes[req.url];
    if (!handler || req.method !== "POST") {
        return json(res, 404, { error: "not-found" });
    }

    handler(req, res).catch(err => {
        console.error("[ERROR] " + req.url + ": " + err.message);
        json(res, 400, { error: err.message });
    });
});

server.listen(PORT, "0.0.0.0", () => {
    if (!TURN_SHARED_SECRET) {
        console.log("[COORDINATOR] WARNING: MIMITA_TURN_SECRET not set; TURN credentials disabled");
    }
    console.log("[COORDINATOR] listening on port " + PORT);
});
