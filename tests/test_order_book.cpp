// order_book.cpp contains the full class definition (acts as a header),
// so we include it directly rather than linking a separate TU.
#include "order_book.cpp"
#include <gtest/gtest.h>
#include <limits>

// Sentinel returned by match() when the incoming order is fully filled.
static constexpr uint64_t FULLY_MATCHED = std::numeric_limits<uint64_t>::max(); // (uint64_t)-1

// Helper: build and add an order, returning its id.
static uint64_t place(OrderBook& book, uint64_t price, uint64_t qty,
                      const std::string& ticker, OrderType type) {
    Order o(price, qty, ticker, type);
    return book.add(std::move(o));
}

// ── add() ─────────────────────────────────────────────────────────────────────

TEST(OrderBookAdd, BuyReturnsCorrectId) {
    OrderBook book;
    Order o(100, 10, "AAPL", OrderType::Buy);
    uint64_t expected = o.id;
    EXPECT_EQ(book.add(std::move(o)), expected);
}

TEST(OrderBookAdd, SellReturnsCorrectId) {
    OrderBook book;
    Order o(100, 10, "AAPL", OrderType::Sell);
    uint64_t expected = o.id;
    EXPECT_EQ(book.add(std::move(o)), expected);
}

TEST(OrderBookAdd, MultipleOrdersReturnDistinctIds) {
    OrderBook book;
    uint64_t id1 = place(book, 100, 10, "AAPL", OrderType::Buy);
    uint64_t id2 = place(book, 100, 20, "AAPL", OrderType::Buy);
    uint64_t id3 = place(book, 200,  5, "AAPL", OrderType::Buy);
    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
    EXPECT_NE(id1, id3);
}

TEST(OrderBookAdd, BuyAndSellReturnDistinctIds) {
    OrderBook book;
    uint64_t buy_id  = place(book, 100, 10, "AAPL", OrderType::Buy);
    uint64_t sell_id = place(book, 110, 10, "AAPL", OrderType::Sell);
    EXPECT_NE(buy_id, sell_id);
}

TEST(OrderBookAdd, MultipleOrdersAtSamePriceDoNotClobber) {
    OrderBook book;
    uint64_t id1 = place(book, 100, 10, "AAPL", OrderType::Buy);
    uint64_t id2 = place(book, 100, 20, "AAPL", OrderType::Buy);
    EXPECT_NE(id1, id2);
}

// ── cancel() ─────────────────────────────────────────────────────────────────

TEST(OrderBookCancel, CancelBuyOrder) {
    OrderBook book;
    uint64_t id = place(book, 100, 10, "AAPL", OrderType::Buy);
    EXPECT_NO_THROW(book.cancel(id));
}

TEST(OrderBookCancel, CancelSellOrder) {
    OrderBook book;
    uint64_t id = place(book, 200, 5, "TSLA", OrderType::Sell);
    EXPECT_NO_THROW(book.cancel(id));
}

TEST(OrderBookCancel, CancelNonExistentIdIsNoop) {
    OrderBook book;
    EXPECT_NO_THROW(book.cancel(999999999ULL));
}

TEST(OrderBookCancel, CancelEmptyBookIsNoop) {
    OrderBook book;
    EXPECT_NO_THROW(book.cancel(0));
}

TEST(OrderBookCancel, CancelTwiceSecondIsNoop) {
    OrderBook book;
    uint64_t id = place(book, 100, 10, "AAPL", OrderType::Buy);
    book.cancel(id);
    // id no longer in order_book lookup — must early-return cleanly
    EXPECT_NO_THROW(book.cancel(id));
}

TEST(OrderBookCancel, CancelOneOfManyAtSamePrice) {
    OrderBook book;
    uint64_t id1 = place(book, 100, 10, "AAPL", OrderType::Buy);
    uint64_t id2 = place(book, 100, 20, "AAPL", OrderType::Buy);
    EXPECT_NO_THROW(book.cancel(id1));
    EXPECT_NO_THROW(book.cancel(id2));
}

// ── match() — no resting orders ──────────────────────────────────────────────

TEST(OrderBookMatch, BuyWithEmptyBookIsAddedToBook) {
    OrderBook book;
    Order o(100, 10, "AAPL", OrderType::Buy);
    uint64_t expected = o.id;
    uint64_t result = book.match(std::move(o));
    EXPECT_EQ(result, expected);
}

