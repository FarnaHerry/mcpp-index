// compat.concurrentqueue `c-api` feature — drive the flat extern "C" surface
// over both queues. Values cross the boundary as void*, so the test round-trips
// integers through MoodycamelValue and checks them on the way out. A package
// that compiled the wrappers against the wrong queue type (cq vs bcq handles
// are both void*) would still link and swap semantics — which is exactly the
// failure these assertions catch.
#include <c_api/concurrentqueue.h>

#include <cstdio>
#include <stdint.h>

int main() {
    bool ok = true;
    auto check = [&](bool cond, const char* what) {
        if (!cond) {
            std::printf("FAIL: %s\n", what);
            ok = false;
        }
    };

    // ---- plain queue -------------------------------------------------------
    {
        MoodycamelCQHandle q = nullptr;
        check(moodycamel_cq_create(&q) == 1, "cq_create succeeds");
        check(q != nullptr, "cq_create yields a handle");
        check(moodycamel_cq_size_approx(q) == 0, "fresh cq reports size 0");

        uint64_t sent = 0;
        for (int i = 0; i < 100; ++i) {
            uintptr_t v = 1000 + i;
            sent += v;
            if (moodycamel_cq_enqueue(q, (MoodycamelValue)v) != 1) {
                check(false, "cq_enqueue succeeds");
                break;
            }
        }
        check(moodycamel_cq_size_approx(q) == 100, "cq_size_approx after 100 enqueues");

        uint64_t got = 0;
        int count = 0;
        MoodycamelValue v = nullptr;
        while (moodycamel_cq_try_dequeue(q, &v) == 1) {
            got += (uintptr_t)v;
            ++count;
        }
        check(count == 100, "all 100 values come back out");
        check(got == sent, "values survive the void* round-trip intact");
        check(moodycamel_cq_try_dequeue(q, &v) == 0, "drained cq reports try_dequeue == 0");

        check(moodycamel_cq_destroy(q) == 1, "cq_destroy succeeds");
    }

    // ---- blocking queue ------------------------------------------------------
    {
        MoodycamelBCQHandle q = nullptr;
        check(moodycamel_bcq_create(&q) == 1, "bcq_create succeeds");

        MoodycamelValue v = nullptr;
        check(moodycamel_bcq_try_dequeue(q, &v) == 0, "empty bcq reports try_dequeue == 0");

        for (uintptr_t i = 0; i < 50; ++i) {
            if (moodycamel_bcq_enqueue(q, (MoodycamelValue)i) != 1) {
                check(false, "bcq_enqueue succeeds");
                break;
            }
        }
        // wait_dequeue BLOCKS until an item is available; here the items are
        // already queued, so each call returns without hanging — and in order.
        uint64_t got = 0;
        for (int i = 0; i < 50; ++i) {
            if (moodycamel_bcq_wait_dequeue(q, &v) != 1 || (uintptr_t)v != i) {
                check(false, "bcq_wait_dequeue hands back every value in order");
                break;
            }
            got += (uintptr_t)v;
        }
        check(got == 50L * 49 / 2, "bcq values sum to what went in");
        check(moodycamel_bcq_try_dequeue(q, &v) == 0, "drained bcq reports try_dequeue == 0");

        check(moodycamel_bcq_destroy(q) == 1, "bcq_destroy succeeds");
    }

    std::printf("%s\n", ok ? "concurrentqueue c-api: all checks passed"
                           : "concurrentqueue c-api: FAILED");
    return ok ? 0 : 1;
}
