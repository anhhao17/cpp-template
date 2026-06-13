#include "etlx/io/io.hpp"

#include <cstdint>
#include <cstring>

#include <gtest/gtest.h>

namespace {

etlx::ConstByteSpan Bytes(const char* s) {
    return etlx::ConstByteSpan{reinterpret_cast<const uint8_t*>(s),
                               std::strlen(s)};
}

TEST(SpanReader, ReadsInChunksThenEnds) {
    auto src = Bytes("abcdef");
    etlx::io::SpanReader r(src);

    uint8_t buf[4];
    auto n1 = r.Read(etlx::ByteSpan{buf, 4});
    ASSERT_TRUE(n1.has_value());
    EXPECT_EQ(n1.value(), 4u);
    EXPECT_EQ(0, std::memcmp(buf, "abcd", 4));

    auto n2 = r.Read(etlx::ByteSpan{buf, 4});
    ASSERT_TRUE(n2.has_value());
    EXPECT_EQ(n2.value(), 2u);  // only "ef" left

    auto n3 = r.Read(etlx::ByteSpan{buf, 4});
    ASSERT_TRUE(n3.has_value());
    EXPECT_EQ(n3.value(), 0u);  // end of stream
}

TEST(SpanWriter, WritesUntilFull) {
    uint8_t out[4];
    etlx::io::SpanWriter w(etlx::ByteSpan{out, 4});

    EXPECT_TRUE(w.Write(Bytes("ab")).has_value());
    EXPECT_EQ(w.written(), 2u);

    auto fail = w.Write(Bytes("xyz"));  // would overflow the 4-byte buffer
    ASSERT_FALSE(fail.has_value());
    EXPECT_EQ(fail.error().code, etlx::io::ShortWrite);
}

TEST(CountingWriter, TotalsBytes) {
    etlx::io::CountingWriter c;
    EXPECT_TRUE(c.Write(Bytes("hello")).has_value());
    EXPECT_TRUE(c.Write(Bytes("!")).has_value());
    EXPECT_EQ(c.count(), 6u);
}

TEST(Copy, StreamsThroughTeeIntoBufferAndCounter) {
    auto src = Bytes("stream me through small buffers");
    etlx::io::SpanReader reader(src);

    uint8_t dst_buf[64];
    etlx::io::SpanWriter   dest(etlx::ByteSpan{dst_buf, sizeof(dst_buf)});
    etlx::io::CountingWriter counter;
    etlx::io::TeeWriter      tee(dest, counter);

    uint8_t scratch[8];
    auto copied =
        etlx::io::Copy(reader, tee, etlx::ByteSpan{scratch, sizeof(scratch)});

    ASSERT_TRUE(copied.has_value());
    EXPECT_EQ(copied.value(), src.size());
    EXPECT_EQ(counter.count(), src.size());
    EXPECT_EQ(dest.written(), src.size());
    EXPECT_EQ(0, std::memcmp(dst_buf, src.data(), src.size()));
}

TEST(Copy, RejectsEmptyScratch) {
    auto src = Bytes("x");
    etlx::io::SpanReader reader(src);
    etlx::io::CountingWriter counter;
    auto r = etlx::io::Copy(reader, counter, etlx::ByteSpan{});
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code, etlx::io::OutOfRange);
}

} // namespace
