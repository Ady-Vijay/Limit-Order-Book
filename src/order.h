#ifndef ORDER_H
#define ORDER_H

#include <string>
#include <chrono>
#include <cstdint>
#include <atomic>

using Clock = std::chrono::high_resolution_clock;
using Nanos = std::chrono::nanoseconds;

enum class OrderType{Buy, Sell};

struct Order {
    static inline std::atomic<uint64_t> uniqueOrderID = 0;

    uint64_t price;
    uint64_t quantity;
    std::string ticker;
    OrderType type;
    uint64_t id;
    int64_t timestamp;

    Order(uint64_t p, uint64_t q, std::string t, OrderType ot) : 
        price(p), quantity(q), ticker(std::move(t)), type(ot), id(uniqueOrderID++), timestamp(std::chrono::duration_cast<Nanos>(Clock::now().time_since_epoch()).count()) {}  

    //remove copy operator but keep move operators
    Order(const Order&) = delete;
    Order& operator=(const Order&) = delete;
    Order(Order&&) noexcept = default;
    Order& operator=(Order&&) noexcept = default;
};

#endif


