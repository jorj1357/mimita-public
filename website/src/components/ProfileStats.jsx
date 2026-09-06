// 09 06 2026, 14 43
/* purpose
* Display backend-persisted statistics on self and public profiles.
* Refresh once per minute using the existing diagnostic API wrapper.
* Reuse profile stat styling and preserve the last successful response.
* DOES NOT calculate rewards or save gameplay data from the browser.
*/
import { useEffect, useState } from "react"
import { apiRequest } from "../lib/api"
import { formatPersistentStat } from "../lib/persistentStats"

export default function ProfileStats({ userId }) {
    const [profile, setProfile] = useState(null)
    const [error, setError] = useState(false)

    useEffect(() => {
        const controller = new AbortController()
        let pending = false
        const refresh = async () => {
            if (pending) return
            pending = true
            try {
                const data = await apiRequest(`/api/profile/${encodeURIComponent(userId)}`, { signal: controller.signal })
                if (!data?.success || !data.profile) throw new Error("Invalid profile response")
                if (!controller.signal.aborted) {
                    setProfile(data.profile)
                    setError(false)
                }
            } catch {
                if (!controller.signal.aborted) setError(true)
            } finally {
                pending = false
            }
        }
        refresh()
        const timer = setInterval(refresh, 60 * 1000)
        return () => { controller.abort(); clearInterval(timer) }
    }, [userId])

    if (!profile) return <p role="status">{error ? "Unable to load statistics. Retrying shortly." : "Loading statistics..."}</p>

    const rows = [
        ["Level", profile.level, "#00d9d9"],
        ["Total XP", profile.totalXp, "#00d9d9"],
        ["Gold", profile.gold, "#ffd900"],
        ["Playtime", profile.playtimeTicks ?? profile.playtime_ticks, null, true],
        ["Player Kills", profile.playerKills],
        ["NPC Kills", profile.npcKills],
        ["Deaths", profile.deaths],
        ["Matches", profile.matchesPlayed],
        ["Wins", profile.wins, "#4caf50"],
        ["Losses", profile.losses, "#f44336"],
        ["FFA Played", profile.ffa?.played],
        ["FFA Wins", profile.ffa?.wins, "#4caf50"],
        ["TDM Played", profile.tdm?.played],
        ["TDM Wins", profile.tdm?.wins, "#4caf50"],
    ]
    return (
        <div className="profileStats">
            <p>Saved game totals · refreshes every minute.</p>
            {error && <p role="status">Unable to refresh statistics. Showing the last loaded totals.</p>}
            {rows.map(([label, value, color, playtime]) => (
                <div className="profileStatRow" key={label}>
                    <span className="profileStatLabel">{label}</span>
                    <span className="profileStatValue" style={{ color }}>{formatPersistentStat(value, playtime)}</span>
                </div>
            ))}
        </div>
    )
}