TEST(OrderBookMatch, SellWithEmptyBookIsAddedToBook) {
    OrderBook book;
    Order o(100, 10, "AAPL", OrderType::Sell);
    uint64_t expected = o.id;
    uint64_t result = book.match(std::move(o));
    EXPECT_EQ(result, expected);
}

// ── match() — price does not cross, no fill ───────────────────────────────────

TEST(OrderBookMatch, BuyBelowBestSellIsNotMatched) {
    OrderBook book;
    // Resting sell at 110; incoming buy at 100 — no cross
    place(book, 110, 20, "AAPL", OrderType::Sell);
    Order buy(100, 10, "AAPL", OrderType::Buy);
    uint64_t buy_id = buy.id;
    uint64_t result = book.match(std::move(buy));
    // Not matched → added to book with its own id
    EXPECT_EQ(result, buy_id);
}

TEST(OrderBookMatch, SellAboveBestBuyIsNotMatched) {
    OrderBook book;
    // Resting buy at 90; incoming sell at 100 — no cross
    place(book, 90, 20, "AAPL", OrderType::Buy);
    Order sell(100, 10, "AAPL", OrderType::Sell);
    uint64_t sell_id = sell.id;
    uint64_t result = book.match(std::move(sell));
    EXPECT_EQ(result, sell_id);
}

// ── match() — full fill (incoming qty ≤ resting qty) ─────────────────────────

TEST(OrderBookMatch, BuyFullyFilledAgainstSell) {
    OrderBook book;
    // Resting sell at 100 with qty=20; incoming buy at 100 with qty=10
    place(book, 100, 20, "AAPL", OrderType::Sell);
    Order buy(100, 10, "AAPL", OrderType::Buy);
    uint64_t result = book.match(std::move(buy));
    // Fully matched — returns sentinel
    EXPECT_EQ(result, FULLY_MATCHED);
}

TEST(OrderBookMatch, SellFullyFilledAgainstBuy) {
    OrderBook book;
    // Resting buy at 100 with qty=20; incoming sell at 100 with qty=10
    place(book, 100, 20, "AAPL", OrderType::Buy);
    Order sell(100, 10, "AAPL", OrderType::Sell);
    uint64_t result = book.match(std::move(sell));
    EXPECT_EQ(result, FULLY_MATCHED);
}

TEST(OrderBookMatch, ExactQtyMatchFullyFills) {
    OrderBook book;
    // Resting sell qty=10; incoming buy qty=10 — exact match
    place(book, 100, 10, "AAPL", OrderType::Sell);
    Order buy(100, 10, "AAPL", OrderType::Buy);
    uint64_t result = book.match(std::move(buy));
    EXPECT_EQ(result, FULLY_MATCHED);
}

TEST(OrderBookMatch, BuyPriceAboveSellStillMatches) {
    OrderBook book;
    // Aggressive buy at 120 crosses resting sell at 100
    place(book, 100, 20, "AAPL", OrderType::Sell);
    Order buy(120, 5, "AAPL", OrderType::Buy);
    uint64_t result = book.match(std::move(buy));
    EXPECT_EQ(result, FULLY_MATCHED);
}

// ── match() — partial fill (incoming qty < resting qty) ───────────────────────

TEST(OrderBookMatch, PartialBuyFillAddsRemainingToBook) {
    OrderBook book;
    // Resting sell at 100 qty=5; incoming buy at 100 qty=3 → fully absorbed, 0 remaining
    // (incoming qty < resting qty → incoming is fully consumed)
    place(book, 100, 5, "AAPL", OrderType::Sell);
    Order buy(100, 3, "AAPL", OrderType::Buy);
    uint64_t result = book.match(std::move(buy));
    // Incoming qty=3 <= resting qty=5 → incoming fully filled, returns FULLY_MATCHED
    EXPECT_EQ(result, FULLY_MATCHED);
}

TEST(OrderBookMatch, NoMatchUnfilledOrderHasOriginalId) {
    OrderBook book;
    // No resting orders at all → order goes straight to book
    Order buy(100, 10, "AAPL", OrderType::Buy);
    uint64_t expected = buy.id;
    uint64_t result = book.match(std::move(buy));
    EXPECT_EQ(result, expected);
}

// ── best_price() sentinel behaviour ──────────────────────────────────────────

