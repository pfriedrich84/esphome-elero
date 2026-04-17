#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"
mkdir -p .autoresearch

cat > .autoresearch/reliability_probe.cpp <<'CPP'
#include <iostream>
#include "elero/elero_dispatch_logic.h"
#include "elero/elero_packet_validation.h"

using namespace esphome::elero::dispatch_logic;
using namespace esphome::elero::packet_validation;

int main() {
  int dispatch_total = 0;
  int dispatch_failed = 0;
  int packet_total = 0;
  int packet_failed = 0;

  auto dcheck = [&](bool cond) {
    dispatch_total++;
    if (!cond) dispatch_failed++;
  };
  auto pcheck = [&](bool cond) {
    packet_total++;
    if (!cond) packet_failed++;
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

  // Need at least bytes for payload[0..1] + encrypted payload bytes before decode.
  // length=26 with dests_len=0 is too short and should be rejected.
  pcheck(!is_valid_packet_bounds(26, 0));
  pcheck(is_valid_packet_bounds(28, 0));

  int total = dispatch_total + packet_total;
  int failed = dispatch_failed + packet_failed;
  int passed = total - failed;

  double dispatch_score = dispatch_total ? (100.0 * (dispatch_total - dispatch_failed) / dispatch_total) : 0.0;
  double packet_score = packet_total ? (100.0 * (packet_total - packet_failed) / packet_total) : 0.0;
  double combined = total ? (100.0 * passed / total) : 0.0;

  std::cout << "checks=" << total << " failed=" << failed << " passed=" << passed << "\n";
  std::cout << "METRIC reliability_score_v2=" << dispatch_score << "\n";
  std::cout << "METRIC packet_score=" << packet_score << "\n";
  std::cout << "METRIC failed_checks=" << failed << "\n";
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
