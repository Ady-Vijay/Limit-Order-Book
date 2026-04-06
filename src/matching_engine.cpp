#include "order_book.h"
#include "order_queue.h"
#include "order.h"
#include <atomic>

class MatchingEngine {
private:
    OrderQueue& order_queue;
    std::unordered_map<std::string, OrderBook> order_books;
    std::unordered_map<uint64_t, std::string> order_id_to_ticker;
    std::atomic<bool> running{true};

public:
    MatchingEngine(OrderQueue& oq) : order_queue(oq) {}

    void stop() { running.store(false); }

    void run() {
        while (running.load()) {
            // Drain book-cancels first, before processing the next order,
            // so a cancel submitted before an order isn't missed.
            std::unordered_set<uint64_t> to_cancel;
            {
                std::lock_guard<std::mutex> cancel_lock(order_queue.cancel_mtx);
                to_cancel.swap(order_queue.cancelled_orders);
            }
            for (auto id : to_cancel)
                order_books[order_id_to_ticker[id]].cancel(id);

            Order order;
            if (order_queue.pop(std::move(order))) {
                order_id_to_ticker[order.id] = order.ticker;
                if(!to_cancel.count(order.id)){
                    order_books[order.ticker].match(std::move(order));
                }
            }
        }
    }

    const std::vector<Trade> trades() const {
        std::vector<Trade> all_trades;
        for (const auto& [ticker, book] : order_books) {
            all_trades.insert(all_trades.end(), book.trades.begin(), book.trades.end());
        }
        return all_trades;
    }
};
