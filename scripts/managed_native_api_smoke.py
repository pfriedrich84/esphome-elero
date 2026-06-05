#!/usr/bin/env python3
"""Smoke-test Elero managed-mode ESPHome Native API services through Home Assistant.

The script calls the ESPHome custom services exposed in Home Assistant and checks
managed diagnostic text sensors. It intentionally uses only the Python standard
library.

Example:
  HA_TOKEN=... python3 scripts/managed_native_api_smoke.py \
    --ha-url http://homeassistant.local:8123 \
    --node-name test-managed-minimal \
    --hub-id 123456789 \
    --current-revision 0 \
    --revision-entity text_sensor.elero_managed_registry_revision \
    --device-count-entity text_sensor.elero_managed_device_count \
    --push

After reboot/OTA, verify persisted diagnostics without pushing again:
  HA_TOKEN=... python3 scripts/managed_native_api_smoke.py \
    --ha-url http://homeassistant.local:8123 \
    --node-name test-managed-minimal \
    --revision-entity text_sensor.elero_managed_registry_revision \
    --device-count-entity text_sensor.elero_managed_device_count \
    --expect-revision 1 \
    --expect-device-count 1 \
    --verify-only
"""

from __future__ import annotations

import argparse
import json
import os
import re
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any
from urllib.error import HTTPError, URLError
from urllib.parse import urljoin
from urllib.request import Request, urlopen

FNV_OFFSET = 2166136261
FNV_PRIME = 16777619
SCHEMA_VERSION = 1
DEFAULT_POLL_INTERVAL_MS = 300_000


@dataclass(frozen=True)
class HomeAssistantClient:
    base_url: str
    token: str
    timeout: float

    def request_json(self, method: str, path: str, payload: dict[str, Any] | None = None) -> Any:
        data = None if payload is None else json.dumps(payload).encode()
        request = Request(
            urljoin(self.base_url.rstrip("/") + "/", path.lstrip("/")),
            data=data,
            method=method,
            headers={
                "Authorization": f"Bearer {self.token}",
                "Content-Type": "application/json",
            },
        )
        try:
            with urlopen(request, timeout=self.timeout) as response:  # noqa: S310 - user supplied HA URL
                body = response.read().decode()
        except HTTPError as err:
            raise SystemExit(f"Home Assistant HTTP {err.code} for {path}: {err.read().decode()}") from err
        except URLError as err:
            raise SystemExit(f"Cannot connect to Home Assistant: {err}") from err
        return json.loads(body) if body else None

    def call_service(self, domain: str, service: str, payload: dict[str, Any] | None = None) -> Any:
        return self.request_json("POST", f"/api/services/{domain}/{service}", payload or {})

    def state(self, entity_id: str) -> str | None:
        data = self.request_json("GET", f"/api/states/{entity_id}")
        return None if not isinstance(data, dict) else str(data.get("state"))


def fnv_update(hash_value: int, value: int) -> int:
    hash_value ^= value & 0xFF
    return (hash_value * FNV_PRIME) & 0xFFFFFFFF


def fnv_u16(hash_value: int, value: int) -> int:
    hash_value = fnv_update(hash_value, value)
    return fnv_update(hash_value, value >> 8)


def fnv_u32(hash_value: int, value: int) -> int:
    for shift in (0, 8, 16, 24):
        hash_value = fnv_update(hash_value, value >> shift)
    return hash_value


def fnv_string(hash_value: int, value: str) -> int:
    encoded = value.encode()
    hash_value = fnv_u32(hash_value, len(encoded))
    for byte in encoded:
        hash_value = fnv_update(hash_value, byte)
    return hash_value


