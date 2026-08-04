// Behavioral test for compat.protobuf's `gzip` feature.
//
// io/gzip_stream.cc is wrapped head-to-toe in `#if HAVE_ZLIB`, so with the
// feature OFF the TU is empty and GzipOutputStream / GzipInputStream have no
// definitions — this member therefore fails to LINK without the feature,
// which is the negative half of the verification (see the design doc).
//
// With it on: compress a payload, assert the compressed form is actually
// smaller and is a real gzip stream (magic 0x1f 0x8b), then decompress and
// assert byte equality.
#include <string>

#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/io/gzip_stream.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace gpb = google::protobuf;

namespace {

std::string make_payload() {
    // Highly repetitive, so a working deflate must shrink it a lot. Random
    // data would leave "compressed" ~= "original" and make the size assertion
    // meaningless.
    std::string s;
    for (int i = 0; i < 2000; ++i) s += "the quick brown fox jumps over the lazy dog\n";
    return s;
}

bool compress(const std::string& in, std::string* out) {
    gpb::io::StringOutputStream sink(out);
    gpb::io::GzipOutputStream::Options opts;
    opts.format = gpb::io::GzipOutputStream::GZIP;
    gpb::io::GzipOutputStream gz(&sink, opts);
    {
        // GzipOutputStream is a ZeroCopyOutputStream (Next/BackUp); CodedOutputStream
        // is the adapter that turns that into a plain buffer write. It must be
        // destroyed — i.e. flushed back into gz — before gz.Close().
        gpb::io::CodedOutputStream coded(&gz);
        coded.WriteRaw(in.data(), static_cast<int>(in.size()));
        if (coded.HadError()) return false;
    }
    return gz.Close();
}

bool decompress(const std::string& in, std::string* out) {
    gpb::io::ArrayInputStream source(in.data(), static_cast<int>(in.size()));
    gpb::io::GzipInputStream gz(&source, gpb::io::GzipInputStream::GZIP);

    const void* chunk = nullptr;
    int size = 0;
    while (gz.Next(&chunk, &size)) {
        if (size > 0) out->append(static_cast<const char*>(chunk), static_cast<std::size_t>(size));
    }
    return gz.ZlibErrorMessage() == nullptr;
}

}  // namespace

int main() {
    const std::string original = make_payload();

    std::string compressed;
    if (!compress(original, &compressed)) return 1;

    // A real gzip member starts with the 0x1f 0x8b magic.
    if (compressed.size() < 2) return 1;
    if (static_cast<unsigned char>(compressed[0]) != 0x1f) return 1;
    if (static_cast<unsigned char>(compressed[1]) != 0x8b) return 1;

    // Repetitive input must compress substantially; this also proves deflate
    // ran rather than the stream passing bytes through.
    if (compressed.size() >= original.size() / 10) return 1;

    std::string restored;
    if (!decompress(compressed, &restored)) return 1;

    return restored == original ? 0 : 1;
}
