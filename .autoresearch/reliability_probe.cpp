#include <iostream>
#include "elero/elero_dispatch_logic.h"
#include "elero/elero_packet_validation.h"
#if __has_include("elero/elero_watchdog_logic.h")
#include "elero/elero_watchdog_logic.h"
#define HAS_WATCHDOG_HELPER 1
#else
#define HAS_WATCHDOG_HELPER 0
#endif
#if __has_include("elero/elero_recovery_logic.h")
#include "elero/elero_recovery_logic.h"
#define HAS_RECOVERY_HELPER 1
#else
#define HAS_RECOVERY_HELPER 0
#endif
#if __has_include("elero/elero_overflow_logic.h")
#include "elero/elero_overflow_logic.h"
#define HAS_OVERFLOW_HELPER 1
#else
#define HAS_OVERFLOW_HELPER 0
#endif
#if __has_include("elero/elero_tx_logic.h")
#include "elero/elero_tx_logic.h"
#define HAS_TX_HELPER 1
#else
#define HAS_TX_HELPER 0
#endif

using namespace esphome::elero::dispatch_logic;
using namespace esphome::elero::packet_validation;

namespace {
#ifdef ELERO_HAS_CRC_STATUS_HELPER
constexpr bool kHasCrcHelper = true;
#else
constexpr bool kHasCrcHelper = false;
#endif
}