def calculate_checksum(registry: dict[str, Any]) -> int:
    hash_value = FNV_OFFSET
    hash_value = fnv_u16(hash_value, int(registry["schema_version"]))
    hash_value = fnv_u32(hash_value, int(registry["registry_revision"]))
    hash_value = fnv_u32(hash_value, int(registry["hub_id"]))
    devices = registry["devices"]
    hash_value = fnv_u32(hash_value, len(devices))
    for device in devices:
        hash_value = fnv_update(hash_value, 1 if device.get("enabled", True) else 0)
        hash_value = fnv_update(hash_value, 1 if device.get("type") == "light" else 0)
        hash_value = fnv_string(hash_value, str(device["name"]))
        hash_value = fnv_u32(hash_value, int(device["blind_address"]))
        hash_value = fnv_u32(hash_value, int(device["remote_address"]))
        hash_value = fnv_update(hash_value, int(device["channel"]))
        pck_inf = device.get("pck_inf", [0, 0])
        hash_value = fnv_update(hash_value, int(pck_inf[0]))
        hash_value = fnv_update(hash_value, int(pck_inf[1]))
        hash_value = fnv_update(hash_value, int(device.get("hop", 0)))
        payload = list(device.get("payload", []))[:10]
        payload.extend([0] * (10 - len(payload)))
        for value in payload:
            hash_value = fnv_update(hash_value, int(value))
        hash_value = fnv_u32(hash_value, int(device.get("open_duration_ms", 0)))
        hash_value = fnv_u32(hash_value, int(device.get("close_duration_ms", 0)))
        hash_value = fnv_u32(hash_value, int(device.get("dim_duration_ms", 0)))
        hash_value = fnv_u32(hash_value, int(device.get("poll_interval_ms", DEFAULT_POLL_INTERVAL_MS)))
        hash_value = fnv_update(hash_value, 1 if device.get("supports_tilt", False) else 0)
    return hash_value or 1


def slug(value: str) -> str:
    return re.sub(r"_+", "_", re.sub(r"[^a-z0-9_]", "_", value.lower())).strip("_")


def build_registry(args: argparse.Namespace) -> dict[str, Any]:
    registry = {
        "schema_version": SCHEMA_VERSION,
        "registry_revision": args.current_revision,
        "hub_id": args.hub_id,
        "devices": [
            {
                "enabled": True,
                "type": args.device_type,
                "name": args.device_name,
                "blind_address": args.blind_address,
                "remote_address": args.remote_address,
                "channel": args.channel,
                "pck_inf": [args.pck_inf0, args.pck_inf1],
                "hop": args.hop,
                "payload": args.payload,
                "open_duration_ms": args.open_duration_ms,
                "close_duration_ms": args.close_duration_ms,
                "dim_duration_ms": args.dim_duration_ms,
                "poll_interval_ms": args.poll_interval_ms,
                "supports_tilt": args.supports_tilt,
            }
        ],
    }
    registry["checksum"] = calculate_checksum(registry)
    return registry


