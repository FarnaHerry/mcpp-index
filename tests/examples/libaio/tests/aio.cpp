// Behavioral test: real kernel AIO submissions, not a link check.
//
// Three things can go wrong with this package and none of them is a missing
// symbol.
//
//   1. `struct iocb` is the KERNEL's structure, assembled by libaio.h out of
//      PADDED/PADDEDptr/PADDEDul macros chosen by endianness and word size. Get
//      that selection wrong and every field lands at the wrong offset — the
//      library still links, io_submit still returns 1, and the kernel writes
//      the wrong bytes to the wrong place. So the assertions read back what
//      actually hit the file.
//
//   2. The plain names `io_getevents` and `io_cancel` do not exist as ordinary
//      definitions upstream: the functions are called `io_getevents_0_4` /
//      `io_cancel_0_4` and publish the short names through
//      `.symver … @@LIBAIO_0.4`. Whether that survives into a consumer's link
//      is a linker behavior, so both are called here on purpose.
//
//   3. libaio does NOT use errno. Every wrapper returns `-errno` directly and
//      restores the caller's errno, which is the opposite of the convention
//      the surrounding POSIX calls follow. The error-path assertion pins that
//      down rather than assuming it.
//
// The static_asserts below are upstream's own src/struct_offsets.c, which this
// package deliberately does not compile (it is a build-time check that emits no
// library code). Restating them here keeps the check and moves it to where it
// is actually observable.

#ifdef __linux__

#include <libaio.h>

#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <unistd.h>

// ── upstream src/struct_offsets.c, verbatim in intent ────────────────────
// The iocb.u union overlays io_iocb_common / io_iocb_vector / io_iocb_sockaddr,
// and the kernel reads whichever the opcode implies. If the padding macros
// picked the wrong variant these three stop agreeing.
static_assert(offsetof(struct iocb, u.v.nr) == offsetof(struct iocb, u.c.nbytes),
              "iocb.u.v.nr does not match the offset of iocb.u.c.nbytes");
static_assert(offsetof(struct iocb, u.v.offset) == offsetof(struct iocb, u.c.offset),
              "iocb.u.v.offset does not match the offset of iocb.u.c.offset");
static_assert(offsetof(struct iocb, u.saddr.len) == offsetof(struct iocb, u.c.nbytes),
              "iocb.u.saddr.len does not match the offset of iocb.u.c.nbytes");

namespace {

constexpr std::size_t kBlock = 512;

// Reap exactly `want` completions, or give up. 10s is far beyond what buffered
// AIO on a temp file needs; it is a deadlock guard, not a timing assumption.
int reap(io_context_t ctx, io_event* events, long want) {
    timespec timeout{10, 0};
    return io_getevents(ctx, want, want, events, &timeout);
}

struct TempFile {
    char path[64] = {};
    int  fd       = -1;

    TempFile() {
        const char* dir = std::getenv("TMPDIR");
        std::snprintf(path, sizeof path, "%s/mcpp_libaio_XXXXXX",
                      (dir && *dir) ? dir : "/tmp");
        fd = ::mkstemp(path);
    }
    ~TempFile() {
        if (fd >= 0) ::close(fd);
        if (path[0]) ::unlink(path);
    }
};

// Set by the io_queue_run callback below.
struct CallbackRecord {
    int         calls = 0;
    iocb*       obj   = nullptr;
    long        res   = -1;
};
CallbackRecord g_callback;

void on_complete(io_context_t, iocb* cb, long res, long /*res2*/) {
    g_callback.calls += 1;
    g_callback.obj = cb;
    g_callback.res = res;
}

}  // namespace

