// 2026-11-13
// Escrow ledger implementation.

#include "escrow/escrow.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>

namespace escrow {
namespace {

namespace fs = std::filesystem;

constexpr std::int64_t kBpsDenominator = 10000;

std::int64_t account_balance(const nlohmann::json& accounts,
                             const std::string& account) {
    if (!accounts.is_object()) {
        return 0;
    }
    auto it = accounts.find(account);
    if (it == accounts.end() || !it->is_number_integer()) {
        return 0;
    }
    return it->get<std::int64_t>();
}

void set_account_balance(nlohmann::json& accounts, const std::string& account,
                         std::int64_t cents) {
    if (!accounts.is_object()) {
        accounts = nlohmann::json::object();
    }
    accounts[account] = cents;
}

Deal deal_from_json(const nlohmann::json& value) {
    Deal deal;
    if (!value.is_object()) {
        return deal;
    }
    auto get_string = [&](const char* key, std::string& out) {
        auto it = value.find(key);
        if (it != value.end() && it->is_string()) {
            out = it->get<std::string>();
        }
    };
    auto get_int = [&](const char* key, std::int64_t& out) {
        auto it = value.find(key);
        if (it != value.end() && it->is_number_integer()) {
            out = it->get<std::int64_t>();
        }
    };
    std::int64_t id_value = 0;
    get_int("id", id_value);
    deal.id = id_value > 0 ? static_cast<std::uint64_t>(id_value) : 0;
    get_string("buyer", deal.buyer);
    get_string("seller", deal.seller);
    get_string("arbiter", deal.arbiter);
    get_int("amount_cents", deal.amount_cents);
    get_int("fee_bps", deal.fee_bps);
    get_int("fee_cents", deal.fee_cents);
    get_string("currency", deal.currency);
    get_string("memo", deal.memo);
    get_string("state", deal.state);
    get_int("deadline_unix", deal.deadline_unix);
    get_int("created_unix", deal.created_unix);
    get_int("funded_unix", deal.funded_unix);
    get_int("settled_unix", deal.settled_unix);
    return deal;
}

nlohmann::json deal_to_json(const Deal& deal) {
    return nlohmann::json{
        {"id", deal.id},
        {"buyer", deal.buyer},
        {"seller", deal.seller},
        {"arbiter", deal.arbiter},
        {"amount_cents", deal.amount_cents},
        {"fee_bps", deal.fee_bps},
        {"fee_cents", deal.fee_cents},
        {"currency", deal.currency},
        {"memo", deal.memo},
        {"state", deal.state},
        {"deadline_unix", deal.deadline_unix},
        {"created_unix", deal.created_unix},
        {"funded_unix", deal.funded_unix},
        {"settled_unix", deal.settled_unix},
    };
}

std::string id_key(std::uint64_t id) { return std::to_string(id); }

std::string op_key(const std::string& action, std::uint64_t deal_id,
                   const std::string& op_id) {
    return action + ":" + std::to_string(deal_id) + ":" + op_id;
}

bool write_atomic(const fs::path& path, const std::string& text,
                  std::string* error) {
    std::error_code ec;
    if (!path.parent_path().empty()) {
        fs::create_directories(path.parent_path(), ec);
        if (ec) {
            if (error) {
                *error = "cannot create directory: " + ec.message();
            }
            return false;
        }
    }
    const fs::path temporary = path.string() + ".tmp";
    {
        std::ofstream out(temporary.string(),
                          std::ios::binary | std::ios::trunc);
        if (!out) {
            if (error) {
                *error = "cannot open temp file: " + temporary.string();
            }
            return false;
        }
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        out.flush();
        if (!out) {
            if (error) {
                *error = "cannot write temp file: " + temporary.string();
            }
            return false;
        }
    }
    fs::rename(temporary, path, ec);
    if (ec) {
        fs::remove(path, ec);
        ec.clear();
        fs::rename(temporary, path, ec);
    }
    if (ec) {
        if (error) {
            *error = "cannot replace " + path.string() + ": " + ec.message();
        }
        return false;
    }
    return true;
}

std::string now_iso() { return format_iso8601_utc(now_unix()); }

}  // namespace

std::int64_t now_unix() {
    return static_cast<std::int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

std::string format_money(std::int64_t cents) {
    const bool negative = cents < 0;
    const std::uint64_t absolute = static_cast<std::uint64_t>(
        negative ? -cents : cents);
    std::ostringstream out;
    if (negative) {
        out << '-';
    }
    out << (absolute / 100) << '.' << std::setw(2) << std::setfill('0')
        << (absolute % 100);
    return out.str();
}

std::optional<std::int64_t> parse_money(const std::string& text) {
    std::string value = text;
    if (!value.empty() && value.front() == '$') {
        value.erase(value.begin());
    }
    if (value.empty()) {
        return std::nullopt;
    }
    bool negative = false;
    std::size_t index = 0;
    if (value[0] == '-') {
        negative = true;
        index = 1;
    }
    std::int64_t whole = 0;
    bool digits = false;
    for (; index < value.size() && value[index] != '.'; ++index) {
        if (value[index] < '0' || value[index] > '9') {
            return std::nullopt;
        }
        digits = true;
        if (whole > (INT64_MAX - 9) / 10) {
            return std::nullopt;
        }
        whole = whole * 10 + (value[index] - '0');
    }
    std::int64_t fraction = 0;
    if (index < value.size() && value[index] == '.') {
        ++index;
        std::size_t places = 0;
        for (; index < value.size(); ++index) {
            if (value[index] < '0' || value[index] > '9' || places >= 2) {
                return std::nullopt;
            }
            fraction = fraction * 10 + (value[index] - '0');
            ++places;
        }
        if (places == 0) {
            return std::nullopt;
        }
        while (places < 2) {
            fraction *= 10;
            ++places;
        }
    }
    if (!digits) {
        return std::nullopt;
    }
    const std::int64_t cents = whole * 100 + fraction;
    return negative ? -cents : cents;
}

std::optional<std::int64_t> parse_iso8601_utc(const std::string& text) {
    std::tm time{};
    std::istringstream input(text);
    input >> std::get_time(&time, "%Y-%m-%dT%H:%M:%SZ");
    if (input.fail()) {
        input.clear();
        input.str(text);
        input >> std::get_time(&time, "%Y-%m-%dT%H:%M:%S");
    }
    if (input.fail()) {
        return std::nullopt;
    }
    time.tm_isdst = 0;
#if defined(_WIN32)
    const std::time_t value = _mkgmtime(&time);
#else
    const std::time_t value = timegm(&time);
#endif
    if (value == static_cast<std::time_t>(-1)) {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(value);
}

std::string format_iso8601_utc(std::int64_t unix_seconds) {
    const std::time_t value = static_cast<std::time_t>(unix_seconds);
    std::tm time{};
#if defined(_WIN32)
    gmtime_s(&time, &value);
#else
    gmtime_r(&value, &time);
#endif
    char buffer[32] = {};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &time);
    return std::string(buffer);
}

Result Escrow::load(const Config& config, Escrow* out) {
    if (!out) {
        return Result::failure("internal: null output");
    }
    Escrow escrow;
    escrow.config_ = config;
    const fs::path path(config.store_path);
    std::error_code ec;
    if (fs::exists(path, ec)) {
        std::ifstream input(path.string(), std::ios::binary);
        if (!input) {
            return Result::failure("cannot open store: " + config.store_path);
        }
        try {
            input >> escrow.state_;
        } catch (const std::exception& error) {
            return Result::failure(std::string("invalid store JSON: ") +
                                   error.what());
        }
    } else {
        escrow.state_ = nlohmann::json::object();
    }
    if (!escrow.state_.is_object()) {
        escrow.state_ = nlohmann::json::object();
    }
    if (!escrow.state_.contains("version")) {
        escrow.state_["version"] = 1;
    }
    if (!escrow.state_.contains("paused")) {
        escrow.state_["paused"] = false;
    }
    if (!escrow.state_.contains("next_deal_id")) {
        escrow.state_["next_deal_id"] = 1;
    }
    if (!escrow.state_.contains("next_event_id")) {
        escrow.state_["next_event_id"] = 1;
    }
    if (!escrow.state_.contains("accounts") ||
        !escrow.state_["accounts"].is_object()) {
        escrow.state_["accounts"] = nlohmann::json::object();
    }
    if (!escrow.state_.contains("deals") || !escrow.state_["deals"].is_object()) {
        escrow.state_["deals"] = nlohmann::json::object();
    }
    if (!escrow.state_.contains("applied_ops") ||
        !escrow.state_["applied_ops"].is_object()) {
        escrow.state_["applied_ops"] = nlohmann::json::object();
    }
    *out = std::move(escrow);
    return Result::success();
}

Result Escrow::save() const {
    std::string error;
    if (!write_atomic(fs::path(config_.store_path), state_.dump(2), &error)) {
        return Result::failure(error);
    }
    return Result::success();
}

std::optional<Deal> Escrow::find(std::uint64_t deal_id) const {
    auto deals = state_.find("deals");
    if (deals == state_.end() || !deals->is_object()) {
        return std::nullopt;
    }
    auto it = deals->find(id_key(deal_id));
    if (it == deals->end()) {
        return std::nullopt;
    }
    return deal_from_json(*it);
}

std::vector<Deal> Escrow::deals() const {
    std::vector<Deal> result;
    auto it = state_.find("deals");
    if (it == state_.end() || !it->is_object()) {
        return result;
    }
    for (const auto& entry : it->items()) {
        result.push_back(deal_from_json(entry.value()));
    }
    std::sort(result.begin(), result.end(),
              [](const Deal& a, const Deal& b) { return a.id < b.id; });
    return result;
}

std::int64_t Escrow::balance(const std::string& account) const {
    auto it = state_.find("accounts");
    if (it == state_.end()) {
        return 0;
    }
    return account_balance(*it, account);
}

bool Escrow::paused() const {
    auto it = state_.find("paused");
    return it != state_.end() && it->is_boolean() && it->get<bool>();
}

std::uint64_t Escrow::next_deal_id() const {
    auto it = state_.find("next_deal_id");
    return it != state_.end() && it->is_number_unsigned()
               ? it->get<std::uint64_t>()
               : 1;
}

Result Escrow::append_event(const std::string& action, const Deal& deal,
                            const std::string& actor,
                            const std::string& op_id) const {
    nlohmann::json event{
        {"event_id", state_.value("next_event_id", 1)},
        {"ts", now_iso()},
        {"action", action},
        {"deal_id", deal.id},
        {"actor", actor},
        {"state", deal.state},
        {"amount_cents", deal.amount_cents},
        {"fee_cents", deal.fee_cents},
        {"currency", deal.currency},
        {"op_id", op_id},
    };
    std::error_code ec;
    const fs::path path(config_.event_log_path);
    if (!path.parent_path().empty()) {
        fs::create_directories(path.parent_path(), ec);
    }
    std::ofstream out(path.string(), std::ios::binary | std::ios::app);
    if (!out) {
        return Result::failure("cannot open event log: " +
                               config_.event_log_path);
    }
    out << event.dump() << '\n';
    out.flush();
    if (!out) {
        return Result::failure("cannot append event log: " +
                               config_.event_log_path);
    }
    return Result::success();
}

Result Escrow::deposit(const std::string& account, std::int64_t amount_cents) {
    if (account.empty()) {
        return Result::failure("account is required");
    }
    if (amount_cents <= 0) {
        return Result::failure("deposit amount must be positive");
    }
    auto& accounts = state_["accounts"];
    const std::int64_t current = account_balance(accounts, account);
    if (current > INT64_MAX - amount_cents) {
        return Result::failure("balance overflow");
    }
    set_account_balance(accounts, account, current + amount_cents);
    Deal event_deal;
    event_deal.amount_cents = amount_cents;
    event_deal.state = "deposit";
    event_deal.currency = "";
    const Result logged = append_event("deposit", event_deal, account, "");
    if (!logged) {
        return logged;
    }
    state_["next_event_id"] = state_.value("next_event_id", 1) + 1;
    return save();
}

Result Escrow::create(const std::string& buyer, const std::string& seller,
                      const std::string& arbiter, std::int64_t amount_cents,
                      const std::string& currency, std::int64_t deadline_unix,
                      std::int64_t fee_bps, const std::string& memo,
                      std::uint64_t* deal_id) {
    if (buyer.empty() || seller.empty() || arbiter.empty()) {
        return Result::failure("buyer, seller, and arbiter are required");
    }
    if (buyer == seller) {
        return Result::failure("buyer and seller must differ");
    }
    if (amount_cents <= 0) {
        return Result::failure("amount must be positive");
    }
    if (fee_bps < 0 || fee_bps > config_.max_fee_bps) {
        return Result::failure("fee_bps must be between 0 and " +
                               std::to_string(config_.max_fee_bps));
    }
    if (currency.empty()) {
        return Result::failure("currency is required");
    }
    if (deadline_unix < 0) {
        return Result::failure("deadline must be a UTC timestamp");
    }

    const std::uint64_t id = next_deal_id();
    Deal deal;
    deal.id = id;
    deal.buyer = buyer;
    deal.seller = seller;
    deal.arbiter = arbiter;
    deal.amount_cents = amount_cents;
    deal.fee_bps = fee_bps;
    deal.currency = currency;
    deal.memo = memo;
    deal.state = "created";
    deal.deadline_unix = deadline_unix;
    deal.created_unix = now_unix();

    state_["deals"][id_key(id)] = deal_to_json(deal);
    state_["next_deal_id"] = id + 1;
    if (deal_id) {
        *deal_id = id;
    }
    const Result logged = append_event("create", deal, buyer, "");
    if (!logged) {
        return logged;
    }
    state_["next_event_id"] = state_.value("next_event_id", 1) + 1;
    return save();
}

Result Escrow::mutate(const std::string& action, std::uint64_t deal_id,
                      const std::string& actor, const std::string& op_id,
                      const Mutation& mutation) {
    if (paused() && action != "pause") {
        return Result::failure("escrow is paused");
    }
    auto deals = state_.find("deals");
    if (deals == state_.end() || !deals->is_object()) {
        return Result::failure("no deals exist");
    }
    auto deal_it = deals->find(id_key(deal_id));
    if (deal_it == deals->end()) {
        return Result::failure("unknown deal id " + std::to_string(deal_id));
    }
    if (!op_id.empty()) {
        const std::string key = op_key(action, deal_id, op_id);
        auto applied = state_["applied_ops"].find(key);
        if (applied != state_["applied_ops"].end()) {
            return Result::success();
        }
    }
    Result result = mutation(*deal_it, action, actor, op_id);
    if (!result) {
        return result;
    }
    if (!op_id.empty()) {
        state_["applied_ops"][op_key(action, deal_id, op_id)] = {
            {"actor", actor},
            {"state", (*deal_it).value("state", "")},
        };
    }
    const Deal updated = deal_from_json(*deal_it);
    result = append_event(action, updated, actor, op_id);
    if (!result) {
        return result;
    }
    state_["next_event_id"] = state_.value("next_event_id", 1) + 1;
    return save();
}

Result Escrow::fund(std::uint64_t deal_id, const std::string& actor,
                    const std::string& op_id) {
    return mutate("fund", deal_id, actor, op_id,
                  [&](nlohmann::json& deal, const std::string&,
                      const std::string&, const std::string&) -> Result {
                      if (deal.value("state", "") != "created") {
                          return Result::failure("deal is not in created state");
                      }
                      if (actor != deal.value("buyer", "")) {
                          return Result::failure("only the buyer may fund");
                      }
                      const std::int64_t amount = deal.value("amount_cents", 0);
                      if (amount <= 0) {
                          return Result::failure("deal amount is invalid");
                      }
                      const std::int64_t deadline =
                          deal.value("deadline_unix", 0);
                      if (deadline > 0 && now_unix() > deadline) {
                          return Result::failure("deal deadline has passed");
                      }
                      auto& accounts = state_["accounts"];
                      const std::int64_t current =
                          account_balance(accounts, actor);
                      if (current < amount) {
                          return Result::failure("insufficient balance");
                      }
                      set_account_balance(accounts, actor, current - amount);
                      deal["state"] = "funded";
                      deal["funded_unix"] = now_unix();
                      return Result::success();
                  });
}

Result Escrow::release(std::uint64_t deal_id, const std::string& actor,
                       const std::string& op_id) {
    return mutate("release", deal_id, actor, op_id,
                  [&](nlohmann::json& deal, const std::string&,
                      const std::string&, const std::string&) -> Result {
                      if (deal.value("state", "") != "funded") {
                          return Result::failure("deal is not funded");
                      }
                      const std::string arbiter = deal.value("arbiter", "");
                      if (actor != arbiter && actor != config_.admin) {
                          return Result::failure(
                              "only the arbiter or admin may release");
                      }
                      const std::int64_t amount = deal.value("amount_cents", 0);
                      const std::int64_t fee_bps = deal.value("fee_bps", 0);
                      const std::int64_t fee =
                          (amount * fee_bps) / kBpsDenominator;
                      auto& accounts = state_["accounts"];
                      const std::string seller = deal.value("seller", "");
                      set_account_balance(accounts, seller,
                                          account_balance(accounts, seller) +
                                              (amount - fee));
                      if (fee > 0) {
                          set_account_balance(
                              accounts, config_.fee_account,
                              account_balance(accounts, config_.fee_account) +
                                  fee);
                      }
                      deal["state"] = "released";
                      deal["fee_cents"] = fee;
                      deal["settled_unix"] = now_unix();
                      return Result::success();
                  });
}

Result Escrow::refund(std::uint64_t deal_id, const std::string& actor,
                      const std::string& op_id) {
    return mutate("refund", deal_id, actor, op_id,
                  [&](nlohmann::json& deal, const std::string&,
                      const std::string&, const std::string&) -> Result {
                      if (deal.value("state", "") != "funded") {
                          return Result::failure("deal is not funded");
                      }
                      const std::string arbiter = deal.value("arbiter", "");
                      if (actor != arbiter && actor != config_.admin) {
                          return Result::failure(
                              "only the arbiter or admin may refund");
                      }
                      const std::int64_t amount = deal.value("amount_cents", 0);
                      auto& accounts = state_["accounts"];
                      const std::string buyer = deal.value("buyer", "");
                      set_account_balance(accounts, buyer,
                                          account_balance(accounts, buyer) +
                                              amount);
                      deal["state"] = "refunded";
                      deal["settled_unix"] = now_unix();
                      return Result::success();
                  });
}

Result Escrow::expire(std::uint64_t deal_id, std::int64_t now_seconds,
                      const std::string& op_id) {
    return mutate("expire", deal_id, "system", op_id,
                  [&](nlohmann::json& deal, const std::string&,
                      const std::string&, const std::string&) -> Result {
                      if (deal.value("state", "") != "funded") {
                          return Result::failure("deal is not funded");
                      }
                      const std::int64_t deadline =
                          deal.value("deadline_unix", 0);
                      if (deadline <= 0) {
                          return Result::failure("deal has no deadline");
                      }
                      if (now_seconds < deadline) {
                          return Result::failure("deal deadline has not passed");
                      }
                      const std::int64_t amount = deal.value("amount_cents", 0);
                      auto& accounts = state_["accounts"];
                      const std::string buyer = deal.value("buyer", "");
                      set_account_balance(accounts, buyer,
                                          account_balance(accounts, buyer) +
                                              amount);
                      deal["state"] = "expired";
                      deal["settled_unix"] = now_seconds;
                      return Result::success();
                  });
}

Result Escrow::pause(const std::string& actor, bool paused) {
    if (actor != config_.admin) {
        return Result::failure("only admin may pause or resume");
    }
    state_["paused"] = paused;
    Deal event_deal;
    event_deal.state = paused ? "paused" : "resumed";
    const Result logged =
        append_event(paused ? "pause" : "resume", event_deal, actor, "");
    if (!logged) {
        return logged;
    }
    state_["next_event_id"] = state_.value("next_event_id", 1) + 1;
    return save();
}

}  // namespace escrow