def wait_for_state(client: HomeAssistantClient, entity_id: str, expected: str, timeout: float) -> None:
    deadline = time.monotonic() + timeout
    last_state = None
    while time.monotonic() < deadline:
        last_state = client.state(entity_id)
        if last_state == expected:
            print(f"OK {entity_id}={expected}")
            return
        time.sleep(1)
    raise SystemExit(f"Timed out waiting for {entity_id}={expected}; last state was {last_state!r}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ha-url", required=True)
    parser.add_argument("--token-env", default="HA_TOKEN")
    parser.add_argument("--node-name", required=True, help="ESPHome node name; used to derive esphome service names")
    parser.add_argument("--service-domain", default="esphome")
    parser.add_argument("--hub-id", type=int)
    parser.add_argument("--current-revision", type=int)
    parser.add_argument("--revision-entity")
    parser.add_argument("--device-count-entity")
    parser.add_argument("--last-ok-entity")
    parser.add_argument("--raw-response-entity")
    parser.add_argument("--push", action="store_true", help="Actually push the one-device registry after validation")
    parser.add_argument("--clear", action="store_true", help="Clear the managed registry after stale-revision validation")
    parser.add_argument("--verify-only", action="store_true", help="Only call get_elero_info and verify diagnostic sensor states")
    parser.add_argument("--expect-revision", type=int, help="Expected registry revision for persistence checks")
    parser.add_argument("--expect-device-count", type=int, help="Expected device count for persistence checks")
    parser.add_argument("--check-stale-revision", action="store_true", help="Validate a mismatched revision and expect Last OK=false")
    parser.add_argument("--timeout", type=float, default=20)
    parser.add_argument("--registry-json-file", type=Path, help="Use an explicit managed registry JSON file instead of the generated one-device registry")
    parser.add_argument("--refresh-checksum", action="store_true", help="Refresh checksum for --registry-json-file before validate/push")
    parser.add_argument("--device-type", choices=["cover", "light"], default="cover")
    parser.add_argument("--device-name", default="Managed Smoke Test")
    parser.add_argument("--blind-address", type=lambda x: int(x, 0), default=0xA831E5)
    parser.add_argument("--remote-address", type=lambda x: int(x, 0), default=0xF0D008)
    parser.add_argument("--channel", type=int, default=4)
    parser.add_argument("--pck-inf0", type=lambda x: int(x, 0), default=0x6A)
    parser.add_argument("--pck-inf1", type=lambda x: int(x, 0), default=0)
    parser.add_argument("--hop", type=lambda x: int(x, 0), default=1)
    parser.add_argument("--payload", type=lambda value: [int(item, 0) for item in value.split(",")], default=[1, 2, 0, 0, 0, 0, 0, 0, 0, 0])
    parser.add_argument("--open-duration-ms", type=int, default=25_000)
    parser.add_argument("--close-duration-ms", type=int, default=26_000)
    parser.add_argument("--dim-duration-ms", type=int, default=0)
    parser.add_argument("--poll-interval-ms", type=int, default=DEFAULT_POLL_INTERVAL_MS)
    parser.add_argument("--supports-tilt", action="store_true")
    args = parser.parse_args()

    token = os.environ.get(args.token_env)
    if not token:
        raise SystemExit(f"Set {args.token_env} to a Home Assistant long-lived access token")

    client = HomeAssistantClient(args.ha_url, token, args.timeout)
    service_prefix = slug(args.node_name)

    client.call_service(args.service_domain, f"{service_prefix}_get_elero_info")
    print("Called get_elero_info")

    if args.expect_revision is not None:
        if not args.revision_entity:
            raise SystemExit("--expect-revision requires --revision-entity")
        wait_for_state(client, args.revision_entity, str(args.expect_revision), args.timeout)
    if args.expect_device_count is not None:
        if not args.device_count_entity:
            raise SystemExit("--expect-device-count requires --device-count-entity")
        wait_for_state(client, args.device_count_entity, str(args.expect_device_count), args.timeout)
    if args.verify_only:
        print("Verify-only check complete")
        return

    if args.clear:
        if args.current_revision is None:
            raise SystemExit("--clear requires --current-revision")
        client.call_service(
            args.service_domain,
            f"{service_prefix}_clear_elero_managed_registry",
            {"registry_revision": str(args.current_revision), "confirm": "CLEAR"},
        )
        print("Called clear_elero_managed_registry")
        if args.revision_entity:
            wait_for_state(client, args.revision_entity, str(args.current_revision + 1), args.timeout)
        if args.device_count_entity:
            wait_for_state(client, args.device_count_entity, "0", args.timeout)
        return

    if args.registry_json_file:
        registry = json.loads(args.registry_json_file.read_text())
        if not isinstance(registry, dict):
            raise SystemExit("--registry-json-file must contain a JSON object")
        if args.refresh_checksum:
            registry["checksum"] = calculate_checksum(registry)
    else:
        if args.hub_id is None or args.current_revision is None:
            raise SystemExit("--hub-id and --current-revision are required unless --verify-only, --clear, or --registry-json-file is used")
        registry = build_registry(args)
    active_revision = args.current_revision
    if active_revision is None:
        active_revision = int(registry.get("registry_revision", 0))
    registry_json = json.dumps(registry, separators=(",", ":"))
    print(f"registry_json={registry_json}")

    client.call_service(
        args.service_domain,
        f"{service_prefix}_validate_elero_managed_registry",
        {"registry_json": registry_json},
    )
    print("Called validate_elero_managed_registry")

    if args.raw_response_entity:
        time.sleep(1)
        print(f"{args.raw_response_entity}={client.state(args.raw_response_entity)!r}")

    if args.check_stale_revision:
        stale_registry = dict(registry)
        stale_registry["registry_revision"] = active_revision + 1
        stale_registry["checksum"] = calculate_checksum(stale_registry)
        client.call_service(
            args.service_domain,
            f"{service_prefix}_validate_elero_managed_registry",
            {"registry_json": json.dumps(stale_registry, separators=(",", ":"))},
        )
        print("Called stale validate_elero_managed_registry")
        if args.last_ok_entity:
            wait_for_state(client, args.last_ok_entity, "false", args.timeout)

    if not args.push:
        print("Dry run complete; pass --push to persist the registry or --clear to clear it")
        return

    client.call_service(
        args.service_domain,
        f"{service_prefix}_push_elero_managed_registry",
        {"registry_json": registry_json},
    )
    print("Called push_elero_managed_registry")

    if args.revision_entity:
        wait_for_state(client, args.revision_entity, str(active_revision + 1), args.timeout)
    if args.device_count_entity:
        wait_for_state(client, args.device_count_entity, "1", args.timeout)


if __name__ == "__main__":
    main()