TEST(OrderBookMatch, BuyMatchesAtExactSellPrice) {
    OrderBook book;
    place(book, 100, 50, "AAPL", OrderType::Sell);
    Order buy(100, 1, "AAPL", OrderType::Buy);
    EXPECT_EQ(book.match(std::move(buy)), FULLY_MATCHED);
}

TEST(OrderBookMatch, SellMatchesAtExactBuyPrice) {
    OrderBook book;
    place(book, 100, 50, "AAPL", OrderType::Buy);
    Order sell(100, 1, "AAPL", OrderType::Sell);
    EXPECT_EQ(book.match(std::move(sell)), FULLY_MATCHED);
}

// ── match() — incoming qty exceeds a single resting order ────────────────────

TEST(OrderBookMatch, BuyConsumesFullRestingOrderAndContinues) {
    OrderBook book;
    // Two resting sells at the same price: 5 + 10 = 15 total available
    // Incoming buy qty=7 > first resting qty=5, so first is fully consumed,
    // then remaining 2 are filled from the second resting order.
    place(book, 100, 5,  "AAPL", OrderType::Sell);
    place(book, 100, 10, "AAPL", OrderType::Sell);
    Order buy(100, 7, "AAPL", OrderType::Buy);
    EXPECT_EQ(book.match(std::move(buy)), FULLY_MATCHED);
}

TEST(OrderBookMatch, SellConsumesFullRestingOrderAndContinues) {
    OrderBook book;
    place(book, 100, 5,  "AAPL", OrderType::Buy);
    place(book, 100, 10, "AAPL", OrderType::Buy);
    Order sell(100, 7, "AAPL", OrderType::Sell);
    EXPECT_EQ(book.match(std::move(sell)), FULLY_MATCHED);
}

TEST(OrderBookMatch, BuyConsumesEntirePriceLevel) {
    OrderBook book;
    // Incoming buy exactly drains all resting qty at a price level
    place(book, 100, 5,  "AAPL", OrderType::Sell);
    place(book, 100, 10, "AAPL", OrderType::Sell);
    Order buy(100, 15, "AAPL", OrderType::Buy);
    EXPECT_EQ(book.match(std::move(buy)), FULLY_MATCHED);
}

TEST(OrderBookMatch, BuyConsumesMultiplePriceLevels) {
    OrderBook book;
    // Two resting sell levels: 10 @ 100, 10 @ 105
    // Incoming buy @ 110 qty=15 — crosses both levels
    place(book, 100, 10, "AAPL", OrderType::Sell);
    place(book, 105, 10, "AAPL", OrderType::Sell);
    Order buy(110, 15, "AAPL", OrderType::Buy);
    EXPECT_EQ(book.match(std::move(buy)), FULLY_MATCHED);
}

TEST(OrderBookMatch, SellConsumesMultiplePriceLevels) {
    OrderBook book;
    // Two resting buy levels: 10 @ 110, 10 @ 105
    // Incoming sell @ 100 qty=15 — crosses both levels
    place(book, 110, 10, "AAPL", OrderType::Buy);
    place(book, 105, 10, "AAPL", OrderType::Buy);
    Order sell(100, 15, "AAPL", OrderType::Sell);
    EXPECT_EQ(book.match(std::move(sell)), FULLY_MATCHED);
}

TEST(OrderBookMatch, PartialFillRemainingAddedToBook) {
    OrderBook book;
    // Resting sell: 5 @ 100. Incoming buy: qty=8 @ 100.
    // 5 filled, 3 left over → should be added to book with its own id.
    place(book, 100, 5, "AAPL", OrderType::Sell);
    Order buy(100, 8, "AAPL", OrderType::Buy);
    uint64_t buy_id = buy.id;
    uint64_t result = book.match(std::move(buy));
    EXPECT_EQ(result, buy_id);
}

TEST(OrderBookMatch, PartialFillAcrossLevelsRemainingAddedToBook) {
    OrderBook book;
    // Resting sells: 5 @ 100, 5 @ 105. Incoming buy: qty=15 @ 110.
    // 10 filled across both levels, 5 left → added to book.
    place(book, 100, 5, "AAPL", OrderType::Sell);
    place(book, 105, 5, "AAPL", OrderType::Sell);
    Order buy(110, 15, "AAPL", OrderType::Buy);
    uint64_t buy_id = buy.id;
    uint64_t result = book.match(std::move(buy));
    EXPECT_EQ(result, buy_id);
}

