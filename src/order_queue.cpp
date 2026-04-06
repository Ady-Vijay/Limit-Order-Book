#include "order_queue.h"

uint64_t OrderQueue::push(uint64_t price, uint64_t quantity, std::string ticker, OrderType type) {
    Order order(price, quantity, std::move(ticker), type);
    queue.push_back(std::move(order));
    order_map[id] = std::prev(queue.end());
    return id;
}

bool OrderQueue::pop(Order&& item) {
    std::lock_guard<std::mutex> lock(mtx);
    if (queue.empty()) {
        return false;
    }
    Order order = std::move(queue.front());
    queue.pop_front();
    order_map.erase(order.id);
    item = std::move(order);
    return true;
}

void OrderQueue::cancel(uint64_t order_id) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = order_map.find(order_id);
    if (it != order_map.end()) {
        queue.erase(it->second);
        order_map.erase(it);
    } else {
        std::lock_guard<std::mutex> cancel_lock(cancel_mtx);
        cancelled_orders.insert(order_id);
    }
}