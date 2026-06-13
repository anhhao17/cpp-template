#include "apps/democli/cli.hpp"

#include "etlx/log/log.hpp"
#include "host/host_io.hpp"

// Process entry point. Wires the host log sink, then defers everything to
// democli::Main so the same logic can be exercised from unit tests.
int main(int argc, char** argv) {
    static etlx::ports::host::StderrLogSink sink;
    etlx::log::SetSink(&sink);
    etlx::log::SetLevel(etlx::log::Level::Info);

    return democli::Main(argc, argv);
}
