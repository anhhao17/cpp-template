/* se05x_reset_stub.c — no-op reset stubs for simulator transport.
 * Replaces platform/rsp/se05x_reset.c (GPIO-based) when there is no hardware.
 * Based on wolfSSL/simulators SE050Sim/wolfcrypt-test/se05x_reset.c (GPL-3.0+). */

#include <stdint.h>
#include "sm_timer.h"

void axReset_HostConfigure(void) {}
void axReset_HostUnconfigure(void) {}
void axReset_ResetPulseDUT(int reset_logic) { (void)reset_logic; }
void axReset_PowerDown(int reset_logic)     { (void)reset_logic; }
void axReset_PowerUp(int reset_logic)       { (void)reset_logic; }
void se05x_ic_reset(uint32_t applet_version){ (void)applet_version; }
