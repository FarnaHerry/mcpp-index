// Behavioral test for compat.abseil.
//
// Every assertion below crosses into a COMPILED Abseil translation unit, on
// purpose — header-only usage would link zero objects from the package and
// still go green, which is exactly the failure this suite has to catch:
//
//   absl::StrCat / StrSplit / StrJoin  -> absl/strings/*.cc
//   absl::Status / StatusOr            -> absl/status/status.cc, statusor.cc
//   absl::Mutex / Notification         -> absl/synchronization/mutex.cc
//   absl::Cord                         -> absl/strings/cord.cc + cord_rep_*.cc
//   absl::FormatTime / ParseTime       -> absl/time/*.cc + internal/cctz/src/*.cc
//   absl::flat_hash_map                -> absl/container/internal/raw_hash_set.cc
//   absl::StrFormat                    -> absl/strings/internal/str_format/*.cc
//
// Returns non-zero on any mismatch.
#include <string>
#include <thread>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/civil_time.h"
#include "absl/time/time.h"

namespace {

bool strings_ok() {
    if (absl::StrCat("answer=", 42, "/", 1.5) != "answer=42/1.5") return false;
    if (absl::StrFormat("%s:%04d:%#x", "id", 7, 255) != "id:0007:0xff") return false;

    const std::vector<std::string> parts = absl::StrSplit("a,b,,c", ',');
    if (parts.size() != 4 || parts[2] != "") return false;
    if (absl::StrJoin(parts, "|") != "a|b||c") return false;
    return true;
}

bool cord_ok() {
    // Build a Cord large enough to leave the inline representation and go
    // through the btree reps in absl/strings/internal/cord_rep_btree*.cc.
    absl::Cord cord;
    for (int i = 0; i < 512; ++i) cord.Append(absl::StrCat("chunk", i, ";"));
    const std::string flat(cord);
    return cord.size() == flat.size() && flat.rfind("chunk511;") != std::string::npos;
}

bool status_ok() {
    const absl::Status ok = absl::OkStatus();
    const absl::Status err = absl::InvalidArgumentError("bad input");
    if (!ok.ok() || err.ok()) return false;
    if (err.code() != absl::StatusCode::kInvalidArgument) return false;
    if (err.message() != "bad input") return false;
    // ToString() formats through absl/status/status.cc.
    if (err.ToString().find("INVALID_ARGUMENT") == std::string::npos) return false;

    const absl::StatusOr<int> good = 11;
    const absl::StatusOr<int> bad = absl::NotFoundError("missing");
    return good.ok() && *good == 11 && !bad.ok();
}

bool time_ok() {
    // A fixed instant, formatted in UTC: exercises the vendored cctz sources.
    const absl::TimeZone utc = absl::UTCTimeZone();
    const absl::Time t = absl::FromCivil(absl::CivilSecond(2026, 8, 4, 12, 34, 56), utc);
    if (absl::FormatTime("%Y-%m-%dT%H:%M:%S", t, utc) != "2026-08-04T12:34:56") return false;

    absl::Time parsed;
    std::string err;
    if (!absl::ParseTime("%Y-%m-%d", "2026-08-04", utc, &parsed, &err)) return false;
    return absl::ToUnixSeconds(t) - absl::ToUnixSeconds(parsed) == 12 * 3600 + 34 * 60 + 56;
}

bool concurrency_ok() {
    // absl::Mutex + Notification pull in the real synchronization objects
    // (mutex.cc, waiter backends, thread identity).
    absl::Mutex mu;
    int counter = 0;
    absl::Notification started;

    std::thread worker([&] {
        started.WaitForNotification();
        for (int i = 0; i < 1000; ++i) {
            absl::MutexLock lock(&mu);
            ++counter;
        }
    });

    started.Notify();
    for (int i = 0; i < 1000; ++i) {
        absl::MutexLock lock(&mu);
        ++counter;
    }
    worker.join();

    absl::MutexLock lock(&mu);
    return counter == 2000;
}

bool container_ok() {
    absl::flat_hash_map<std::string, int> m;
    for (int i = 0; i < 256; ++i) m.emplace(absl::StrCat("k", i), i);
    if (m.size() != 256) return false;
    const auto it = m.find("k255");
    return it != m.end() && it->second == 255;
}

}  // namespace

int main() {
    const bool ok = strings_ok() && cord_ok() && status_ok() && time_ok()
                    && concurrency_ok() && container_ok();
    return ok ? 0 : 1;
}
