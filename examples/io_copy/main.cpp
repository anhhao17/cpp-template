// Example: stream bytes through fixed buffers with the io Reader/Writer model,
// fanning the same data into two sinks via TeeWriter while a CountingWriter
// totals it. Build target: example_io_copy.
#include "etlx/io/io.hpp"

#include <cstdint>
#include <cstdio>

int main() {
    const char* msg = "stream me through fixed buffers without a heap";
    etlx::io::SpanReader reader(etlx::ConstByteSpan{
        reinterpret_cast<const uint8_t*>(msg), __builtin_strlen(msg)});

    uint8_t out_buf[64];
    etlx::io::SpanWriter   dest(etlx::ByteSpan{out_buf, sizeof(out_buf)});
    etlx::io::CountingWriter counter;
    etlx::io::TeeWriter      tee(dest, counter);

    // 8-byte scratch: the payload is copied in several small chunks.
    uint8_t scratch[8];
    auto copied = etlx::io::Copy(reader, tee, etlx::ByteSpan{scratch, sizeof(scratch)});

    if (!copied) {
        std::printf("copy failed: %s\n", copied.error().message.c_str());
        return 1;
    }
    std::printf("copied %zu bytes (counter saw %zu)\n", copied.value(),
                counter.count());
    std::printf("dest: %.*s\n", static_cast<int>(dest.written()), out_buf);
    return 0;
}
