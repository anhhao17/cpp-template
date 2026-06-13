#include "host/host_io.hpp"

#include <cstdio>

namespace etlx::ports::host {

FileWriter::FileWriter(void* file) : file_(file ? file : stdout) {}

Status FileWriter::Write(ConstByteSpan src) {
    auto* fp = static_cast<std::FILE*>(file_);
    const size_t n = std::fwrite(src.data(), 1, src.size(), fp);
    if (n != src.size()) {
        return Fail(io::Category, io::ShortWrite, "fwrite short write");
    }
    return Ok();
}

void StderrLogSink::Write(log::Level level, etl::string_view line) {
    const char* tag = "?";
    switch (level) {
        case log::Level::Error: tag = "ERROR"; break;
        case log::Level::Warn:  tag = "WARN";  break;
        case log::Level::Info:  tag = "INFO";  break;
        case log::Level::Debug: tag = "DEBUG"; break;
        case log::Level::None:  tag = "NONE";  break;
    }
    std::fprintf(stderr, "[%s] %.*s\n", tag,
                 static_cast<int>(line.size()), line.data());
}

} // namespace etlx::ports::host
