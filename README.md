# Limit Order Book

A multithreaded limit order book and matching engine written in C++17. Orders enter through a lock-free MPSC queue and are matched by a dedicated single-threaded engine — no locks on the hot path.

---

## Architecture

```
[Order Entry Thread(s)]
        |
        v
[Lock-free MPSC Queue]      <-- concurrent producers, single consumer
        |
        v
[Matching Engine Thread]    <-- owns OrderBook exclusively, no locks needed
        |
        v
[Trade log (vector<Trade>)]
```

The matching engine thread is the sole owner of the order book. Because no other thread touches the book, the matching path is entirely lock-free — no mutexes, no cache line bouncing. Concurrency is confined to the queue between order entry and the engine.

This is the same architecture used by LMAX and most HFT matching engines.

---

## Design decisions

**Why `std::list` for orders at a price level, not `std::vector`?**

Cancel is O(1) via a stored iterator into the list (`order_book` unordered_map maps order ID → `list::iterator`). `vector::erase` is O(n) due to shifting.

**Why `std::map` for price levels, not `std::unordered_map`?**

Best bid = `rbegin()`, best ask = `begin()` — both O(1). `unordered_map` has no ordering, so finding the best price requires a linear scan or a separate heap. `std::map` gives sorted order for free at O(log n) insert/erase.

**Why does the matching engine run single-threaded instead of locking the book?**

Locks on the matching path introduce latency jitter. A single-threaded engine with a lock-free input queue has zero synchronization overhead on the hot path. The matching bottleneck stays in the queue, which scales horizontally with producers.

**Why integer prices?**

IEEE 754 floating point comparison is unreliable (`0.1 + 0.2 != 0.3`). Price equality and ordering are correctness-critical in a matching engine. All prices are integer ticks.

---

## Build

```bash
cmake -B build
cmake --build build --config Release
```

Requires CMake 3.14+ and a C++17 compiler.

---

## Tests

```bash
cd build
ctest -C Release --output-on-failure
```

Four test suites covering order construction, order book add/cancel/match/price-time priority, lock-free queue correctness (including concurrent push/pop and cancel), and the full matching engine pipeline.

---

## Benchmarks

Run on your machine after building:

```bash
./build/Release/bench.exe     # Windows
./build/bench                 # Linux/macOS
```

Results on a Windows 10 machine (AMD/Intel, MSVC, Release build):

```
=== Limit Order Book — Benchmarks ===

[ Single-threaded throughput ]
  add()  no crossing                                           2,802,391  ops/sec
  match()  fully crossing                                      2,729,498  ops/sec
  cancel()                                                     4,045,741  ops/sec

[ Latency  (100,000 samples) ]
  match()  single crossing         p50=200 ns   p99=300 ns   p999=700 ns

[ Queue push throughput — no engine ]
  OrderQueue::push()  1 producer thread                       11,635,538  ops/sec
  OrderQueue::push()  2 producer threads                      15,545,234  ops/sec
  OrderQueue::push()  4 producer threads                      23,471,644  ops/sec

[ Pipeline: producers -> queue -> engine ]
  Pipeline  1 producer  + engine (concurrent)                 10,716,161  ops/sec
  Pipeline  2 producers + engine (concurrent)                 14,352,721  ops/sec
  Pipeline  4 producers + engine (concurrent)                 23,348,884  ops/sec

[ Realistic workload — 10 producer threads ]
  Order mix    ~80% passive [96-104]  ~10% aggressive [92/108]  ~10% cancel
  Tickers      AAPL TSLA MSFT NVDA SPY  (each has its own book)
  Orders pushed:    899,765   Cancels:  100,235   Trades:  316,110   Fill: 35.1%
  Push throughput  10 threads, mixed ops                      18,433,621  ops/sec

[ Full-match guarantee — 10 producer threads ]
  5 buy threads + 5 sell threads, price=100, qty=1, ticker=AAPL
  Orders: 500,000   Expected trades: 250,000   Actual: 250,000  [PASS]
  Push throughput  10 threads, all-match                      45,309,144  ops/sec

[ High-volume stress test — 10 rounds x 2M orders = 20M total ]
  Round  1                                                    17,640,651  ops/sec
  Round  2                                                    19,251,390  ops/sec
  Round  3                                                    14,378,135  ops/sec
  Round  4                                                    18,934,320  ops/sec
  Round  5                                                    18,550,157  ops/sec
  Round  6                                                    17,245,572  ops/sec
  Round  7                                                    16,028,068  ops/sec
  Round  8                                                    17,893,601  ops/sec
  Round  9                                                    18,357,584  ops/sec
  Round 10                                                    11,865,669  ops/sec
  ── totals: 18M orders, 2M cancels, 14.2M trades, 79.1% fill ──
  Avg push throughput across all rounds                       16,668,996  ops/sec
```

The queue scales near-linearly with producers. The matching engine itself is the throughput ceiling at ~2.7M matches/sec single-threaded; the queue peaks at 45M ops/sec under a perfectly matched all-fill workload. Under a realistic mixed workload (5 tickers, varied prices, 10% cancels) the fill rate is ~35% — passive orders don't always find a cross. The stress test sustains 16M+ ops/sec push throughput across 20M orders.
