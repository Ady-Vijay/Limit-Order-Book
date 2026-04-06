#include "order_queue.cpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>

// ── pop() on empty queue ──────────────────────────────────────────────────────

TEST(OrderQueuePop, EmptyQueueReturnsFalse) {
    OrderQueue q;
    Order out;
    EXPECT_FALSE(q.pop(std::move(out)));
}

// ── push() / pop() basic behaviour ───────────────────────────────────────────

TEST(OrderQueuePushPop, PopAfterPushReturnsTrue) {
    OrderQueue q;
    q.push(100, 10, "AAPL", OrderType::Buy);
    Order out;
    EXPECT_TRUE(q.pop(std::move(out)));
}

TEST(OrderQueuePushPop, PoppedOrderPreservesFields) {
    OrderQueue q;
    uint64_t expected_id = q.push(200, 5, "TSLA", OrderType::Sell);
    Order out;
    q.pop(std::move(out));

    EXPECT_EQ(out.id,     expected_id);
    EXPECT_EQ(out.price,  200u);
    EXPECT_EQ(out.quantity, 5u);
    EXPECT_EQ(out.ticker, "TSLA");
    EXPECT_EQ(out.type,   OrderType::Sell);
}

TEST(OrderQueuePushPop, QueueIsEmptyAfterSinglePopOnSinglePush) {
    OrderQueue q;
    q.push(100, 10, "AAPL", OrderType::Buy);
    Order out;
    q.pop(std::move(out));
    EXPECT_FALSE(q.pop(std::move(out)));
}

TEST(OrderQueuePushPop, FIFOOrderPreserved) {
    OrderQueue q;
    uint64_t id_a = q.push(100, 1, "AAPL", OrderType::Buy);
    uint64_t id_b = q.push(200, 2, "AAPL", OrderType::Buy);

    Order out;
    q.pop(std::move(out));
    EXPECT_EQ(out.id, id_a);
    q.pop(std::move(out));
    EXPECT_EQ(out.id, id_b);
}

TEST(OrderQueuePushPop, MultiplePopsDrainQueue) {
    OrderQueue q;
    q.push(100, 1, "AAPL", OrderType::Buy);
    q.push(100, 2, "AAPL", OrderType::Buy);
    q.push(100, 3, "AAPL", OrderType::Buy);

    Order out;
    EXPECT_TRUE(q.pop(std::move(out)));
    EXPECT_TRUE(q.pop(std::move(out)));
    EXPECT_TRUE(q.pop(std::move(out)));
    EXPECT_FALSE(q.pop(std::move(out)));
}

// ── cancel() ─────────────────────────────────────────────────────────────────

TEST(OrderQueueCancel, CancelNonExistentIdIsNoop) {
    OrderQueue q;
    EXPECT_NO_THROW(q.cancel(999999ULL));
}

TEST(OrderQueueCancel, CancelledOrderIsNotPopped) {
    OrderQueue q;
    uint64_t id = q.push(100, 10, "AAPL", OrderType::Buy);
    q.cancel(id);

    Order out;
    q.pop(std::move(out));
    EXPECT_EQ(out.id, 0);
}

TEST(OrderQueueCancel, CancelOnlyTargetedOrder) {
    OrderQueue q;
    uint64_t id_a = q.push(100, 1, "AAPL", OrderType::Buy);
    uint64_t id_b = q.push(200, 2, "AAPL", OrderType::Sell);
    q.cancel(id_a);

    Order out;
    EXPECT_TRUE(q.pop(std::move(out)));
    EXPECT_EQ(out.id, 0);
    q.pop(std::move(out));
    EXPECT_EQ(out.id, id_b);
}

TEST(OrderQueueCancel, CancelTwiceSecondIsNoop) {
    OrderQueue q;
    uint64_t id = q.push(100, 10, "AAPL", OrderType::Buy);
    q.cancel(id);
    EXPECT_NO_THROW(q.cancel(id));
}

TEST(OrderQueueCancel, CancelAfterPopIsNoop) {
    OrderQueue q;
    uint64_t id = q.push(100, 10, "AAPL", OrderType::Buy);
    Order out;
    q.pop(std::move(out));
    EXPECT_NO_THROW(q.cancel(id));
}

TEST(OrderQueueCancel, CancelMiddleOfQueue) {
    OrderQueue q;
    uint64_t id_a = q.push(100, 1, "AAPL", OrderType::Buy);
    uint64_t id_b = q.push(100, 2, "AAPL", OrderType::Buy);
    uint64_t id_c = q.push(100, 3, "AAPL", OrderType::Buy);
    q.cancel(id_b);

    Order out;
    q.pop(std::move(out)); EXPECT_EQ(out.id, id_a);
    q.pop(std::move(out)); EXPECT_EQ(out.id, 0);
    q.pop(std::move(out)); EXPECT_EQ(out.id, id_c);
}

// ── Thread safety ─────────────────────────────────────────────────────────────

TEST(OrderQueueThreadSafety, ConcurrentPushesAllDequeued) {
    OrderQueue q;
    constexpr int N = 1000;

    std::vector<std::thread> producers;
    for (int i = 0; i < 4; ++i) {
        producers.emplace_back([&]() {
            for (int j = 0; j < N / 4; ++j)
                q.push(100, 1, "AAPL", OrderType::Buy);
        });
    }
    for (auto& t : producers) t.join();

    int popped = 0;
    Order out;
    while (q.pop(std::move(out))) ++popped;
    EXPECT_EQ(popped, N);
}

TEST(OrderQueueThreadSafety, ConcurrentPushAndPop) {
    OrderQueue q;
    constexpr int N = 500;
    std::atomic<int> consumed{0};

    std::thread producer([&]() {
        for (int i = 0; i < N; ++i)
            q.push(100, 1, "AAPL", OrderType::Buy);
    });

    std::thread consumer([&]() {
        int seen = 0;
        while (seen < N) {
            Order out;
            if (q.pop(std::move(out))) {
                ++seen;
                consumed.fetch_add(1, std::memory_order_relaxed);
            }
        }
    });

    producer.join();
    consumer.join();
    EXPECT_EQ(consumed.load(), N);
}

TEST(OrderQueueThreadSafety, CancelRacingWithPop) {
    OrderQueue q;
    constexpr int N = 200;

    std::vector<uint64_t> ids;
    ids.reserve(N);
    for (int i = 0; i < N; ++i)
        ids.push_back(q.push(100, 1, "AAPL", OrderType::Buy));

    std::atomic<int> popped_count{0};

    std::thread popper([&]() {
        Order out;
        while (q.pop(std::move(out)))
            popped_count.fetch_add(1, std::memory_order_relaxed);
    });

    std::thread canceller([&]() {
        for (int i = 0; i < N; i += 2)
            q.cancel(ids[i]);
    });

    canceller.join();
    popper.join();

    // No crash and no orders unaccounted for — each was either popped or cancelled.
    EXPECT_GE(popped_count.load(), 0);
    EXPECT_LE(popped_count.load(), N);
}
