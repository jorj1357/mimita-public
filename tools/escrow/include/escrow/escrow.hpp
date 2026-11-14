// 2026-11-13
// Standalone escrow ledger with a persistent JSON store and append-only event log.
//
// Money is stored as integer cents. The tool is intentionally independent of the
// game runtime so it can be embedded by tooling or run from the terminal.

#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace escrow {

struct Config {
    std::string store_path = "escrow-state.json";
    std::string event_log_path = "escrow-events.jsonl";
    std::string admin = "admin";
    std::string fee_account = "fees";
    std::string default_currency = "USD";
    std::int64_t default_fee_bps = 100;
    std::int64_t max_fee_bps = 1000;
};

struct Result {
    bool ok = false;
    std::string error;

    static Result success() { return Result{true, {}}; }
    static Result failure(std::string message) {
        return Result{false, std::move(message)};
    }
    explicit operator bool() const { return ok; }
};

struct Deal {
    std::uint64_t id = 0;
    std::string buyer;
    std::string seller;
    std::string arbiter;
    std::int64_t amount_cents = 0;
    std::int64_t fee_bps = 0;
    std::int64_t fee_cents = 0;
    std::string currency;
    std::string memo;
    std::string state;
    std::int64_t deadline_unix = 0;
    std::int64_t created_unix = 0;
    std::int64_t funded_unix = 0;
    std::int64_t settled_unix = 0;
};

class Escrow {
public:
    Escrow() = default;
    Escrow(const Escrow&) = delete;
    Escrow& operator=(const Escrow&) = delete;
    Escrow(Escrow&&) = default;
    Escrow& operator=(Escrow&&) = default;

    static Result load(const Config& config, Escrow* out);

    Result deposit(const std::string& account, std::int64_t amount_cents);
    Result create(const std::string& buyer, const std::string& seller,
                  const std::string& arbiter, std::int64_t amount_cents,
                  const std::string& currency, std::int64_t deadline_unix,
                  std::int64_t fee_bps, const std::string& memo,
                  std::uint64_t* deal_id);
    Result fund(std::uint64_t deal_id, const std::string& actor,
                const std::string& op_id);
    Result release(std::uint64_t deal_id, const std::string& actor,
                   const std::string& op_id);
    Result refund(std::uint64_t deal_id, const std::string& actor,
                  const std::string& op_id);
    Result expire(std::uint64_t deal_id, std::int64_t now_unix,
                  const std::string& op_id);
    Result pause(const std::string& actor, bool paused);
    Result save() const;

    std::optional<Deal> find(std::uint64_t deal_id) const;
    std::vector<Deal> deals() const;
    std::int64_t balance(const std::string& account) const;
    bool paused() const;
    std::uint64_t next_deal_id() const;

private:
    using Mutation = std::function<Result(nlohmann::json& deal,
                                          const std::string& action,
                                          const std::string& actor,
                                          const std::string& op_id)>;
    Result mutate(const std::string& action, std::uint64_t deal_id,
                  const std::string& actor, const std::string& op_id,
                  const Mutation& mutation);
    Result append_event(const std::string& action, const Deal& deal,
                        const std::string& actor,
                        const std::string& op_id) const;

    nlohmann::json state_;
    Config config_;
};

std::string format_money(std::int64_t cents);
std::optional<std::int64_t> parse_money(const std::string& text);
std::optional<std::int64_t> parse_iso8601_utc(const std::string& text);
std::string format_iso8601_utc(std::int64_t unix_seconds);
std::int64_t now_unix();

}  // namespace escrow
