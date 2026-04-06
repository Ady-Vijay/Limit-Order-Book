#include "order_book.h"
#include "order_queue.h"
#include "order.h"
#include <atomic>

class MatchingEngine {
private:
    OrderQueue& order_queue;
    OrderBook order_book;
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
                order_book.cancel(id);

            Order order;
            if (order_queue.pop(std::move(order))) {
                if(!to_cancel.count(order.id)){
                    order_book.match(std::move(order));
                }
            }
        }
    }

    const std::vector<Trade>& trades() const { return order_book.trades; }
};
