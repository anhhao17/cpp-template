#ifndef ETLX_IO_IO_HPP
#define ETLX_IO_IO_HPP

#include "etlx/core/result.hpp"
#include "etlx/core/span.hpp"

#include <cstddef>

// Byte-stream abstraction, modelled on Mender's Reader/Writer. Crypto, http and
// storage all speak these interfaces so data can be streamed through fixed
// buffers without ever materialising a whole payload.
namespace etlx::io {

extern const error::Category Category;

enum Code : error::Code {
    None        = 0,
    EndOfStream = 1,  // a Read had no more data to give
    ShortWrite  = 2,  // a Writer could not accept all the bytes
    OutOfRange  = 3,
};

// Pulls bytes out of a source into the caller's buffer. Returns the number of
// bytes actually read (0 means end of stream).
struct Reader {
    virtual Result<size_t> Read(ByteSpan dst) = 0;
    virtual ~Reader() = default;
};

// Pushes bytes into a sink. Must consume the whole span or return an error.
struct Writer {
    virtual Status Write(ConstByteSpan src) = 0;
    virtual ~Writer() = default;
};

// Reads sequentially out of a fixed in-memory buffer.
class SpanReader final : public Reader {
public:
    explicit SpanReader(ConstByteSpan data) : data_(data) {}

    Result<size_t> Read(ByteSpan dst) override;

    size_t remaining() const { return data_.size() - pos_; }

private:
    ConstByteSpan data_;
    size_t        pos_ = 0;
};

// Writes sequentially into a fixed in-memory buffer, failing once it is full.
class SpanWriter final : public Writer {
public:
    explicit SpanWriter(ByteSpan buffer) : buffer_(buffer) {}

    Status Write(ConstByteSpan src) override;

    size_t written() const { return pos_; }

private:
    ByteSpan buffer_;
    size_t   pos_ = 0;
};

// Discards everything written but counts the bytes; useful for sizing passes.
class CountingWriter final : public Writer {
public:
    Status Write(ConstByteSpan src) override;

    size_t count() const { return count_; }

private:
    size_t count_ = 0;
};

// Forwards every write to two underlying writers, e.g. hash a stream while also
// storing it. Fails if either downstream writer fails.
class TeeWriter final : public Writer {
public:
    TeeWriter(Writer& a, Writer& b) : a_(a), b_(b) {}

    Status Write(ConstByteSpan src) override;

private:
    Writer& a_;
    Writer& b_;
};

// Drains a Reader into a Writer using a fixed scratch buffer, never holding the
// whole stream in memory. Returns the total number of bytes copied.
Result<size_t> Copy(Reader& src, Writer& dst, ByteSpan scratch);

} // namespace etlx::io

#endif // ETLX_IO_IO_HPP
