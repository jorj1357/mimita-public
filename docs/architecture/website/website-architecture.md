// 2026-11-02
/* purpose
* Explain which MiMITA services serve the website, accounts, room discovery, and network relay.
* Give operators a stable map from a browser request to its owning process.
* DOES NOT define the full website UI or game networking protocol.
* DOES NOT authorize direct production edits or imply that an online process is healthy.
*/

# Website and online-service architecture

Audience: website developers and VPS operators.

## Website request path

```text
Browser
  -> nginx on the VPS (HTTPS 443)
     -> website frontend files from website/dist/
     -> website API on localhost:3002 for /api requests
        -> PostgreSQL 14/main on localhost:5432 for accounts and site data
```

The frontend is built from `website/src/` with `npm run build`. The Express API
owns authentication, profiles, moderation, support, and website data access.
PostgreSQL owns permanent accounts and persisted website/game progression.
An HTTP 200 from `/` proves only that the page was served; it does not prove the
API or database is available. PM2 `online` proves that Node is running, not that
PostgreSQL accepts a query.

The production debug router is intentionally unavailable in production. Use
the public, detail-free `/api/health/ready` response for external monitoring;
it checks a database query and disk headroom without returning host paths,
database names, configuration, or exception text. `/api/health/backup` reports
only whether a full database archive was uploaded and verified within the last
26 hours; its status comes from a root-written marker and does not expose the
backup name, timestamp, Drive path, or error details.

## Room-code multiplayer path

```text
PC A: starts the authoritative MiMITA game server
  -> coordinator: registers room code and exchanges ICE signaling
PC B/C/D: enter room code -> coordinator returns signaling/candidate data
  -> ICE tries a direct host connection first
  -> TURN may relay gameplay packets only when direct networking fails
```

The coordinator is a Node service on port 3001. It stores temporary room
discovery and signaling state; it does not simulate gameplay. The game server
on PC A remains authoritative. STUN on the VPS helps peers discover public
network candidates. Coturn's TURN relay can forward packets when peers cannot
connect directly. Coturn activity or STUN `BINDING` log lines alone do not
prove that gameplay was relayed.

The direct-only test policy lives in
`coordinator-server/turn-policy.json`. Disabling it stops TURN credentials for
new and reconnected sessions while keeping room codes, coordinator signaling,
and STUN available. Existing relay sessions may finish. Flip `enabled` back to
`true` through the reviewed repository deployment process to restore relay
fallback. Record the real activation time; do not backdate it.

## Service owners

| Service | Owner and responsibility | Health evidence |
|---|---|---|
| nginx | Public HTTP/HTTPS proxy and static frontend | External HTTPS probe and `systemctl is-active nginx` |
| `mimita-api` | Website API, port 3002 | `/api/health/ready`, API response, and PM2 status |
| `postgresql@14-main` | Website/account database, port 5432 | `pg_isready` plus API `SELECT 1` check |
| `mimita-database-backup.timer` | Daily verified PostgreSQL archive and encrypted Drive copy | `/api/health/backup`, timer status, and remote content verification |
| `mimita-coordinator` | Room-code discovery and ICE signaling, port 3001 | PM2 status and a bounded coordinator probe |
| Coturn | STUN and optional TURN relay, port 3478 | Service status and candidate-path evidence from the game |

The route from PC B to PC A is not the same as the website path. The website
and coordinator help peers find and negotiate a connection; PC A's game server
owns gameplay. A selected `ICE_PATH` event is the evidence for whether a
specific game session used a direct or relay candidate pair.
