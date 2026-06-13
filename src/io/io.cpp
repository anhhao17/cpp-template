#include "etlx/io/io.hpp"

#include <algorithm>
#include <cstring>

namespace etlx::io {

const error::Category Category{"io"};

Result<size_t> SpanReader::Read(ByteSpan dst) {
    const size_t n = std::min(dst.size(), remaining());
    if (n > 0) {
        std::memcpy(dst.data(), data_.data() + pos_, n);
        pos_ += n;
    }
    return n;  // 0 signals end of stream
}

Status SpanWriter::Write(ConstByteSpan src) {
    if (src.size() > buffer_.size() - pos_) {
        return Fail(Category, ShortWrite, "SpanWriter buffer full");
    }
    std::memcpy(buffer_.data() + pos_, src.data(), src.size());
    pos_ += src.size();
    return Ok();
}

Status CountingWriter::Write(ConstByteSpan src) {
    count_ += src.size();
    return Ok();
}

Status TeeWriter::Write(ConstByteSpan src) {
    if (auto s = a_.Write(src); !s) return s;
    return b_.Write(src);
}

Result<size_t> Copy(Reader& src, Writer& dst, ByteSpan scratch) {
    if (scratch.empty()) {
        return Fail(Category, OutOfRange, "Copy needs a non-empty scratch buffer");
    }
    size_t total = 0;
    for (;;) {
        auto rd = src.Read(scratch);
        if (!rd) return Unexpected<>{rd.error()};
        const size_t n = rd.value();
        if (n == 0) break;  // end of stream

        if (auto wr = dst.Write(ConstByteSpan{scratch.data(), n}); !wr) {
            return Unexpected<>{wr.error()};
        }
        total += n;
    }
    return total;
}

} // namespace etlx::io
