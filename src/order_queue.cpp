    #include "order_queue.h"

    OrderQueue::OrderQueue() {
        stub = new Node();
        head = stub;
        tail = stub;
    }

    uint64_t OrderQueue::push(uint64_t price, uint64_t quantity, std::string ticker, OrderType type) {
        Order order(price, quantity, std::move(ticker), type);
        uint64_t id = order.id;
        Node* node = new Node(std::move(order));
        Node* prev = head.exchange(node);
        prev->next.store(node);
        return id;
    }

    bool OrderQueue::pop(Order&& item) {
        Node* node = tail->next.load(std::memory_order_acquire);
        if (node == nullptr) {
            return false;
        }

        Node* expected = node;  // save a copy
        head.compare_exchange_strong(expected, tail);
        item = std::move(node->order);
        if(cancelled_orders.count(item.id)){
            item.id = 0;
        }
        Node* old_tail = tail;
        tail = node;  // tail moves forward to the consumed node

        delete old_tail;  // free the old tail
        return true;
    }

    void OrderQueue::cancel(uint64_t order_id) {
        std::lock_guard<std::mutex> cancel_lock(cancel_mtx);
        cancelled_orders.insert(order_id);
    }