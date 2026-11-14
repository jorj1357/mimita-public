// 2026-11-13
// Escrow CLI.

#include "escrow/escrow.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

constexpr const char* kVersion = "1.0.0";

void print_usage() {
    std::cout
        << "escrow " << kVersion << "\n\n"
        << "Usage: escrow [--config <path>] <command> [options]\n\n"
        << "Commands:\n"
        << "  init                         Create or normalize the store\n"
        << "  deposit --account A --amount N\n"
        << "  create --buyer A --seller B --arbiter C --amount N\n"
        << "         [--currency USD] [--deadline ISO8601|unix]\n"
        << "         [--fee-bps N] [--memo text]\n"
        << "  fund --id N --by A [--op-id X]\n"
        << "  release --id N --by A [--op-id X]\n"
        << "  refund --id N --by A [--op-id X]\n"
        << "  expire --id N [--now ISO8601|unix] [--op-id X]\n"
        << "  expire-all [--now ISO8601|unix]\n"
        << "  pause --by A | resume --by A\n"
        << "  status --id N [--json]\n"
        << "  list [--state S] [--json]\n"
        << "  balance --account A\n"
        << "  events [--id N]\n\n"
        << "Defaults come from --config JSON:\n"
        << "  store_path, event_log_path, admin, fee_account,\n"
        << "  default_fee_bps, max_fee_bps, default_currency\n";
}

struct Args {
    std::map<std::string, std::string> values;
    std::vector<std::string> flags;

    bool has(const std::string& key) const {
        return values.find(key) != values.end();
    }
    std::string get(const std::string& key,
                    const std::string& fallback = "") const {
        auto it = values.find(key);
        return it == values.end() ? fallback : it->second;
    }
    bool flag(const std::string& key) const {
        for (const auto& flag : flags) {
            if (flag == key) {
                return true;
            }
        }
        return false;
    }
};

bool parse_args(int argc, char** argv, int start, Args* out,
                std::string* error) {
    for (int index = start; index < argc; ++index) {
        const std::string token = argv[index];
        if (token.rfind("--", 0) != 0) {
            *error = "unexpected argument: " + token;
            return false;
        }
        const std::string key = token.substr(2);
        if (key == "json" || key == "help") {
            out->flags.push_back(key);
            continue;
        }
        if (index + 1 >= argc) {
            *error = "missing value for --" + key;
            return false;
        }
        out->values[key] = argv[++index];
    }
    return true;
}

bool load_config(const std::string& path, escrow::Config* config,
                 std::string* error) {
    if (path.empty()) {
        return true;
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        *error = "cannot open config: " + path;
        return false;
    }
    nlohmann::json json;
    try {
        input >> json;
    } catch (const std::exception& exception) {
        *error = std::string("invalid config JSON: ") + exception.what();
        return false;
    }
    if (!json.is_object()) {
        *error = "config root must be an object";
        return false;
    }
    auto assign = [&](const char* key, std::string& out) {
        auto it = json.find(key);
        if (it != json.end() && it->is_string()) {
            out = it->get<std::string>();
        }
    };
    auto assign_int = [&](const char* key, std::int64_t& out) {
        auto it = json.find(key);
        if (it != json.end() && it->is_number_integer()) {
            out = it->get<std::int64_t>();
        }
    };
    assign("store_path", config->store_path);
    assign("event_log_path", config->event_log_path);
    assign("admin", config->admin);
    assign("fee_account", config->fee_account);
    assign("default_currency", config->default_currency);
    assign_int("default_fee_bps", config->default_fee_bps);
    assign_int("max_fee_bps", config->max_fee_bps);

    const fs::path base = fs::path(path).parent_path();
    if (!base.empty()) {
        if (fs::path(config->store_path).is_relative()) {
            config->store_path = (base / config->store_path).string();
        }
        if (fs::path(config->event_log_path).is_relative()) {
            config->event_log_path =
                (base / config->event_log_path).string();
        }
    }
    return true;
}

std::optional<std::int64_t> parse_time_arg(const std::string& text) {
    if (!text.empty() && text.find_first_not_of("0123456789") == std::string::npos) {
        try {
            return static_cast<std::int64_t>(std::stoll(text));
        } catch (...) {
            return std::nullopt;
        }
    }
    return escrow::parse_iso8601_utc(text);
}

std::optional<std::int64_t> parse_integer_arg(const std::string& text) {
    if (text.empty() || text.find_first_not_of("-0123456789") != std::string::npos) {
        return std::nullopt;
    }
    try {
        return static_cast<std::int64_t>(std::stoll(text));
    } catch (...) {
        return std::nullopt;
    }
}

