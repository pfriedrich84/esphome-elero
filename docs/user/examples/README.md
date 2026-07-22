# User Configuration Examples

Copy these examples as starting points and replace Wi-Fi secrets, addresses, and
names for your installation. Light examples are intentionally omitted until the
light workflow is documented and tested more thoroughly.

## Complete starting points

| Example | Use case |
|---|---|
| [`lilygo-minimal.yaml`](lilygo-minimal.yaml) | Lilygo T-Embed CC1101 with onboard radio, board power, antenna switch, dummy cover, and scan buttons |
| [`external-cc1101-minimal.yaml`](external-cc1101-minimal.yaml) | Classic ESP32 with an external CC1101 module on safe SPI pins |

## Discovery examples

| Example | Use case |
|---|---|
| [`discovery-web-ui.yaml`](discovery-web-ui.yaml) | Enable RF discovery through the `/elero` Web-UI with YAML export |
| [`discovery-buttons.yaml`](discovery-buttons.yaml) | RF discovery through Home Assistant buttons and ESPHome logs |

## Feature snippets

| Example | Use case |
|---|---|
| [`cover-position.yaml`](cover-position.yaml) | One roller shutter with timed position control |
| [`cover-tilt.yaml`](cover-tilt.yaml) | One tilt-capable shutter / venetian blind |
| [`group-cover.yaml`](group-cover.yaml) | Group multiple Elero covers under one Home Assistant cover |
| [`diagnostics-custom.yaml`](diagnostics-custom.yaml) | Override auto-generated diagnostics with custom names |

See also:

- [Installation](../installation.md)
- [Configuration Reference](../configuration.md)
- [Discovery](../discovery.md)
- [Common Issues](../common-issues.md)
