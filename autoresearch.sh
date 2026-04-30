#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"
mkdir -p .autoresearch

cat > .autoresearch/reliability_probe.cpp <<'CPP'
#include <iostream>
#include "elero/elero_dispatch_logic.h"
#include "elero/elero_packet_validation.h"
#include "elero/elero_radio_state_logic.h"
#include "elero/elero_cover_logic.h"
#include "elero/elero_light_logic.h"
#include "elero/elero_poll_logic.h"
#include "elero/elero_runtime_blind_logic.h"
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
#if __has_include("elero/elero_dedup_logic.h")
#include "elero/elero_dedup_logic.h"
#define HAS_DEDUP_HELPER 1
#else
#define HAS_DEDUP_HELPER 0
#endif

using namespace esphome::elero::dispatch_logic;
using namespace esphome::elero::packet_validation;

namespace {
#ifdef ELERO_HAS_CRC_STATUS_HELPER
constexpr bool kHasCrcHelper = true;
#else
constexpr bool kHasCrcHelper = false;
#endif
#ifdef ELERO_HAS_RECOVERY_OUTCOME_HELPER
constexpr bool kHasRecoveryOutcomeHelper = true;
#else
constexpr bool kHasRecoveryOutcomeHelper = false;
#endif
#ifdef ELERO_HAS_TX_ADDR_HELPER
constexpr bool kHasTxAddrHelper = true;
#else
constexpr bool kHasTxAddrHelper = false;
#endif
#ifdef ELERO_HAS_TX_EFFECTIVE_DESTS_HELPER
constexpr bool kHasTxEffectiveDestsHelper = true;
#else
constexpr bool kHasTxEffectiveDestsHelper = false;
#endif
#ifdef ELERO_HAS_DISPATCH_DROP_COUNTER_HELPER
constexpr bool kHasDispatchDropCounterHelper = true;
#else
constexpr bool kHasDispatchDropCounterHelper = false;
#endif
#ifdef ELERO_HAS_DISPATCH_DROP_STALE_REFRESH_HELPER
constexpr bool kHasDispatchDropStaleRefreshHelper = true;
#else
constexpr bool kHasDispatchDropStaleRefreshHelper = false;
#endif
#ifdef ELERO_HAS_DISPATCH_QUEUE_FULL_DROP_CLEAR_HELPER
constexpr bool kHasDispatchQueueFullDropClearHelper = true;
#else
constexpr bool kHasDispatchQueueFullDropClearHelper = false;
#endif
#ifdef ELERO_HAS_COVER_POLL_HELPER
constexpr bool kHasCoverPollHelper = true;
#else
constexpr bool kHasCoverPollHelper = false;
#endif
#ifdef ELERO_HAS_COVER_PUBLISH_HELPER
constexpr bool kHasCoverPublishHelper = true;
#else
constexpr bool kHasCoverPublishHelper = false;
#endif
#ifdef ELERO_HAS_COVER_IMMEDIATE_POLL_HELPER
constexpr bool kHasCoverImmediatePollHelper = true;
#else
constexpr bool kHasCoverImmediatePollHelper = false;
#endif
#ifdef ELERO_HAS_COVER_MOVEMENT_QUEUE_HELPER
constexpr bool kHasCoverMovementQueueHelper = true;
#else
constexpr bool kHasCoverMovementQueueHelper = false;
#endif
#ifdef ELERO_HAS_COVER_TILT_QUEUE_HELPER
constexpr bool kHasCoverTiltQueueHelper = true;
#else
constexpr bool kHasCoverTiltQueueHelper = false;
#endif
#ifdef ELERO_HAS_COVER_STOP_PRIORITY_HELPER
constexpr bool kHasCoverStopPriorityHelper = true;
#else
constexpr bool kHasCoverStopPriorityHelper = false;
#endif
#ifdef ELERO_HAS_COVER_AUTO_STOP_PRIORITY_HELPER
constexpr bool kHasCoverAutoStopPriorityHelper = true;
#else
constexpr bool kHasCoverAutoStopPriorityHelper = false;
#endif
#ifdef ELERO_HAS_LIGHT_PUBLISH_HELPER
constexpr bool kHasLightPublishHelper = true;
#else
constexpr bool kHasLightPublishHelper = false;
#endif
#ifdef ELERO_HAS_LIGHT_IMMEDIATE_POLL_HELPER
constexpr bool kHasLightImmediatePollHelper = true;
#else
constexpr bool kHasLightImmediatePollHelper = false;
#endif
#ifdef ELERO_HAS_LIGHT_ACTION_QUEUE_HELPER
constexpr bool kHasLightActionQueueHelper = true;
#else
constexpr bool kHasLightActionQueueHelper = false;
#endif
#ifdef ELERO_HAS_LIGHT_ACTION_PRECHECK_HELPER
constexpr bool kHasLightActionPrecheckHelper = true;
#else
constexpr bool kHasLightActionPrecheckHelper = false;
#endif
#ifdef ELERO_HAS_POLL_INCLUSIVE_HELPER
constexpr bool kHasPollInclusiveHelper = true;
#else
constexpr bool kHasPollInclusiveHelper = false;
#endif
#ifdef ELERO_HAS_TX_TIMEOUT_HELPER
constexpr bool kHasTxTimeoutHelper = true;
#else
constexpr bool kHasTxTimeoutHelper = false;
#endif
#ifdef ELERO_HAS_WATCHDOG_ACTION_HELPER
constexpr bool kHasWatchdogActionHelper = true;
#else
constexpr bool kHasWatchdogActionHelper = false;
#endif
#ifdef ELERO_HAS_RUNTIME_STALE_COUNTER_HELPER
constexpr bool kHasRuntimeStaleCounterHelper = true;
#else
constexpr bool kHasRuntimeStaleCounterHelper = false;
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
  int tx_packet_total = 0;
  int tx_packet_failed = 0;
  int dedup_total = 0;
  int dedup_failed = 0;
  int radio_total = 0;
  int radio_failed = 0;
  int cover_total = 0;
  int cover_failed = 0;
  int light_total = 0;
  int light_failed = 0;
  int poll_total = 0;
  int poll_failed = 0;
  int runtime_total = 0;
  int runtime_failed = 0;

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
  auto tpcheck = [&](bool cond) {
    tx_packet_total++;
    if (!cond) tx_packet_failed++;
  };
  auto ddcheck = [&](bool cond) {
    dedup_total++;
    if (!cond) dedup_failed++;
  };
  auto rscheck = [&](bool cond) {
    radio_total++;
    if (!cond) radio_failed++;
  };
  auto ccheck = [&](bool cond) {
    cover_total++;
    if (!cond) cover_failed++;
  };
  auto lcheck = [&](bool cond) {
    light_total++;
    if (!cond) light_failed++;
  };
  auto pollcheck = [&](bool cond) {
    poll_total++;
    if (!cond) poll_failed++;
  };
  auto rtcheck = [&](bool cond) {
    runtime_total++;
    if (!cond) runtime_failed++;
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
  dcheck(kHasDispatchDropCounterHelper);
#ifdef ELERO_HAS_DISPATCH_DROP_COUNTER_HELPER
  dcheck(!should_advance_counter_on_drop(0));
  dcheck(should_advance_counter_on_drop(1));
#endif
  dcheck(kHasDispatchDropStaleRefreshHelper);
#ifdef ELERO_HAS_DISPATCH_DROP_STALE_REFRESH_HELPER
  dcheck(should_refresh_stale_timer_on_drop(true));
  dcheck(!should_refresh_stale_timer_on_drop(false));
#endif
  dcheck(kHasDispatchQueueFullDropClearHelper);
#ifdef ELERO_HAS_DISPATCH_QUEUE_FULL_DROP_CLEAR_HELPER
  dcheck(should_clear_queue_full_latch_after_drop(true, true));
  dcheck(!should_clear_queue_full_latch_after_drop(false, true));
  dcheck(!should_clear_queue_full_latch_after_drop(true, false));
#endif

  // Packet validation reliability invariants.
  pcheck(is_valid_packet_length(17));
  pcheck(!is_valid_packet_length(16));
  pcheck(is_valid_dest_count(max_safe_dests()));
  pcheck(!is_valid_dest_count(max_safe_dests() + 1));
  pcheck(!is_valid_dest_count(0));

  // Payload bounds: the full 10-byte payload is copied starting at
  // index (19 + dests_len), so payload[9] at (28 + dests_len) must be
  // within the declared packet length before decode.
  pcheck(is_valid_packet_bounds(28, 0));
  pcheck(!is_valid_packet_bounds(27, 0));
  pcheck(is_valid_packet_bounds(31, 3));
  pcheck(!is_valid_packet_bounds(30, 3));

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
  wcheck(kHasWatchdogActionHelper);
#ifdef ELERO_HAS_WATCHDOG_ACTION_HELPER
  using esphome::elero::watchdog_logic::RecoveryAction;
  wcheck(esphome::elero::watchdog_logic::choose_recovery_action(CC1101_MARCSTATE_IDLE, 3, 3, 3, 3) == RecoveryAction::RESTART_RX);
  wcheck(esphome::elero::watchdog_logic::choose_recovery_action(CC1101_MARCSTATE_RXFIFO_OFLOW, 0, 0, 3, 3) == RecoveryAction::FLUSH_RX);
  wcheck(esphome::elero::watchdog_logic::choose_recovery_action(CC1101_MARCSTATE_TXFIFO_UFLOW, 0, 0, 3, 3) == RecoveryAction::FLUSH_AND_RX);
  wcheck(esphome::elero::watchdog_logic::choose_recovery_action(CC1101_MARCSTATE_TXFIFO_UFLOW, 3, 0, 3, 3) == RecoveryAction::RESET);
  wcheck(esphome::elero::watchdog_logic::choose_recovery_action(CC1101_MARCSTATE_TXFIFO_UFLOW, 3, 3, 3, 3) == RecoveryAction::FAIL);
#endif
#endif

  // Recovery helper invariants for reinit-failure counting + window boundaries.
  rcheck(HAS_RECOVERY_HELPER == 1);
#if HAS_RECOVERY_HELPER
  rcheck(esphome::elero::recovery_logic::next_reinit_failure_count(0, false) == 1);
  rcheck(esphome::elero::recovery_logic::next_reinit_failure_count(1, false) == 2);
  rcheck(esphome::elero::recovery_logic::next_reinit_failure_count(1, true) == 1);
  rcheck(kHasRecoveryOutcomeHelper);
#ifdef ELERO_HAS_RECOVERY_OUTCOME_HELPER
  rcheck(esphome::elero::recovery_logic::apply_reinit_outcome(2, true) == 0);
  rcheck(esphome::elero::recovery_logic::apply_reinit_outcome(2, false) == 2);
#endif

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
  tcheck(kHasTxAddrHelper);
  tcheck(kHasTxEffectiveDestsHelper);
#ifdef ELERO_HAS_TX_ADDR_HELPER
  uint32_t addrs1[10] = {0x010203, 0x0A0B0C, 0, 0, 0, 0, 0, 0, 0, 0};
  tcheck(esphome::elero::tx_logic::count_nonzero_dest_addrs(addrs1, 10) == 2);
  uint32_t addrs2[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  tcheck(esphome::elero::tx_logic::count_nonzero_dest_addrs(addrs2, 10) == 0);
#endif
#ifdef ELERO_HAS_TX_EFFECTIVE_DESTS_HELPER
  tcheck(esphome::elero::tx_logic::effective_num_dests(5, 2) == 2);
  tcheck(esphome::elero::tx_logic::effective_num_dests(2, 5) == 2);
  tcheck(esphome::elero::tx_logic::effective_num_dests(0, 5) == 0);
#endif
#endif

  // TX packet-length invariants.
#if HAS_TX_HELPER
  tpcheck(esphome::elero::tx_logic::calculate_msg_len(1) == 29);
  tpcheck(esphome::elero::tx_logic::calculate_msg_len(10) == 56);
  tpcheck(esphome::elero::tx_logic::calculate_msg_len(255) > 57);
  tpcheck(esphome::elero::tx_logic::is_msg_len_valid(esphome::elero::tx_logic::calculate_msg_len(10), 57));
#else
  tpcheck(false);
#endif

  // Dedup helper invariants.
  ddcheck(HAS_DEDUP_HELPER == 1);
#if HAS_DEDUP_HELPER
  ddcheck(esphome::elero::dedup_logic::is_duplicate_within_window(1000, 500, 600));
  ddcheck(!esphome::elero::dedup_logic::is_duplicate_within_window(1100, 500, 600));
  ddcheck(esphome::elero::dedup_logic::should_prune_entry(1100, 500, 600));
  ddcheck(!esphome::elero::dedup_logic::should_prune_entry(1099, 500, 600));
#endif

  // Cover polling invariants: scheduled polls should fire at the exact
  // interval boundary rather than waiting for an extra loop tick.
  ccheck(kHasCoverPollHelper);
#ifdef ELERO_HAS_COVER_POLL_HELPER
  ccheck(esphome::elero::cover_logic::should_poll(1000, 0, 1000));
  ccheck(!esphome::elero::cover_logic::should_poll(999, 0, 1000));
#endif
  ccheck(kHasCoverPublishHelper);
#ifdef ELERO_HAS_COVER_PUBLISH_HELPER
  ccheck(esphome::elero::cover_logic::should_publish_position(1000, 0, 1000));
  ccheck(!esphome::elero::cover_logic::should_publish_position(999, 0, 1000));
#endif
  ccheck(kHasCoverImmediatePollHelper);
#ifdef ELERO_HAS_COVER_IMMEDIATE_POLL_HELPER
  ccheck(!esphome::elero::cover_logic::should_schedule_immediate_poll(0x00, 0, 10, 1000, 0, 1000));
  ccheck(!esphome::elero::cover_logic::should_schedule_immediate_poll(0x04, 10, 10, 1000, 0, 1000));
  ccheck(!esphome::elero::cover_logic::should_schedule_immediate_poll(0x04, 0, 10, 999, 0, 1000));
  ccheck(esphome::elero::cover_logic::should_schedule_immediate_poll(0x04, 0, 10, 1000, 0, 1000));
#endif
  ccheck(kHasCoverMovementQueueHelper);
#ifdef ELERO_HAS_COVER_MOVEMENT_QUEUE_HELPER
  ccheck(!esphome::elero::cover_logic::can_enqueue_movement(10, 10));
  ccheck(esphome::elero::cover_logic::can_enqueue_movement(9, 10));
  ccheck(esphome::elero::cover_logic::should_reject_movement_before_state_update(10, 10));
  ccheck(!esphome::elero::cover_logic::should_reject_movement_before_state_update(9, 10));
#endif
  ccheck(kHasCoverTiltQueueHelper);
#ifdef ELERO_HAS_COVER_TILT_QUEUE_HELPER
  ccheck(!esphome::elero::cover_logic::can_enqueue_tilt_command(10, 10));
  ccheck(esphome::elero::cover_logic::can_enqueue_tilt_command(9, 10));
  ccheck(esphome::elero::cover_logic::should_publish_queue_full_for_tilt(10, 10));
  ccheck(!esphome::elero::cover_logic::should_publish_queue_full_for_tilt(9, 10));
#endif
  ccheck(kHasCoverStopPriorityHelper);
#ifdef ELERO_HAS_COVER_STOP_PRIORITY_HELPER
  ccheck(esphome::elero::cover_logic::should_apply_stop_after_priority_result(true));
  ccheck(!esphome::elero::cover_logic::should_apply_stop_after_priority_result(false));
  ccheck(esphome::elero::cover_logic::should_schedule_stop_verification(true));
  ccheck(!esphome::elero::cover_logic::should_schedule_stop_verification(false));
#endif
  ccheck(kHasCoverAutoStopPriorityHelper);
#ifdef ELERO_HAS_COVER_AUTO_STOP_PRIORITY_HELPER
  ccheck(esphome::elero::cover_logic::should_apply_auto_stop_after_priority_result(true));
  ccheck(!esphome::elero::cover_logic::should_apply_auto_stop_after_priority_result(false));
#endif

  // Light dimming publish invariants: estimated brightness should publish at
  // the exact interval boundary rather than waiting for an extra loop tick.
  lcheck(kHasLightPublishHelper);
#ifdef ELERO_HAS_LIGHT_PUBLISH_HELPER
  lcheck(esphome::elero::light_logic::should_publish_dimming(1000, 0, 1000));
  lcheck(!esphome::elero::light_logic::should_publish_dimming(999, 0, 1000));
#endif
  lcheck(kHasLightImmediatePollHelper);
#ifdef ELERO_HAS_LIGHT_IMMEDIATE_POLL_HELPER
  lcheck(!esphome::elero::light_logic::should_schedule_immediate_poll(0x00, 0, 10, 1000, 0, 1000));
  lcheck(!esphome::elero::light_logic::should_schedule_immediate_poll(0x04, 10, 10, 1000, 0, 1000));
  lcheck(!esphome::elero::light_logic::should_schedule_immediate_poll(0x04, 0, 10, 999, 0, 1000));
  lcheck(esphome::elero::light_logic::should_schedule_immediate_poll(0x04, 0, 10, 1000, 0, 1000));
#endif
  lcheck(kHasLightActionQueueHelper);
#ifdef ELERO_HAS_LIGHT_ACTION_QUEUE_HELPER
  using esphome::elero::light_logic::LightAction;
  lcheck(esphome::elero::light_logic::required_command_slots(LightAction::NONE) == 0);
  lcheck(esphome::elero::light_logic::required_command_slots(LightAction::ON_THEN_DIM_DOWN) == 2);
  lcheck(!esphome::elero::light_logic::can_enqueue_action(LightAction::ON_THEN_DIM_DOWN, 9, 10));
  lcheck(esphome::elero::light_logic::can_enqueue_action(LightAction::ON_THEN_DIM_DOWN, 8, 10));
#endif
  lcheck(kHasLightActionPrecheckHelper);
#ifdef ELERO_HAS_LIGHT_ACTION_PRECHECK_HELPER
  lcheck(esphome::elero::light_logic::should_reject_action_before_state_update(LightAction::OFF, 10, 10));
  lcheck(esphome::elero::light_logic::should_reject_action_before_state_update(LightAction::ON, 10, 10));
  lcheck(esphome::elero::light_logic::should_reject_action_before_state_update(LightAction::DIM_UP, 10, 10));
  lcheck(!esphome::elero::light_logic::should_reject_action_before_state_update(LightAction::NONE, 10, 10));
#endif

  // Runtime blind stale-clear invariants: dropping a partially transmitted
  // runtime command queue should advance the rolling counter to avoid reuse.
  rtcheck(kHasRuntimeStaleCounterHelper);
#ifdef ELERO_HAS_RUNTIME_STALE_COUNTER_HELPER
  rtcheck(!esphome::elero::runtime_blind_logic::should_advance_counter_on_stale_clear(0));
  rtcheck(esphome::elero::runtime_blind_logic::should_advance_counter_on_stale_clear(1));
#endif

  // Generic poll helper invariants: shared polling decisions should use the
  // same inclusive exact-boundary semantics as cover/runtime polling.
  pollcheck(kHasPollInclusiveHelper);
  pollcheck(esphome::elero::poll_logic::should_poll_now(1000, 0, 1000));
  pollcheck(!esphome::elero::poll_logic::should_poll_now(999, 0, 1000));

  // Radio state helper invariants: TX/RX switch states are short-lived
  // hardware transitions and should not consume watchdog escalation budget.
  rscheck(kHasTxTimeoutHelper);
#ifdef ELERO_HAS_TX_TIMEOUT_HELPER
  rscheck(esphome::elero::radio_state_logic::has_tx_timed_out(50, 50));
  rscheck(!esphome::elero::radio_state_logic::has_tx_timed_out(49, 50));
#endif
  rscheck(esphome::elero::radio_state_logic::is_watchdog_transient_state(CC1101_MARCSTATE_TXRX_SWITCH));
  rscheck(esphome::elero::radio_state_logic::is_watchdog_transient_state(CC1101_MARCSTATE_RXTX_SWITCH));
  rscheck(!esphome::elero::radio_state_logic::is_watchdog_transient_state(CC1101_MARCSTATE_RXFIFO_OFLOW));
  rscheck(!esphome::elero::radio_state_logic::is_watchdog_transient_state(CC1101_MARCSTATE_TXFIFO_UFLOW));

  int total = dispatch_total + packet_total + watchdog_total + recovery_total + overflow_total + tx_total + tx_packet_total + dedup_total + radio_total + cover_total + light_total + poll_total + runtime_total;
  int failed = dispatch_failed + packet_failed + watchdog_failed + recovery_failed + overflow_failed + tx_failed + tx_packet_failed + dedup_failed + radio_failed + cover_failed + light_failed + poll_failed + runtime_failed;
  int passed = total - failed;

  double dispatch_score = dispatch_total ? (100.0 * (dispatch_total - dispatch_failed) / dispatch_total) : 0.0;
  double packet_score = packet_total ? (100.0 * (packet_total - packet_failed) / packet_total) : 0.0;
  double watchdog_score = watchdog_total ? (100.0 * (watchdog_total - watchdog_failed) / watchdog_total) : 0.0;
  double recovery_score = recovery_total ? (100.0 * (recovery_total - recovery_failed) / recovery_total) : 0.0;
  double overflow_score = overflow_total ? (100.0 * (overflow_total - overflow_failed) / overflow_total) : 0.0;
  double tx_score = tx_total ? (100.0 * (tx_total - tx_failed) / tx_total) : 0.0;
  double tx_packet_score = tx_packet_total ? (100.0 * (tx_packet_total - tx_packet_failed) / tx_packet_total) : 0.0;
  double dedup_score = dedup_total ? (100.0 * (dedup_total - dedup_failed) / dedup_total) : 0.0;
  double radio_score = radio_total ? (100.0 * (radio_total - radio_failed) / radio_total) : 0.0;
  double cover_score = cover_total ? (100.0 * (cover_total - cover_failed) / cover_total) : 0.0;
  double light_score = light_total ? (100.0 * (light_total - light_failed) / light_total) : 0.0;
  double poll_score = poll_total ? (100.0 * (poll_total - poll_failed) / poll_total) : 0.0;
  double runtime_score = runtime_total ? (100.0 * (runtime_total - runtime_failed) / runtime_total) : 0.0;
  double combined = total ? (100.0 * passed / total) : 0.0;

  std::cout << "checks=" << total << " failed=" << failed << " passed=" << passed << "\n";
  std::cout << "METRIC reliability_score_v2=" << dispatch_score << "\n";
  std::cout << "METRIC packet_score=" << packet_score << "\n";
  std::cout << "METRIC watchdog_score=" << watchdog_score << "\n";
  std::cout << "METRIC recovery_score=" << recovery_score << "\n";
  std::cout << "METRIC overflow_score=" << overflow_score << "\n";
  std::cout << "METRIC tx_score=" << tx_score << "\n";
  std::cout << "METRIC tx_packet_score=" << tx_packet_score << "\n";
  std::cout << "METRIC dedup_score=" << dedup_score << "\n";
  std::cout << "METRIC radio_state_score=" << radio_score << "\n";
  std::cout << "METRIC cover_score=" << cover_score << "\n";
  std::cout << "METRIC light_score=" << light_score << "\n";
  std::cout << "METRIC poll_score=" << poll_score << "\n";
  std::cout << "METRIC runtime_score=" << runtime_score << "\n";
  std::cout << "METRIC failed_checks=" << failed << "\n";
  std::cout << "METRIC reliability_score_v37=" << combined << "\n";
  std::cout << "METRIC reliability_score_v36=" << combined << "\n";
  std::cout << "METRIC reliability_score_v35=" << combined << "\n";
  std::cout << "METRIC reliability_score_v34=" << combined << "\n";
  std::cout << "METRIC reliability_score_v33=" << combined << "\n";
  std::cout << "METRIC reliability_score_v32=" << combined << "\n";
  std::cout << "METRIC reliability_score_v31=" << combined << "\n";
  std::cout << "METRIC reliability_score_v30=" << combined << "\n";
  std::cout << "METRIC reliability_score_v29=" << combined << "\n";
  std::cout << "METRIC reliability_score_v28=" << combined << "\n";
  std::cout << "METRIC reliability_score_v27=" << combined << "\n";
  std::cout << "METRIC reliability_score_v26=" << combined << "\n";
  std::cout << "METRIC reliability_score_v25=" << combined << "\n";
  std::cout << "METRIC reliability_score_v24=" << combined << "\n";
  std::cout << "METRIC reliability_score_v23=" << combined << "\n";
  std::cout << "METRIC reliability_score_v22=" << combined << "\n";
  std::cout << "METRIC reliability_score_v21=" << combined << "\n";
  std::cout << "METRIC reliability_score_v20=" << combined << "\n";
  std::cout << "METRIC reliability_score_v19=" << combined << "\n";
  std::cout << "METRIC reliability_score_v18=" << combined << "\n";
  std::cout << "METRIC reliability_score_v17=" << combined << "\n";
  std::cout << "METRIC reliability_score_v16=" << combined << "\n";
  std::cout << "METRIC reliability_score_v15=" << combined << "\n";
  std::cout << "METRIC reliability_score_v14=" << combined << "\n";
  std::cout << "METRIC reliability_score_v13=" << combined << "\n";
  std::cout << "METRIC reliability_score_v12=" << combined << "\n";
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
CPP

g++ -std=c++17 -Icomponents .autoresearch/reliability_probe.cpp -o .autoresearch/reliability_probe

start=$(date +%s)
.autoresearch/reliability_probe
end=$(date +%s)

echo "METRIC unit_seconds=$((end - start))"
