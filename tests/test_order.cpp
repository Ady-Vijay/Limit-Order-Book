#include <gtest/gtest.h>
#include "order.h"

// ── Construction ─────────────────────────────────────────────────────────────

TEST(OrderTest, FieldsSetCorrectly) {
    Order o(150, 30, "AAPL", OrderType::Buy);
    EXPECT_EQ(o.price,    150u);
    EXPECT_EQ(o.quantity, 30u);
    EXPECT_EQ(o.ticker,   "AAPL");
    EXPECT_EQ(o.type,     OrderType::Buy);
}

TEST(OrderTest, SellTypeSetCorrectly) {
    Order o(200, 5, "GOOG", OrderType::Sell);
    EXPECT_EQ(o.type, OrderType::Sell);
}

// ── ID assignment ─────────────────────────────────────────────────────────────

TEST(OrderTest, IdsAreUnique) {
    Order o1(100, 1, "AAPL", OrderType::Buy);
    Order o2(100, 1, "AAPL", OrderType::Buy);
    EXPECT_NE(o1.id, o2.id);
}

TEST(OrderTest, IdsAreMonotonicallyIncreasing) {
    Order o1(100, 1, "AAPL", OrderType::Buy);
    Order o2(100, 1, "AAPL", OrderType::Buy);
    EXPECT_LT(o1.id, o2.id);
}

// ── Timestamp ─────────────────────────────────────────────────────────────────

TEST(OrderTest, TimestampIsPositive) {
    Order o(100, 10, "TSLA", OrderType::Buy);
    EXPECT_GT(o.timestamp, 0);
}

TEST(OrderTest, LaterOrderHasLaterTimestamp) {
    Order o1(100, 1, "AAPL", OrderType::Buy);
    Order o2(100, 1, "AAPL", OrderType::Buy);
    // Timestamps are nanosecond-resolution; o2 must be >= o1
    EXPECT_GE(o2.timestamp, o1.timestamp);
}

// ── Move semantics ────────────────────────────────────────────────────────────

TEST(OrderTest, MoveConstructorPreservesFields) {
    Order o1(300, 75, "MSFT", OrderType::Sell);
    uint64_t expected_id    = o1.id;
    int64_t  expected_ts    = o1.timestamp;
    uint64_t expected_price = o1.price;

    Order o2(std::move(o1));
    EXPECT_EQ(o2.id,        expected_id);
    EXPECT_EQ(o2.timestamp, expected_ts);
    EXPECT_EQ(o2.price,     expected_price);
    EXPECT_EQ(o2.ticker,    "MSFT");
    EXPECT_EQ(o2.type,      OrderType::Sell);
}

TEST(OrderTest, MoveAssignmentPreservesFields) {
    Order o1(400, 10, "NVDA", OrderType::Buy);
    uint64_t expected_id = o1.id;

    Order o2(1, 1, "X", OrderType::Sell);
    o2 = std::move(o1);
    EXPECT_EQ(o2.id,     expected_id);
    EXPECT_EQ(o2.price,  400u);
    EXPECT_EQ(o2.ticker, "NVDA");
}

// Copy construction and assignment must be deleted (compile-time guarantees;
// verified via type trait so we don't need to instantiate them):
TEST(OrderTest, IsCopyConstructibleFalse) {
    EXPECT_FALSE(std::is_copy_constructible<Order>::value);
}

TEST(OrderTest, IsCopyAssignableFalse) {
    EXPECT_FALSE(std::is_copy_assignable<Order>::value);
}
