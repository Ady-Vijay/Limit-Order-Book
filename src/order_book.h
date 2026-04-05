#ifndef ORDER_BOOK_H
#define ORDER_BOOK_H

#include "order.h"
#include <map>
#include <list>
#include <unordered_map>


struct Trade {
    uint64_t price;
    uint64_t quantity;
    std::string ticker;
    int64_t timestamp;

    Trade(uint64_t p, uint64_t q, std::string t) : 
        price(p), quantity(q), ticker(std::move(t)), timestamp(std::chrono::duration_cast<Nanos>(Clock::now().time_since_epoch()).count()) {}
};


class OrderBook {
private:
    std::map<uint64_t, std::list<Order>> buy_orders;
    std::map<uint64_t, std::list<Order>> sell_orders;

    std::unordered_map<uint64_t, std::list<Order>::iterator> order_book;

    static uint64_t best_price(Order& order, std::map<uint64_t, std::list<Order>>& order_type);
    void fulfilment(Order& order, std::map<uint64_t, std::list<Order>>& order_type);
public:
    
    std::vector<Trade> trades;
    uint64_t add(Order&& order);
    void cancel(uint64_t order_id);
    uint64_t match(Order&& incoming_order);
};

#endif