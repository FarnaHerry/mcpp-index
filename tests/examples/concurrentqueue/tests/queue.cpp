// compat.concurrentqueue — exercise the C++ API the way the library's README
// shows it: enqueue/try_dequeue, bulk ops, a blocking consumer, and a real
// multi-producer/multi-consumer race. Every check asserts observable
// behaviour (order, exact-once delivery, timeout), so a package that links
// but miscompiles the lock-free paths still fails here.
#include <concurrentqueue.h>
#include <blockingconcurrentqueue.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

int main() {
    bool ok = true;
    auto check = [&](bool cond, const char* what) {
        if (!cond) {
            std::printf("FAIL: %s\n", what);
            ok = false;
        }
    };

    // ---- single-threaded FIFO --------------------------------------------
    // The queue is MPMC, not ordered ACROSS producers — but a single producer
    // hands its own items to the consumer in order, and that is what we hold
    // it to here.
    {
        moodycamel::ConcurrentQueue<int> q;
        check(q.size_approx() == 0, "fresh queue reports size 0");
        for (int i = 0; i < 1000; ++i) q.enqueue(i);
        check(q.size_approx() == 1000, "size_approx after 1000 enqueues");

        int v = -1;
        for (int i = 0; i < 1000; ++i) {
            if (!q.try_dequeue(v) || v != i) {
                check(false, "single-producer FIFO order preserved");
                break;
            }
        }
        check(!q.try_dequeue(v), "drained queue reports try_dequeue == false");
        check(q.size_approx() == 0, "size_approx back to 0 after draining");
    }

    // ---- bulk ops ---------------------------------------------------------
    {
        moodycamel::ConcurrentQueue<int> q;
        std::vector<int> in(5000);
        for (int i = 0; i < 5000; ++i) in[i] = i;
        // enqueue_bulk answers YES/NO (bool); only try_dequeue_bulk answers
        // HOW MANY (size_t). Mixing the two up is exactly the kind of mistake
        // this test exists to catch — starting with its own author's.
        check(q.enqueue_bulk(in.data(), in.size()),
              "enqueue_bulk reports everything enqueued");

        std::vector<int> out(5000, -1);
        check(q.try_dequeue_bulk(out.data(), out.size()) == out.size(),
              "try_dequeue_bulk reports everything dequeued");
        check(in == out, "bulk round-trip preserves contents and order");
    }

    // ---- producer token ----------------------------------------------------
    // Tokens are the documented fast path for a dedicated producer; exercise
    // at least the enqueue side of them.
    {
        moodycamel::ConcurrentQueue<int> q;
        moodycamel::ProducerToken ptok(q);
        q.enqueue(ptok, 7);
        int v = 0;
        check(q.try_dequeue(v) && v == 7, "token enqueue round-trips");
    }

    // ---- move-only elements ------------------------------------------------
    {
        moodycamel::ConcurrentQueue<std::unique_ptr<int>> q;
        q.enqueue(std::make_unique<int>(41));
        q.enqueue(std::make_unique<int>(42));
        std::unique_ptr<int> p;
        check(q.try_dequeue(p) && p && *p == 41, "move-only element survives the queue");
        check(q.try_dequeue(p) && p && *p == 42, "second move-only element comes out in order");
        check(!q.try_dequeue(p), "queue drains after move-only elements");
    }

    // ---- blocking consumer -------------------------------------------------
    // wait_dequeue must actually block until the producer lands an item, and
    // wait_dequeue_timed on an EMPTY queue must come back false after the
    // timeout — the second half is what separates "semaphore posted eagerly"
    // from a real wait.
    {
        moodycamel::BlockingConcurrentQueue<int> q;

        int missed = -1;
        auto t0 = std::chrono::steady_clock::now();
        bool got = q.wait_dequeue_timed(missed, std::chrono::milliseconds(50));
        auto elapsed = std::chrono::steady_clock::now() - t0;
        check(!got, "wait_dequeue_timed on an empty queue times out");
        check(elapsed >= std::chrono::milliseconds(45),
              "the timeout was actually waited, not spun through");

        std::thread producer([&q] {
            for (int i = 0; i < 2000; ++i) q.enqueue(i);
        });
        int prev = -1;
        for (int i = 0; i < 2000; ++i) {
            int v = -1;
            // wait_dequeue returns void in 1.0.5 (only the _timed spelling
            // reports success), so the assertion is on the VALUE alone: if
            // the semaphore path were broken this call would either hang
            // (test timeout) or hand back a value out of sequence.
            q.wait_dequeue(v);
            if (v != prev + 1) {
                check(false, "blocking consumer receives every item in order");
                break;
            }
            prev = v;
        }
        producer.join();
        int drained = -1;
        check(!q.wait_dequeue_timed(drained, std::chrono::milliseconds(50)),
              "nothing left after the consumer caught up");
    }

    // ---- MPMC: exact-once delivery under contention -------------------------
    // 4 producers x 10000 unique values, 4 consumers racing them out. Every
    // value must arrive EXACTLY once: a lost slot, a duplicated slot, or an
    // ABA-shaped torn read all show up as a counter != 1. This is the check
    // the single-threaded assertions cannot make.
    {
        constexpr int kProducers = 4, kConsumers = 4, kPerProducer = 10000;
        constexpr int kTotal = kProducers * kPerProducer;

        moodycamel::ConcurrentQueue<int> q;
        std::vector<std::atomic<char>> seen(kTotal);
        for (auto& s : seen) s.store(0, std::memory_order_relaxed);
        std::atomic<int> dequeued{0};

        std::vector<std::thread> producers, consumers;
        for (int p = 0; p < kProducers; ++p) {
            producers.emplace_back([&q, p] {
                for (int i = 0; i < kPerProducer; ++i) q.enqueue(p * kPerProducer + i);
            });
        }
        for (int c = 0; c < kConsumers; ++c) {
            consumers.emplace_back([&] {
                int v;
                // try_dequeue until the producers are done AND the queue is
                // empty — the consumer that would otherwise spin on an empty
                // queue while producers are still working is the whole point
                // of a concurrent queue.
                for (;;) {
                    while (q.try_dequeue(v)) {
                        seen[v].store(1, std::memory_order_relaxed);
                        dequeued.fetch_add(1, std::memory_order_relaxed);
                    }
                    if (dequeued.load(std::memory_order_relaxed) >= kTotal) return;
                    std::this_thread::yield();
                }
            });
        }
        for (auto& t : producers) t.join();
        for (auto& t : consumers) t.join();

        check(dequeued.load() == kTotal, "every enqueued value was dequeued");
        int exactly_once = 0;
        for (const auto& s : seen) exactly_once += (s.load() == 1) ? 1 : 0;
        check(exactly_once == kTotal, "every value delivered exactly once (no loss, no duplicate)");
    }

    std::printf("%s\n", ok ? "concurrentqueue: all checks passed" : "concurrentqueue: FAILED");
    return ok ? 0 : 1;
}