nlohmann::json deal_json(const escrow::Deal& deal) {
    return nlohmann::json{
        {"id", deal.id},
        {"buyer", deal.buyer},
        {"seller", deal.seller},
        {"arbiter", deal.arbiter},
        {"amount_cents", deal.amount_cents},
        {"amount", escrow::format_money(deal.amount_cents)},
        {"fee_bps", deal.fee_bps},
        {"fee_cents", deal.fee_cents},
        {"fee", escrow::format_money(deal.fee_cents)},
        {"currency", deal.currency},
        {"memo", deal.memo},
        {"state", deal.state},
        {"deadline_unix", deal.deadline_unix},
        {"deadline", deal.deadline_unix > 0
                          ? escrow::format_iso8601_utc(deal.deadline_unix)
                          : ""},
        {"created_unix", deal.created_unix},
        {"funded_unix", deal.funded_unix},
        {"settled_unix", deal.settled_unix},
    };
}

void print_deal(const escrow::Deal& deal) {
    std::cout << "deal " << deal.id << " [" << deal.state << "] "
              << escrow::format_money(deal.amount_cents) << " " << deal.currency
              << " buyer=" << deal.buyer << " seller=" << deal.seller
              << " arbiter=" << deal.arbiter;
    if (deal.deadline_unix > 0) {
        std::cout << " deadline=" << escrow::format_iso8601_utc(deal.deadline_unix);
    }
    if (deal.fee_cents > 0) {
        std::cout << " fee=" << escrow::format_money(deal.fee_cents);
    }
    if (!deal.memo.empty()) {
        std::cout << " memo=\"" << deal.memo << "\"";
    }
    std::cout << "\n";
}

int fail(const std::string& message) {
    std::cerr << "error: " << message << "\n";
    return 1;
}

}  // namespace

