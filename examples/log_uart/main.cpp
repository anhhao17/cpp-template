// Example: route logs through a custom sink. A real target's sink would push
// bytes to a UART or RTT channel; here it captures them into a fixed buffer and
// also echoes to stdout, showing the Sink seam. Build target: example_log_uart.
#include "etlx/log/log.hpp"

#include <cstdio>

#include <etl/string.h>

namespace {

// Stand-in for a UART: collects the most recent line and prints it framed.
class FakeUartSink final : public etlx::log::Sink {
public:
    void Write(etlx::log::Level level, etl::string_view line) override {
        last_.assign(line.data(), line.size());
        std::printf("<uart level=%d> %.*s\n", static_cast<int>(level),
                    static_cast<int>(line.size()), line.data());
    }
    const etl::string<128>& last() const { return last_; }

private:
    etl::string<128> last_;
};

} // namespace

int main() {
    FakeUartSink uart;
    etlx::log::SetSink(&uart);
    etlx::log::SetLevel(etlx::log::Level::Info);

    const char* board = "qemu-aarch64";
    ETLX_LOG_INFO("boot ok, hw=%s", board);
    ETLX_LOG_WARN("battery at %d%%", 17);
    ETLX_LOG_DEBUG("this is dropped: runtime level is Info");  // filtered out

    std::printf("captured last line: %s\n", uart.last().c_str());
    return 0;
}
