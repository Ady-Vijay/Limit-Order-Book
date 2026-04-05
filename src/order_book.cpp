#include <list>
#include <map>
#include <unordered_map>
#include "order.h"  

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

    static uint64_t best_price(Order& order, std::map<uint64_t, std::list<Order>>& order_type){
        if(order.type == OrderType::Buy){
            return order_type.empty() ? -1 : order_type.begin()->first;
        }
        return order_type.empty() ? -1 : order_type.rbegin()->first;
    }


    void fulfilment(Order& order, std::map<uint64_t, std::list<Order>>& order_type){
        auto compare = [](Order& order, uint64_t price) { return order.type == OrderType::Buy ? price <= order.price : price >= order.price; };
        while(order.quantity && best_price(order, order_type) != -1 && compare(order, best_price(order, order_type))){
            auto& level = order_type[best_price(order, order_type)];
            while(order.quantity && !level.empty()){
                auto& top_order = level.front();
                uint64_t trade_quantity = std::min(order.quantity, top_order.quantity);
                order.quantity -= trade_quantity;
                top_order.quantity -= trade_quantity;
                trades.emplace_back(top_order.price, trade_quantity, top_order.ticker);
                if(top_order.quantity == 0){
                    order_book.erase(top_order.id);
                    level.pop_front();
                }
            }
            if(level.empty()){
                order_type.erase(best_price(order, order_type));
            }
        }
    }

public:
    std::vector<Trade> trades;

    uint64_t add(Order&& order){
        uint64_t id = order.id;
        uint64_t price = order.price;
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
            auto& level = buy_orders[price];
            level.erase(order);
            if(level.empty()){
                buy_orders.erase(price);
            }
        } else {
            auto& level = sell_orders[price];
            level.erase(order);
            if(level.empty()){
                sell_orders.erase(price);
            }
        }
        order_book.erase(order_id);
    }


    // Returns the order ID if the order is added to the book, or -1 if it is fully matched and not added.
    uint64_t match(Order&& incoming_order){
        if(incoming_order.type == OrderType::Buy){
            fulfilment(incoming_order, sell_orders);
        } else {
            fulfilment(incoming_order, buy_orders);
        }
        if(incoming_order.quantity > 0){
            return add(std::move(incoming_order));
        }
        return -1;
    }
};