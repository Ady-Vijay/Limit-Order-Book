#include "order_book.cpp"
#include "order_queue.cpp"
#include "matching_engine.cpp"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <limits>

static constexpr uint64_t FULLY_MATCHED = std::numeric_limits<uint64_t>::max();

// Runs the engine on a background thread, executes the provided lambda to
// submit work, then stops the engine and joins.
template<typename F>
static void run_with_engine(MatchingEngine& engine, F&& submit) {
    std::thread t([&]{ engine.run(); });
    submit();
    // Give the engine time to drain the queue before stopping.
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    engine.stop();
    t.join();
}

// ── Basic order processing ────────────────────────────────────────────────────

TEST(MatchingEngine, OrderWithNoCounterpartProducesNoTrade) {
    OrderQueue q;
    MatchingEngine engine(q);

    run_with_engine(engine, [&]{
        q.push(100, 10, "AAPL", OrderType::Buy);
    });

    EXPECT_TRUE(engine.trades().empty());
}

TEST(MatchingEngine, MatchingOrdersProduceTrade) {
    OrderQueue q;
    MatchingEngine engine(q);

    run_with_engine(engine, [&]{
        q.push(100, 10, "AAPL", OrderType::Sell);
        q.push(100, 10, "AAPL", OrderType::Buy);
    });

    ASSERT_EQ(engine.trades().size(), 1u);
    EXPECT_EQ(engine.trades()[0].quantity, 10u);
    EXPECT_EQ(engine.trades()[0].price,    100u);
}

TEST(MatchingEngine, PartialFillProducesTradeForFilledPortion) {
    OrderQueue q;
    MatchingEngine engine(q);

    run_with_engine(engine, [&]{
        q.push(100, 5,  "AAPL", OrderType::Sell);
        q.push(100, 10, "AAPL", OrderType::Buy);
    });

    ASSERT_EQ(engine.trades().size(), 1u);
    EXPECT_EQ(engine.trades()[0].quantity, 5u);
}

TEST(MatchingEngine, MultipleMatchesAccumulateTrades) {
    OrderQueue q;
    MatchingEngine engine(q);

    run_with_engine(engine, [&]{
        q.push(100, 10, "AAPL", OrderType::Sell);
        q.push(100, 10, "AAPL", OrderType::Sell);
        q.push(100, 10, "AAPL", OrderType::Buy);
        q.push(100, 10, "AAPL", OrderType::Buy);
    });

    EXPECT_EQ(engine.trades().size(), 2u);
}

TEST(MatchingEngine, PriceDoesNotCrossNoTrade) {
    OrderQueue q;
    MatchingEngine engine(q);

    run_with_engine(engine, [&]{
        q.push(110, 10, "AAPL", OrderType::Sell);
        q.push(100, 10, "AAPL", OrderType::Buy);
    });

    EXPECT_TRUE(engine.trades().empty());
}

// ── Cancel pending (still in queue) ──────────────────────────────────────────

TEST(MatchingEngine, CancelPendingOrderPreventsMatch) {
    OrderQueue q;
    MatchingEngine engine(q);

    // Push a sell then immediately cancel it before the engine processes it.
    uint64_t sell_id = q.push(100, 10, "AAPL", OrderType::Sell);
    q.cancel(sell_id);

    run_with_engine(engine, [&]{
        q.push(100, 10, "AAPL", OrderType::Buy);
    });

    // Sell was cancelled before processing — no match should occur.
    EXPECT_TRUE(engine.trades().empty());
}

TEST(MatchingEngine, CancelOneOfTwoPendingSellsPartialMatch) {
    OrderQueue q;
    MatchingEngine engine(q);

    uint64_t sell_id1 = q.push(100, 10, "AAPL", OrderType::Sell);
    q.push(100, 10, "AAPL", OrderType::Sell);
    q.cancel(sell_id1);

    run_with_engine(engine, [&]{
        q.push(100, 10, "AAPL", OrderType::Buy);
    });

    // Only one sell survives — exactly one trade.
    ASSERT_EQ(engine.trades().size(), 1u);
    EXPECT_EQ(engine.trades()[0].quantity, 10u);
}

// ── Stop / restart ────────────────────────────────────────────────────────────

TEST(MatchingEngine, StopHaltsRunLoop) {
    OrderQueue q;
    MatchingEngine engine(q);

    std::thread t([&]{ engine.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    engine.stop();
    t.join(); // must not hang
    SUCCEED();
}

// ── Concurrent producers ──────────────────────────────────────────────────────

TEST(MatchingEngine, ConcurrentProducersAllOrdersProcessed) {
    OrderQueue q;
    MatchingEngine engine(q);

    constexpr int N = 100;
    std::thread t([&]{ engine.run(); });

    // Two producer threads: N sells and N buys at the same price.
    std::thread seller([&]{
        for (int i = 0; i < N; ++i)
            q.push(100, 1, "AAPL", OrderType::Sell);
    });
    std::thread buyer([&]{
        for (int i = 0; i < N; ++i)
            q.push(100, 1, "AAPL", OrderType::Buy);
    });

    seller.join();
    buyer.join();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    engine.stop();
    t.join();

    EXPECT_EQ(engine.trades().size(), static_cast<size_t>(N));
}