int main(int argc, char** argv) {
    std::string config_path;
    std::string command;
    Args args;
    int index = 1;
    if (index < argc && std::string(argv[index]) == "--config") {
        if (index + 1 >= argc) {
            return fail("--config requires a path");
        }
        config_path = argv[index + 1];
        index += 2;
    }
    if (index >= argc) {
        print_usage();
        return 2;
    }
    command = argv[index++];
    if (command == "help" || command == "--help" || command == "-h") {
        print_usage();
        return 0;
    }
    if (command == "--version" || command == "version") {
        std::cout << "escrow " << kVersion << "\n";
        return 0;
    }
    std::string error;
    if (!parse_args(argc, argv, index, &args, &error)) {
        return fail(error);
    }

    escrow::Config config;
    if (!load_config(config_path, &config, &error)) {
        return fail(error);
    }
    escrow::Escrow book;
    const escrow::Result loaded = escrow::Escrow::load(config, &book);
    if (!loaded) {
        return fail(loaded.error);
    }

    auto require = [&](const char* key) -> std::string {
        if (!args.has(key)) {
            return {};
        }
        return args.get(key);
    };
    auto parse_amount_arg = [&](const char* key,
                                std::int64_t* out) -> bool {
        if (!args.has(key)) {
            error = std::string("missing --") + key;
            return false;
        }
        const auto parsed = escrow::parse_money(args.get(key));
        if (!parsed || *parsed <= 0) {
            error = std::string("invalid --") + key;
            return false;
        }
        *out = *parsed;
        return true;
    };

    if (command == "init") {
        const escrow::Result result = book.save();
        if (!result) {
            return fail(result.error);
        }
        std::cout << "store ready: " << config.store_path << "\n";
        return 0;
    }
    if (command == "deposit") {
        std::int64_t amount = 0;
        if (!parse_amount_arg("amount", &amount)) {
            return fail(error);
        }
        const std::string account = require("account");
        if (account.empty()) {
            return fail("missing --account");
        }
        const escrow::Result result = book.deposit(account, amount);
        if (!result) {
            return fail(result.error);
        }
        std::cout << "deposited " << escrow::format_money(amount) << " to "
                  << account << " (balance "
                  << escrow::format_money(book.balance(account)) << ")\n";
        return 0;
    }
    if (command == "create") {
        std::int64_t amount = 0;
        if (!parse_amount_arg("amount", &amount)) {
            return fail(error);
        }
        const std::string buyer = require("buyer");
        const std::string seller = require("seller");
        const std::string arbiter = require("arbiter");
        if (buyer.empty() || seller.empty() || arbiter.empty()) {
            return fail("--buyer, --seller, and --arbiter are required");
        }
        const std::string currency = args.get("currency", config.default_currency);
        std::int64_t deadline = 0;
        if (args.has("deadline")) {
            const auto parsed = parse_time_arg(args.get("deadline"));
            if (!parsed || *parsed <= 0) {
                return fail("invalid --deadline (use ISO8601 or unix seconds)");
            }
            deadline = *parsed;
        }
        std::int64_t fee_bps = config.default_fee_bps;
        if (args.has("fee-bps")) {
            const auto parsed = parse_integer_arg(args.get("fee-bps"));
            if (!parsed || *parsed < 0 || *parsed > config.max_fee_bps) {
                return fail("invalid --fee-bps");
            }
            fee_bps = *parsed;
        }
        std::uint64_t id = 0;
        const escrow::Result result = book.create(
            buyer, seller, arbiter, amount, currency, deadline, fee_bps,
            args.get("memo"), &id);
        if (!result) {
            return fail(result.error);
        }
        std::cout << "created deal " << id << "\n";
        return 0;
    }
    if (command == "fund" || command == "release" || command == "refund") {
        if (!args.has("id") || !args.has("by")) {
            return fail("--id and --by are required");
        }
        std::uint64_t id = 0;
        try {
            id = std::stoull(args.get("id"));
        } catch (...) {
            return fail("invalid --id");
        }
        const std::string actor = args.get("by");
        const std::string op_id = args.get("op-id");
        escrow::Result result;
        if (command == "fund") {
            result = book.fund(id, actor, op_id);
        } else if (command == "release") {
            result = book.release(id, actor, op_id);
        } else {
            result = book.refund(id, actor, op_id);
        }
        if (!result) {
            return fail(result.error);
        }
        const auto deal = book.find(id);
        if (deal) {
            print_deal(*deal);
        }
        return 0;
    }
    if (command == "expire" || command == "expire-all") {
        std::int64_t now = escrow::now_unix();
        if (args.has("now")) {
            const auto parsed = parse_time_arg(args.get("now"));
            if (!parsed || *parsed <= 0) {
                return fail("invalid --now");
            }
            now = *parsed;
        }
        const std::string op_id = args.get("op-id");
        if (command == "expire") {
            if (!args.has("id")) {
                return fail("--id is required");
            }
            std::uint64_t id = 0;
            try {
                id = std::stoull(args.get("id"));
            } catch (...) {
                return fail("invalid --id");
            }
            const escrow::Result result = book.expire(id, now, op_id);
            if (!result) {
                return fail(result.error);
            }
            const auto deal = book.find(id);
            if (deal) {
                print_deal(*deal);
            }
            return 0;
        }
        int expired = 0;
        for (const auto& deal : book.deals()) {
            if (deal.state != "funded" || deal.deadline_unix <= 0 ||
                now < deal.deadline_unix) {
                continue;
            }
            const escrow::Result result =
                book.expire(deal.id, now, op_id);
            if (result) {
                ++expired;
            } else {
                std::cerr << "warning: deal " << deal.id << ": "
                          << result.error << "\n";
            }
        }
        std::cout << "expired " << expired << " deal(s)\n";
        return 0;
    }
    if (command == "pause" || command == "resume") {
        if (!args.has("by")) {
            return fail("--by is required");
        }
        const escrow::Result result =
            book.pause(args.get("by"), command == "pause");
        if (!result) {
            return fail(result.error);
        }
        std::cout << (command == "pause" ? "paused" : "resumed") << "\n";
        return 0;
    }
    if (command == "status") {
        if (!args.has("id")) {
            return fail("--id is required");
        }
        std::uint64_t id = 0;
        try {
            id = std::stoull(args.get("id"));
        } catch (...) {
            return fail("invalid --id");
        }
        const auto deal = book.find(id);
        if (!deal) {
            return fail("unknown deal id " + args.get("id"));
        }
        if (args.flag("json")) {
            std::cout << deal_json(*deal).dump(2) << "\n";
        } else {
            print_deal(*deal);
        }
        return 0;
    }
    if (command == "list") {
        nlohmann::json result = nlohmann::json::array();
        const std::string state_filter = args.get("state");
        for (const auto& deal : book.deals()) {
            if (!state_filter.empty() && deal.state != state_filter) {
                continue;
            }
            if (args.flag("json")) {
                result.push_back(deal_json(deal));
            } else {
                print_deal(deal);
            }
        }
        if (args.flag("json")) {
            std::cout << result.dump(2) << "\n";
        }
        return 0;
    }
    if (command == "balance") {
        const std::string account = require("account");
        if (account.empty()) {
            return fail("missing --account");
        }
        std::cout << account << " "
                  << escrow::format_money(book.balance(account)) << "\n";
        return 0;
    }
    if (command == "events") {
        std::ifstream input(config.event_log_path);
        if (!input) {
            return fail("cannot open event log: " + config.event_log_path);
        }
        std::uint64_t filter = 0;
        bool has_filter = false;
        if (args.has("id")) {
            try {
                filter = std::stoull(args.get("id"));
                has_filter = true;
            } catch (...) {
                return fail("invalid --id");
            }
        }
        std::string line;
        while (std::getline(input, line)) {
            if (line.empty()) {
                continue;
            }
            try {
                const auto event = nlohmann::json::parse(line);
                if (has_filter && event.value("deal_id", 0ull) != filter) {
                    continue;
                }
                std::cout << event.dump() << "\n";
            } catch (const std::exception&) {
                // Keep the log readable even if one line is corrupted.
            }
        }
        return 0;
    }

    print_usage();
    return 2;
}
