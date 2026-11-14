// 2026-11-13
// Standalone escrow tests (no external test framework).

#include "escrow/escrow.hpp"

#include <filesystem>
#include <iostream>
#include <string>

namespace {

namespace fs = std::filesystem;

#define CHECK(condition)                                                     \
    do {                                                                     \
        if (!(condition)) {                                                  \
            std::cerr << __FILE__ << ":" << __LINE__                         \
                      << ": CHECK failed: " #condition "\n";                 \
            return false;                                                    \
        }                                                                    \
    } while (false)

fs::path make_root() {
    static int counter = 0;
    const fs::path path =
        fs::temp_directory_path() /
        ("mimita-escrow-test-" + std::to_string(escrow::now_unix()) + "-" +
         std::to_string(counter++));
    fs::create_directories(path);
    return path;
}

escrow::Config config_for(const fs::path& root) {
    escrow::Config config;
    config.store_path = (root / "state.json").string();
    config.event_log_path = (root / "events.jsonl").string();
    config.admin = "admin";
    config.fee_account = "fees";
    config.default_currency = "USD";
    config.default_fee_bps = 100;
    config.max_fee_bps = 1000;
    return config;
}

bool load_book(const fs::path& root, escrow::Escrow* book) {
    const escrow::Result result =
        escrow::Escrow::load(config_for(root), book);
    if (!result) {
        std::cerr << "load failed: " << result.error << "\n";
        return false;
    }
    return true;
}

bool test_money() {
    CHECK(escrow::parse_money("10.5") == 1050);
    CHECK(escrow::parse_money("$1.05") == 105);
    CHECK(escrow::parse_money("0.01") == 1);
    CHECK(escrow::parse_money("12") == 1200);
    CHECK(!escrow::parse_money("abc").has_value());
    CHECK(!escrow::parse_money("1.001").has_value());
    CHECK(escrow::format_money(1050) == "10.50");
    CHECK(escrow::format_money(1) == "0.01");
    CHECK(escrow::format_money(0) == "0.00");
    return true;
}

bool test_time() {
    const auto parsed = escrow::parse_iso8601_utc("2026-01-02T03:04:05Z");
    CHECK(parsed.has_value());
    CHECK(escrow::format_iso8601_utc(*parsed) == "2026-01-02T03:04:05Z");
    CHECK(!escrow::parse_iso8601_utc("not-a-date").has_value());
    return true;
}

bool test_happy_path() {
    const fs::path root = make_root();
    escrow::Escrow book;
    CHECK(load_book(root, &book));
    CHECK(book.deposit("alice", 10000));
    CHECK(book.balance("alice") == 10000);

    std::uint64_t id = 0;
    CHECK(book.create("alice", "bob", "carol", 2500, "USD", 0, 250, "trade",
                      &id));
    CHECK(id == 1);
    CHECK(book.fund(id, "alice", "fund-1"));
    CHECK(book.balance("alice") == 7500);
    CHECK(book.release(id, "carol", "release-1"));

    const auto deal = book.find(id);
    CHECK(deal.has_value());
    CHECK(deal->state == "released");
    CHECK(deal->fee_cents == 62);
    CHECK(book.balance("bob") == 2438);
    CHECK(book.balance("fees") == 62);
    CHECK(book.balance("alice") == 7500);
    fs::remove_all(root);
    return true;
}

bool test_unauthorized() {
    const fs::path root = make_root();
    escrow::Escrow book;
    CHECK(load_book(root, &book));
    CHECK(book.deposit("alice", 5000));
    std::uint64_t id = 0;
    CHECK(book.create("alice", "bob", "carol", 1000, "USD", 0, 100, "", &id));
    CHECK(book.fund(id, "alice", ""));
    CHECK(!book.release(id, "alice", ""));
    CHECK(!book.release(id, "bob", ""));
    CHECK(!book.refund(id, "bob", ""));
    CHECK(book.balance("alice") == 4000);
    fs::remove_all(root);
    return true;
}

bool test_refund() {
    const fs::path root = make_root();
    escrow::Escrow book;
    CHECK(load_book(root, &book));
    CHECK(book.deposit("alice", 5000));
    std::uint64_t id = 0;
    CHECK(book.create("alice", "bob", "carol", 2000, "USD", 0, 100, "", &id));
    CHECK(book.fund(id, "alice", ""));
    CHECK(book.refund(id, "admin", "refund-1"));
    CHECK(book.balance("alice") == 5000);
    CHECK(book.balance("bob") == 0);
    CHECK(book.balance("fees") == 0);
    const auto deal = book.find(id);
    CHECK(deal.has_value() && deal->state == "refunded");
    fs::remove_all(root);
    return true;
}

bool test_expire() {
    const fs::path root = make_root();
    escrow::Escrow book;
    CHECK(load_book(root, &book));
    CHECK(book.deposit("alice", 3000));
    const std::int64_t deadline = escrow::now_unix() + 60;
    std::uint64_t id = 0;
    CHECK(book.create("alice", "bob", "carol", 1000, "USD", deadline, 100, "",
                      &id));
    CHECK(book.fund(id, "alice", ""));
    CHECK(book.expire(id, deadline + 60, "expire-1"));
    CHECK(book.balance("alice") == 3000);
    const auto deal = book.find(id);
    CHECK(deal.has_value() && deal->state == "expired");
    fs::remove_all(root);
    return true;
}

bool test_idempotency() {
    const fs::path root = make_root();
    escrow::Escrow book;
    CHECK(load_book(root, &book));
    CHECK(book.deposit("alice", 1000));
    std::uint64_t id = 0;
    CHECK(book.create("alice", "bob", "carol", 400, "USD", 0, 100, "", &id));
    CHECK(book.fund(id, "alice", "same-op"));
    CHECK(book.fund(id, "alice", "same-op"));
    CHECK(book.balance("alice") == 600);
    fs::remove_all(root);
    return true;
}

bool test_pause() {
    const fs::path root = make_root();
    escrow::Escrow book;
    CHECK(load_book(root, &book));
    CHECK(book.deposit("alice", 1000));
    std::uint64_t id = 0;
    CHECK(book.create("alice", "bob", "carol", 400, "USD", 0, 100, "", &id));
    CHECK(book.pause("admin", true));
    CHECK(!book.fund(id, "alice", ""));
    CHECK(book.pause("admin", false));
    CHECK(book.fund(id, "alice", ""));
    fs::remove_all(root);
    return true;
}

bool test_insufficient_balance() {
    const fs::path root = make_root();
    escrow::Escrow book;
    CHECK(load_book(root, &book));
    CHECK(book.deposit("alice", 100));
    std::uint64_t id = 0;
    CHECK(book.create("alice", "bob", "carol", 200, "USD", 0, 100, "", &id));
    CHECK(!book.fund(id, "alice", ""));
    CHECK(book.balance("alice") == 100);
    const auto deal = book.find(id);
    CHECK(deal.has_value() && deal->state == "created");
    fs::remove_all(root);
    return true;
}

bool test_reload() {
    const fs::path root = make_root();
    escrow::Escrow book;
    CHECK(load_book(root, &book));
    CHECK(book.deposit("alice", 5000));
    std::uint64_t id = 0;
    CHECK(book.create("alice", "bob", "carol", 1200, "EUR", 0, 50, "keep",
                      &id));
    CHECK(book.fund(id, "alice", ""));

    escrow::Escrow reloaded;
    CHECK(load_book(root, &reloaded));
    CHECK(reloaded.balance("alice") == 3800);
    CHECK(reloaded.next_deal_id() == 2);
    const auto deal = reloaded.find(id);
    CHECK(deal.has_value());
    CHECK(deal->state == "funded");
    CHECK(deal->currency == "EUR");
    CHECK(deal->memo == "keep");
    fs::remove_all(root);
    return true;
}

}  // namespace

int main() {
    struct Test {
        const char* name;
        bool (*run)();
    };
    const Test tests[] = {
        {"money", test_money},
        {"time", test_time},
        {"happy_path", test_happy_path},
        {"unauthorized", test_unauthorized},
        {"refund", test_refund},
        {"expire", test_expire},
        {"idempotency", test_idempotency},
        {"pause", test_pause},
        {"insufficient_balance", test_insufficient_balance},
        {"reload", test_reload},
    };
    int failures = 0;
    for (const auto& test : tests) {
        if (test.run()) {
            std::cout << "PASS " << test.name << "\n";
        } else {
            std::cout << "FAIL " << test.name << "\n";
            ++failures;
        }
    }
    return failures == 0 ? 0 : 1;
}