int main() {
  int dispatch_total = 0;
  int dispatch_failed = 0;
  int packet_total = 0;
  int packet_failed = 0;
  int watchdog_total = 0;
  int watchdog_failed = 0;
  int recovery_total = 0;
  int recovery_failed = 0;
  int overflow_total = 0;
  int overflow_failed = 0;
  int tx_total = 0;
  int tx_failed = 0;

  auto dcheck = [&](bool cond) {
    dispatch_total++;
    if (!cond) dispatch_failed++;
  };
  auto pcheck = [&](bool cond) {
    packet_total++;
    if (!cond) packet_failed++;
  };
  auto wcheck = [&](bool cond) {
    watchdog_total++;
    if (!cond) watchdog_failed++;
  };
  auto rcheck = [&](bool cond) {
    recovery_total++;
    if (!cond) recovery_failed++;
  };
  auto ocheck = [&](bool cond) {
    overflow_total++;
    if (!cond) overflow_failed++;
  };
  auto tcheck = [&](bool cond) {
    tx_total++;
    if (!cond) tx_failed++;
  };

  // Dispatch reliability invariants.
  dcheck(is_dispatch_ready(false, 100, 0, 100));
  dcheck(should_clear_stale_queue(1000, 0, 1000));
  dcheck(calculate_dispatch_delay(50, 1) < calculate_dispatch_delay(50, 2));
  dcheck(calculate_dispatch_delay(50, 2) < calculate_dispatch_delay(50, 3));
  dcheck(calculate_dispatch_delay(50, 3) == calculate_dispatch_delay(50, 8));
  dcheck(!should_defer_for_stop(true, 0x03, 0x03, 0x04));
  dcheck(!should_defer_for_stop(true, 0x04, 0x03, 0x04));
  dcheck(!should_drop_after_retries(2, 3));
  dcheck(should_drop_after_retries(3, 3));

  // Packet validation reliability invariants.
  pcheck(is_valid_packet_length(17));
  pcheck(!is_valid_packet_length(16));
  pcheck(is_valid_dest_count(max_safe_dests()));
  pcheck(!is_valid_dest_count(max_safe_dests() + 1));
  pcheck(!is_valid_dest_count(0));

  // Payload bounds: payload[7] sits at index (26 + dests_len), so that index
  // must be within the declared packet length.
  pcheck(is_valid_packet_bounds(26, 0));
  pcheck(!is_valid_packet_bounds(25, 0));
  pcheck(is_valid_packet_bounds(29, 3));
  pcheck(!is_valid_packet_bounds(28, 3));

  // CRC helper should exist and correctly map appended status byte semantics.
  pcheck(kHasCrcHelper);
#ifdef ELERO_HAS_CRC_STATUS_HELPER
  pcheck(is_crc_valid_status_byte(0x80));
  pcheck(!is_crc_valid_status_byte(0x00));
#endif

  // Watchdog helper should exist and reset escalation counters on healthy RX.
  wcheck(HAS_WATCHDOG_HELPER == 1);
#if HAS_WATCHDOG_HELPER
  esphome::elero::watchdog_logic::EscalationState st{};
  st.flush_count = 2;
  st.reset_count = 1;
  st.window_start_ms = 100;
  esphome::elero::watchdog_logic::reset_on_healthy(st, 5000);
  wcheck(st.flush_count == 0);
  wcheck(st.reset_count == 0);
  wcheck(st.window_start_ms == 5000);
#endif

  // Recovery helper invariants for reinit-failure counting + window boundaries.
  rcheck(HAS_RECOVERY_HELPER == 1);
#if HAS_RECOVERY_HELPER
  rcheck(esphome::elero::recovery_logic::next_reinit_failure_count(0, false) == 1);
  rcheck(esphome::elero::recovery_logic::next_reinit_failure_count(1, false) == 2);
  rcheck(esphome::elero::recovery_logic::next_reinit_failure_count(1, true) == 1);

  esphome::elero::watchdog_logic::EscalationState win{};
  win.flush_count = 2;
  win.reset_count = 1;
  win.window_start_ms = 1000;
  esphome::elero::watchdog_logic::reset_if_window_expired(win, 61000, 60000);
  // At exact boundary, window should be considered expired.
  rcheck(win.flush_count == 0);
  rcheck(win.reset_count == 0);
#endif

  // RX overflow helper invariants.
  ocheck(HAS_OVERFLOW_HELPER == 1);
#if HAS_OVERFLOW_HELPER
  ocheck(esphome::elero::overflow_logic::next_overflow_count(1100, 100, 3, 1000) == 1);
  ocheck(esphome::elero::overflow_logic::next_overflow_count(900, 100, 3, 1000) == 4);
  ocheck(esphome::elero::overflow_logic::should_reinit_after_overflow_count(5, 5));
  ocheck(!esphome::elero::overflow_logic::should_reinit_after_overflow_count(4, 5));
#endif

  // TX helper invariants for destination count sanitization.
  tcheck(HAS_TX_HELPER == 1);
#if HAS_TX_HELPER
  tcheck(esphome::elero::tx_logic::sanitize_num_dests(0, 10) == 1);
  tcheck(esphome::elero::tx_logic::sanitize_num_dests(5, 10) == 5);
  tcheck(esphome::elero::tx_logic::sanitize_num_dests(12, 10) == 10);
#endif

  int total = dispatch_total + packet_total + watchdog_total + recovery_total + overflow_total + tx_total;
  int failed = dispatch_failed + packet_failed + watchdog_failed + recovery_failed + overflow_failed + tx_failed;
  int passed = total - failed;

  double dispatch_score = dispatch_total ? (100.0 * (dispatch_total - dispatch_failed) / dispatch_total) : 0.0;
  double packet_score = packet_total ? (100.0 * (packet_total - packet_failed) / packet_total) : 0.0;
  double watchdog_score = watchdog_total ? (100.0 * (watchdog_total - watchdog_failed) / watchdog_total) : 0.0;
  double recovery_score = recovery_total ? (100.0 * (recovery_total - recovery_failed) / recovery_total) : 0.0;
  double overflow_score = overflow_total ? (100.0 * (overflow_total - overflow_failed) / overflow_total) : 0.0;
  double tx_score = tx_total ? (100.0 * (tx_total - tx_failed) / tx_total) : 0.0;
  double combined = total ? (100.0 * passed / total) : 0.0;

  std::cout << "checks=" << total << " failed=" << failed << " passed=" << passed << "\n";
  std::cout << "METRIC reliability_score_v2=" << dispatch_score << "\n";
  std::cout << "METRIC packet_score=" << packet_score << "\n";
  std::cout << "METRIC watchdog_score=" << watchdog_score << "\n";
  std::cout << "METRIC recovery_score=" << recovery_score << "\n";
  std::cout << "METRIC overflow_score=" << overflow_score << "\n";
  std::cout << "METRIC tx_score=" << tx_score << "\n";
  std::cout << "METRIC failed_checks=" << failed << "\n";
  std::cout << "METRIC reliability_score_v11=" << combined << "\n";
  std::cout << "METRIC reliability_score_v10=" << combined << "\n";
  std::cout << "METRIC reliability_score_v9=" << combined << "\n";
  std::cout << "METRIC reliability_score_v8=" << combined << "\n";
  std::cout << "METRIC reliability_score_v7=" << combined << "\n";
  std::cout << "METRIC reliability_score_v6=" << combined << "\n";
  std::cout << "METRIC reliability_score_v5=" << combined << "\n";
  std::cout << "METRIC reliability_score_v4=" << combined << "\n";
  std::cout << "METRIC reliability_score=" << combined << "\n";
  std::cout << "METRIC crc_gate_present=0\n";
  return 0;
}
