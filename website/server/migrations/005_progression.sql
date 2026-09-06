-- 09 06 2026, 16 00
/* purpose
 * Extend the existing db.js schema without replacing accounts or totals.
 * Persist host membership and cumulative acknowledgements across retries.
 * Does not infer rewards, reset legacy counters, or contact production.
 */
ALTER TABLE game_stats ADD COLUMN IF NOT EXISTS playtime_ticks BIGINT;
UPDATE game_stats SET playtime_ticks = playtime_seconds * 60 WHERE playtime_ticks IS NULL;
ALTER TABLE game_stats ALTER COLUMN playtime_ticks SET DEFAULT 0;
ALTER TABLE game_stats ALTER COLUMN playtime_ticks SET NOT NULL;
ALTER TABLE game_stats ALTER COLUMN lifetime_player_kills TYPE BIGINT;
ALTER TABLE game_stats ALTER COLUMN lifetime_npc_kills TYPE BIGINT;
ALTER TABLE game_stats ALTER COLUMN lifetime_deaths TYPE BIGINT;
ALTER TABLE game_stats ADD CONSTRAINT game_stats_progression_nonnegative CHECK (
    gold >= 0 AND total_xp >= 0 AND lifetime_player_kills >= 0
    AND lifetime_npc_kills >= 0 AND lifetime_deaths >= 0 AND playtime_ticks >= 0
);
INSERT INTO game_stats (user_id) SELECT id FROM users ON CONFLICT (user_id) DO NOTHING;
CREATE FUNCTION initialize_user_game_stats() RETURNS trigger LANGUAGE plpgsql AS $$
BEGIN
    INSERT INTO game_stats(user_id) VALUES (NEW.id) ON CONFLICT (user_id) DO NOTHING;
    RETURN NEW;
END;
$$;
CREATE TRIGGER users_initialize_game_stats AFTER INSERT ON users
    FOR EACH ROW EXECUTE FUNCTION initialize_user_game_stats();
CREATE INDEX game_stats_xp_rank_idx ON game_stats(total_xp DESC, user_id);
CREATE INDEX game_stats_gold_rank_idx ON game_stats(gold DESC, user_id);
CREATE INDEX game_stats_playtime_rank_idx ON game_stats(playtime_ticks DESC, user_id);
CREATE INDEX game_stats_pvp_rank_idx ON game_stats(lifetime_player_kills DESC, user_id);
CREATE INDEX game_stats_deaths_rank_idx ON game_stats(lifetime_deaths DESC, user_id);
CREATE TABLE progression_sessions (
    session_id TEXT PRIMARY KEY,
    host_user_id BIGINT NOT NULL REFERENCES users(id),
    room_code TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
CREATE TABLE progression_players (
    session_id TEXT NOT NULL REFERENCES progression_sessions(session_id),
    user_id BIGINT NOT NULL REFERENCES users(id),
    ticket_hash TEXT UNIQUE,
    revision BIGINT NOT NULL DEFAULT 0 CHECK (revision >= 0),
    gold BIGINT NOT NULL DEFAULT 0 CHECK (gold >= 0),
    total_xp BIGINT NOT NULL DEFAULT 0 CHECK (total_xp >= 0),
    lifetime_player_kills BIGINT NOT NULL DEFAULT 0 CHECK (lifetime_player_kills >= 0),
    lifetime_npc_kills BIGINT NOT NULL DEFAULT 0 CHECK (lifetime_npc_kills >= 0),
    lifetime_deaths BIGINT NOT NULL DEFAULT 0 CHECK (lifetime_deaths >= 0),
    playtime_ticks BIGINT NOT NULL DEFAULT 0 CHECK (playtime_ticks >= 0),
    PRIMARY KEY(session_id, user_id)
);
