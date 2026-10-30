# API

All endpoints are local HTTP. Never place passwords or tokens in query strings.

## Device settings (ESP32 port 80)

Every route requires HTTP Basic authentication, username `admin`. Use the per-device password on the setup screen. Responses use `Cache-Control: no-store`; secrets are never returned.

| Method | Route | Purpose |
| --- | --- | --- |
| GET | `/` | Embedded bilingual settings page |
| GET | `/api/status` | Connection/config summary, secret-presence flags, CSRF token |
| POST | `/api/scan` | Start async 2.4 GHz scan |
| GET | `/api/scan` | `running`, `ready`, and up to 24 deduplicated networks |
| POST | `/api/config` | Validate, persist, schedule restart |
| POST | `/api/reboot` | Restart device, preserving settings |

All POST requests need `X-CSRF-Token` from authenticated `/api/status`.

Config fields: `ssid` (1–32 bytes), `wifiPassword`, `openNetwork` (boolean), `routerHost` (IPv4 or hostname), `routerPort` (1–65535), `routerToken` (empty or 16–128 characters), `refreshMs` (2000/3000/5000/10000), `brightness` (40–255), `adminPassword` (blank or 12–63 characters). Full validation precedes NVS writes. Blank same-SSID Wi-Fi passwords and blank tokens retain existing values. Marking a network open explicitly clears its Wi-Fi password. Empty initial router token is allowed for Wi-Fi-only setup; polling waits for a token.

Wrong auth returns 401, CSRF mismatch 403, invalid settings 400, oversized settings 413. Successful save returns before the delayed restart. IPv6 literals and TLS router URLs are not supported by v0.1.0 device settings.

## Router agent

`GET /cgi-bin/routerdock?status|clash|ua|traffic` requires `Authorization: Bearer <device-token>`.

- `status`: CPU, memory, temperature, disk/process summary, WAN/LAN, WAN rate delta and Clash connection availability/count.
- `clash&g=1&p=0`: all group names/current selection and only the selected group's four requested node entries. Group index starts at 1; node offset starts at 0. Delay fields include cached active tests or controller history. `ok=false` means the controller is unavailable.
- `ua`: installed/running state, retained records and recent connections, with `stats_scope=retained_log_lines`.
- `traffic`: explicit `clash_api` or `netdev` source, rates, cumulative totals and up to 90 samples. Source/reset changes clear the ring.

`POST /cgi-bin/routerdock?action`, same token, JSON body up to 2048 bytes:

```json
{"action":"delay","args":{"group":"My group","node":"My node"}}
```

| Action | Arguments | Behavior |
| --- | --- | --- |
| `switch` | `group`, `node` | Verify Selector membership, PUT selection; HTTP 204 means success |
| `delay` | `group`, `node` | Verify membership; fixed HTTPS generate_204 URL, 2-second controller timeout; cache result for 5 minutes |
| `restart_clash` | none | Submit installed OpenClash init restart |
| `restart_ua` | none | Submit installed UA3F init restart |
| `update` | none | Submit OpenClash's installed subscription updater |

Tests do not change the selected node. A successful latency request returns `{ok:true,delay:<ms>}`; failures include an error and are displayed as timeout/unreachable. Service tasks report submission, not completion. Unknown actions/nodes are rejected. No arbitrary shell command, URL or filesystem path is accepted from a device action body.
