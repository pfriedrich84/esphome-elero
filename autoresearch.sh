#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"
mkdir -p .autoresearch

cat > .autoresearch/reliability_probe.cpp <<'CPP'
#include <iostream>
#include "elero/elero_dispatch_logic.h"

using namespace esphome::elero::dispatch_logic;

int main() {
  int total = 0;
  int failed = 0;

  auto check = [&](bool cond) {
    total++;
    if (!cond) failed++;
  };

  // Dispatch should be ready exactly at delay boundary to avoid 1-tick latency drift.
  check(is_dispatch_ready(false, 100, 0, 100));

  // Queue should be considered stale at max-age boundary.
  check(should_clear_stale_queue(1000, 0, 1000));

  // Retry backoff should be monotonic and capped at retry>=3 level.
  check(calculate_dispatch_delay(50, 1) < calculate_dispatch_delay(50, 2));
  check(calculate_dispatch_delay(50, 2) < calculate_dispatch_delay(50, 3));
  check(calculate_dispatch_delay(50, 3) == calculate_dispatch_delay(50, 8));

  // Stop/check commands must not be deferred in stop-urgent mode.
  check(!should_defer_for_stop(true, 0x03, 0x03, 0x04));
  check(!should_defer_for_stop(true, 0x04, 0x03, 0x04));

  // At retry ceiling, command should be dropped to prevent queue starvation.
  check(!should_drop_after_retries(2, 3));
  check(should_drop_after_retries(3, 3));

  int passed = total - failed;
  double score = total ? (100.0 * passed / total) : 0.0;

  std::cout << "checks=" << total << " failed=" << failed << " passed=" << passed << "\n";
  std::cout << "METRIC reliability_score_v2=" << score << "\n";
  std::cout << "METRIC failed_checks=" << failed << "\n";
  return 0;
}
CPP

g++ -std=c++17 -Icomponents .autoresearch/reliability_probe.cpp -o .autoresearch/reliability_probe

start=$(date +%s)
probe_out=$(.autoresearch/reliability_probe)
printf '%s\n' "$probe_out"

base_score=$(printf '%s\n' "$probe_out" | awk -F= '/METRIC reliability_score_v2=/{print $2}')
if rg -n "crc\s*==\s*0" components/elero/elero_protocol.cpp >/dev/null 2>&1; then
  crc_gate_present=1
else
  crc_gate_present=0
fi

combined_score=$(python3 - <<PY
base = float("${base_score:-0}")
crc = int("${crc_gate_present}")
print(round((base * 9.0 + (100.0 if crc else 0.0)) / 10.0, 4))
PY
)

end=$(date +%s)

echo "METRIC reliability_score_v3=${combined_score}"
echo "METRIC reliability_score=${combined_score}"
echo "METRIC crc_gate_present=${crc_gate_present}"
echo "METRIC unit_seconds=$((end - start))"
