#ifndef ORDER_QUEUE_H
#define ORDER_QUEUE_H

#include <list> 
#include <unordered_map>
#include <unordered_set>
#include "order.h"
#include "order_book.h"
#include <mutex>

struct Node {
    Order order;
    Node* next;
    Node() : order(), next(nullptr) {};
    Node(Order&& o) : order(std::move(o)), next(nullptr) {};
};

class OrderQueue {
private:
    Node* head;
    Node* tail;
    Node* stub; // dummy node to simplify push/pop logic

public:

    std::mutex cancel_mtx;
    std::unordered_set<uint64_t> cancelled_orders;


    OrderQueue() 
    bool pop(Order&& item);
    uint64_t push(uint64_t price, uint64_t quantity, std::string ticker, OrderType type);
    void cancel(uint64_t order_id);
};

#endif