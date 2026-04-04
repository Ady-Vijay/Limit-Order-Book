#include <list>
#include <map>
#include <unordered_map>
#include "order.h"  

class OrderBook {
public:
    std::map<uint64_t, std::list<Order>> buy_orders;
    std::map<uint64_t, std::list<Order>> sell_orders;

    std::unordered_map<uint64_t, std::list<Order>::iterator> order_book;

    uint64_t add(uint64_t price, uint64_t size, std::string ticker, OrderType type){
        Order order (price, size, ticker, type);
        uint64_t id = order.id;
        
        if(order.type == OrderType::Buy){
            buy_orders[price].push_back(std::move(order));
            order_book[id] = std::prev(buy_orders[price].end());
        } else {
            sell_orders[price].push_back(std::move(order));
            order_book[id] = std::prev(sell_orders[price].end());
        }

        return id;
    }

    void cancel(uint64_t order_id){
        if(order_book.find(order_id) == order_book.end()) return;
    
        auto order = order_book[order_id];
        uint64_t price = order->price;
        if(order->type == OrderType::Buy){
            auto level = buy_orders[price];
            level.erase(order);
            if(level.empty()){
                buy_orders.erase(price);
            }
        } else {
            auto level = sell_orders[price];
            level.erase(order);
            if(level.empty()){
                sell_orders.erase(price);
            }
        }
        order_book.erase(order_id);
    }





};