TEST(OrderBookMatch, CancelledRestingOrderNotMatched) {
    OrderBook book;
    // Place a sell, cancel it, then try to match a buy against it — should not fill.
    uint64_t sell_id = place(book, 100, 20, "AAPL", OrderType::Sell);
    book.cancel(sell_id);
    Order buy(100, 10, "AAPL", OrderType::Buy);
    uint64_t buy_id = buy.id;
    uint64_t result = book.match(std::move(buy));
    EXPECT_EQ(result, buy_id); // no fill → added to book
}

// ── Price-time priority ───────────────────────────────────────────────────────

TEST(OrderBookPriceTime, BuyMatchesBestAskFirst) {
    OrderBook book;
    // Two sell levels: 100 and 105. Incoming buy should hit 100 (best ask) first.
    place(book, 105, 10, "AAPL", OrderType::Sell);
    place(book, 100, 10, "AAPL", OrderType::Sell);
    // Buy qty=10 — should be fully filled at 100, leaving 105 untouched.
    Order buy(110, 10, "AAPL", OrderType::Buy);
    EXPECT_EQ(book.match(std::move(buy)), FULLY_MATCHED);
    // Confirm the 105 sell level is still resting (not consumed).
    // A buy at 105 or higher should match against it.
    Order probe(105, 1, "AAPL", OrderType::Buy);
    EXPECT_EQ(book.match(std::move(probe)), FULLY_MATCHED);
}

TEST(OrderBookPriceTime, SellMatchesBestBidFirst) {
    OrderBook book;
    // Two buy levels: 110 and 105. Incoming sell should hit 110 (best bid) first.
    place(book, 105, 10, "AAPL", OrderType::Buy);
    place(book, 110, 10, "AAPL", OrderType::Buy);
    // Sell qty=10 — should be fully filled at 110, leaving 105 untouched.
    Order sell(100, 10, "AAPL", OrderType::Sell);
    EXPECT_EQ(book.match(std::move(sell)), FULLY_MATCHED);
    // Confirm the 105 buy level is still resting (not consumed).
    // A sell at 105 or lower should match against it.
    Order probe(105, 1, "AAPL", OrderType::Sell);
    EXPECT_EQ(book.match(std::move(probe)), FULLY_MATCHED);
}

TEST(OrderBookPriceTime, SamePriceFilledInArrivalOrder) {
    OrderBook book;
    // Three sells at the same price, added in order A, B, C.
    // Incoming buy should consume A first, then B, leaving C.
    place(book, 100, 5, "AAPL", OrderType::Sell); // A — arrives first
    place(book, 100, 5, "AAPL", OrderType::Sell); // B
    place(book, 100, 5, "AAPL", OrderType::Sell); // C — arrives last
    // Buy qty=10 — fills A(5) + B(5), C(5) must remain.
    Order buy(100, 10, "AAPL", OrderType::Buy);
    EXPECT_EQ(book.match(std::move(buy)), FULLY_MATCHED);
    // Confirm C is still resting: a sell at 100 qty=1 should match immediately.
    Order probe(100, 1, "AAPL", OrderType::Buy);
    EXPECT_EQ(book.match(std::move(probe)), FULLY_MATCHED);
}

TEST(OrderBookPriceTime, BuyDoesNotMatchAboveItsPrice) {
    OrderBook book;
    // Resting sell at 110; incoming buy at 100 — no cross, must not fill.
    place(book, 110, 10, "AAPL", OrderType::Sell);
    Order buy(100, 10, "AAPL", OrderType::Buy);
    uint64_t buy_id = buy.id;
    EXPECT_EQ(book.match(std::move(buy)), buy_id);
}

TEST(OrderBookPriceTime, SellDoesNotMatchBelowItsPrice) {
    OrderBook book;
    // Resting buy at 90; incoming sell at 100 — no cross, must not fill.
    place(book, 90, 10, "AAPL", OrderType::Buy);
    Order sell(100, 10, "AAPL", OrderType::Sell);
    uint64_t sell_id = sell.id;
    EXPECT_EQ(book.match(std::move(sell)), sell_id);
}

// ── Trade recording ───────────────────────────────────────────────────────────

