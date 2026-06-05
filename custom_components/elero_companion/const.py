"""Constants for the Elero Companion integration."""

from __future__ import annotations

from datetime import timedelta

DOMAIN = "elero_companion"

CONF_URL = "url"
CONF_HOST = "host"
CONF_USERNAME = "username"
CONF_PASSWORD = "password"
CONF_ESPHOME_NODE = "esphome_node"

ESPHOME_SERVICE_DOMAIN = "esphome"
SERVICE_GET_INFO = "get_info"
SERVICE_VALIDATE_MANAGED_REGISTRY = "validate_managed_registry"
SERVICE_PUSH_MANAGED_REGISTRY = "push_managed_registry"
SERVICE_CLEAR_MANAGED_REGISTRY = "clear_managed_registry"

DEFAULT_SCAN_INTERVAL = timedelta(seconds=30)
DEFAULT_TIMEOUT = 10

MANUFACTURER = "Elero / ESPHome"
MODEL = "ESPHome Elero hub"

ATTR_CONFIGURED_COVERS = "configured_covers"
ATTR_CONFIGURED_LIGHTS = "configured_lights"
ATTR_RUNTIME_COVERS = "runtime_covers"
ATTR_RUNTIME_LIGHTS = "runtime_lights"
ATTR_WEB_UI_ENABLED = "web_ui_enabled"
ATTR_YAML_EXPORT = "yaml_export"
