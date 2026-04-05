#ifndef ORDER_QUEUE_H
#define ORDER_QUEUE_H

#include <list> 
#include <unordered_map>
#include <unordered_set>
#include "order.h"
#include "order_book.h"
#include <mutex>

class OrderQueue {
private:
    std::list<Order> queue;
    std::mutex mtx;
    std::unordered_map<uint64_t, typename std::list<Order>::iterator> order_map;

public:
    std::mutex cancel_mtx;
    std::unordered_set<uint64_t> cancelled_orders;

    bool pop(Order&& item);
    uint64_t push(uint64_t price, uint64_t quantity, std::string ticker, OrderType type);
    void cancel(uint64_t order_id);
};

#endif