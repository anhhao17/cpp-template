#ifndef ETLX_PORTS_HOST_IO_HPP
#define ETLX_PORTS_HOST_IO_HPP

#include "etlx/io/io.hpp"
#include "etlx/log/log.hpp"

// Host backends for the io and log interfaces, used by democli, the examples
// and tests when running on the native host or under qemu. Equivalent ports for
// freertos / bare-metal would live alongside this one.
namespace etlx::ports::host {

// An io::Writer that appends to a C FILE* (defaults to stdout).
class FileWriter final : public io::Writer {
public:
    explicit FileWriter(void* file = nullptr);  // nullptr => stdout
    Status Write(ConstByteSpan src) override;

private:
    void* file_;
};

// A log::Sink that writes "[LEVEL] line\n" to stderr.
class StderrLogSink final : public log::Sink {
public:
    void Write(log::Level level, etl::string_view line) override;
};

} // namespace etlx::ports::host

#endif // ETLX_PORTS_HOST_IO_HPP