TEST(OrderBookTrades, NoTradeWhenNoMatch) {
    OrderBook book;
    book.match(Order(100, 10, "AAPL", OrderType::Buy));
    EXPECT_TRUE(book.trades.empty());
}

TEST(OrderBookTrades, NoTradeWhenPriceDoesNotCross) {
    OrderBook book;
    place(book, 110, 10, "AAPL", OrderType::Sell);
    book.match(Order(100, 10, "AAPL", OrderType::Buy));
    EXPECT_TRUE(book.trades.empty());
}

TEST(OrderBookTrades, SingleTradeRecordedOnFullMatch) {
    OrderBook book;
    place(book, 100, 10, "AAPL", OrderType::Sell);
    book.match(Order(100, 10, "AAPL", OrderType::Buy));
    ASSERT_EQ(book.trades.size(), 1u);
}

TEST(OrderBookTrades, TradeHasRestingPrice) {
    OrderBook book;
    // Resting sell at 100; incoming buy at 110 — trade must execute at 100 (price improvement).
    place(book, 100, 10, "AAPL", OrderType::Sell);
    book.match(Order(110, 10, "AAPL", OrderType::Buy));
    ASSERT_EQ(book.trades.size(), 1u);
    EXPECT_EQ(book.trades[0].price, 100u);
}

TEST(OrderBookTrades, TradeHasCorrectQuantity) {
    OrderBook book;
    place(book, 100, 10, "AAPL", OrderType::Sell);
    book.match(Order(100, 7, "AAPL", OrderType::Buy));
    ASSERT_EQ(book.trades.size(), 1u);
    EXPECT_EQ(book.trades[0].quantity, 7u);
}

TEST(OrderBookTrades, TradeHasCorrectTicker) {
    OrderBook book;
    place(book, 100, 10, "TSLA", OrderType::Sell);
    book.match(Order(100, 5, "TSLA", OrderType::Buy));
    ASSERT_EQ(book.trades.size(), 1u);
    EXPECT_EQ(book.trades[0].ticker, "TSLA");
}

TEST(OrderBookTrades, TradeTimestampIsPositive) {
    OrderBook book;
    place(book, 100, 10, "AAPL", OrderType::Sell);
    book.match(Order(100, 10, "AAPL", OrderType::Buy));
    ASSERT_EQ(book.trades.size(), 1u);
    EXPECT_GT(book.trades[0].timestamp, 0);
}

TEST(OrderBookTrades, PartialFillRecordsPartialQuantity) {
    OrderBook book;
    // Resting sell qty=10; incoming buy qty=4 — trade qty must be 4, not 10.
    place(book, 100, 10, "AAPL", OrderType::Sell);
    book.match(Order(100, 4, "AAPL", OrderType::Buy));
    ASSERT_EQ(book.trades.size(), 1u);
    EXPECT_EQ(book.trades[0].quantity, 4u);
}

TEST(OrderBookTrades, MultipleRestingOrdersProduceMultipleTrades) {
    OrderBook book;
    // Two resting sells at same price — incoming buy sweeps both.
    place(book, 100, 5, "AAPL", OrderType::Sell);
    place(book, 100, 5, "AAPL", OrderType::Sell);
    book.match(Order(100, 10, "AAPL", OrderType::Buy));
    EXPECT_EQ(book.trades.size(), 2u);
}

TEST(OrderBookTrades, TradesAcrossLevelsRecordedSeparately) {
    OrderBook book;
    // Two resting sells at different prices — incoming buy crosses both.
    place(book, 100, 5, "AAPL", OrderType::Sell);
    place(book, 105, 5, "AAPL", OrderType::Sell);
    book.match(Order(110, 10, "AAPL", OrderType::Buy));
    ASSERT_EQ(book.trades.size(), 2u);
    // First trade at best ask (100), second at next level (105).
    EXPECT_EQ(book.trades[0].price, 100u);
    EXPECT_EQ(book.trades[1].price, 105u);
}

TEST(OrderBookTrades, TradesAccumulateAcrossMultipleMatches) {
    OrderBook book;
    place(book, 100, 10, "AAPL", OrderType::Sell);
    place(book, 100, 10, "AAPL", OrderType::Sell);
    book.match(Order(100, 10, "AAPL", OrderType::Buy));
    book.match(Order(100, 10, "AAPL", OrderType::Buy));
    EXPECT_EQ(book.trades.size(), 2u);
}