int main() {
    TempFile file;
    assert(file.fd >= 0 && "could not create the temp file the test writes through");

    io_context_t ctx = nullptr;
    const int setup = io_setup(8, &ctx);
    assert(setup == 0 && "io_setup failed; libaio returns -errno, not -1");
    assert(ctx != nullptr);

    // ── io_prep_pwrite fills the iocb the kernel will read ───────────────
    // Checked before submitting: if these are wrong the submission below can
    // still "succeed" while writing something else entirely.
    unsigned char out[kBlock];
    for (std::size_t i = 0; i < kBlock; ++i) out[i] = static_cast<unsigned char>(i & 0xFF);

    iocb write_cb;
    io_prep_pwrite(&write_cb, file.fd, out, kBlock, 0);
    assert(write_cb.aio_lio_opcode == IO_CMD_PWRITE);
    assert(write_cb.aio_fildes == file.fd);
    assert(write_cb.u.c.buf == out);
    assert(write_cb.u.c.nbytes == kBlock);
    assert(write_cb.u.c.offset == 0);

    // ── the write actually reaches the file ──────────────────────────────
    {
        iocb* batch[1] = {&write_cb};
        assert(io_submit(ctx, 1, batch) == 1);

        io_event event{};
        assert(reap(ctx, &event, 1) == 1);
        assert(event.obj == &write_cb);
        assert(static_cast<long>(event.res) == static_cast<long>(kBlock));

        unsigned char verify[kBlock] = {};
        assert(::pread(file.fd, verify, kBlock, 0) == static_cast<ssize_t>(kBlock));
        assert(std::memcmp(verify, out, kBlock) == 0 &&
               "the async write did not land the bytes it reported writing");

        // io_cancel on a request the kernel already completed must fail, and
        // must fail libaio's way: a negative errno as the return value.
        // Calling it at all is the point — it is one of the three functions
        // that only exist under a @@LIBAIO_0.4 symbol version.
        io_event discarded{};
        assert(io_cancel(ctx, &write_cb, &discarded) < 0);
    }

    // ── the read path, at a non-zero offset ──────────────────────────────
    {
        unsigned char tail[kBlock];
        for (std::size_t i = 0; i < kBlock; ++i) tail[i] = static_cast<unsigned char>(0xA5 ^ i);

        iocb append;
        io_prep_pwrite(&append, file.fd, tail, kBlock, kBlock);
        iocb* batch[1] = {&append};
        assert(io_submit(ctx, 1, batch) == 1);
        io_event event{};
        assert(reap(ctx, &event, 1) == 1);
        assert(static_cast<long>(event.res) == static_cast<long>(kBlock));

        unsigned char in[kBlock] = {};
        iocb read_cb;
        io_prep_pread(&read_cb, file.fd, in, kBlock, kBlock);
        assert(read_cb.aio_lio_opcode == IO_CMD_PREAD);
        assert(read_cb.u.c.offset == static_cast<long long>(kBlock));

        iocb* rbatch[1] = {&read_cb};
        assert(io_submit(ctx, 1, rbatch) == 1);
        assert(reap(ctx, &event, 1) == 1);
        assert(static_cast<long>(event.res) == static_cast<long>(kBlock));
        assert(std::memcmp(in, tail, kBlock) == 0 &&
               "the async read at offset 512 returned the wrong block");
    }

    // ── a batch of two, told apart by iocb.data ──────────────────────────
    // `data` is the first member of the padded struct, so a wrong PADDEDptr
    // choice shows up here as a cookie that does not come back.
    {
        unsigned char a[kBlock], b[kBlock];
        std::memset(a, 0x11, sizeof a);
        std::memset(b, 0x22, sizeof b);

        iocb first, second;
        io_prep_pwrite(&first, file.fd, a, kBlock, 2 * kBlock);
        io_prep_pwrite(&second, file.fd, b, kBlock, 3 * kBlock);
        first.data = &first;
        second.data = &second;

        iocb* batch[2] = {&first, &second};
        assert(io_submit(ctx, 2, batch) == 2);

        io_event events[2]{};
        assert(reap(ctx, events, 2) == 2);

        bool saw_first = false, saw_second = false;
        for (const io_event& e : events) {
            assert(static_cast<long>(e.res) == static_cast<long>(kBlock));
            if (e.data == &first) saw_first = true;
            if (e.data == &second) saw_second = true;
        }
        assert(saw_first && saw_second && "iocb.data cookies did not round-trip");

        unsigned char verify[kBlock] = {};
        assert(::pread(file.fd, verify, kBlock, 3 * kBlock) == static_cast<ssize_t>(kBlock));
        assert(verify[0] == 0x22 && verify[kBlock - 1] == 0x22);
    }

    // ── the error contract: -errno returned, caller's errno untouched ────
    {
        unsigned char scratch[kBlock] = {};
        iocb bad;
        io_prep_pwrite(&bad, -1, scratch, kBlock, 0);
        iocb* batch[1] = {&bad};

        errno = 0;
        const int rc = io_submit(ctx, 1, batch);
        assert(rc < 0 && "io_submit on a closed fd must fail");
        assert(rc == -EBADF && "libaio reports -errno as the return value");
        assert(errno == 0 && "libaio must not clobber the caller's errno");
    }

    assert(io_destroy(ctx) == 0);

    // ── the io_queue_* convenience layer and its callback ────────────────
    // A separate context, because io_queue_init owns setup/teardown. This is
    // the only path that reads iocb.data as a FUNCTION POINTER, so it is the
    // one place a callback that never fires is a real failure rather than a
    // slow completion.
    {
        io_context_t qctx = nullptr;
        assert(io_queue_init(8, &qctx) == 0);

        unsigned char payload[kBlock];
        std::memset(payload, 0x5A, sizeof payload);

        iocb cb;
        io_prep_pwrite(&cb, file.fd, payload, kBlock, 4 * kBlock);
        io_set_callback(&cb, on_complete);
        assert(cb.data == reinterpret_cast<void*>(&on_complete));

        iocb* batch[1] = {&cb};
        assert(io_submit(qctx, 1, batch) == 1);

        // io_queue_run polls with a zero timeout and returns 0 when the ring is
        // empty, so it is spun until the completion shows up.
        for (int i = 0; i < 1000 && g_callback.calls == 0; ++i) {
            assert(io_queue_run(qctx) >= 0);
            if (g_callback.calls == 0) {
                timespec nap{0, 1'000'000};   // 1ms; 1000 tries = 1s ceiling
                ::nanosleep(&nap, nullptr);
            }
        }

        assert(g_callback.calls == 1 && "io_queue_run never dispatched the callback");
        assert(g_callback.obj == &cb);
        assert(g_callback.res == static_cast<long>(kBlock));

        unsigned char verify[kBlock] = {};
        assert(::pread(file.fd, verify, kBlock, 4 * kBlock) == static_cast<ssize_t>(kBlock));
        assert(verify[0] == 0x5A && verify[kBlock - 1] == 0x5A);

        assert(io_queue_release(qctx) == 0);
    }

    return 0;
}

#else

int main() { return 0; }   // libaio is the Linux AIO ABI; nothing to assert here.

#endif
