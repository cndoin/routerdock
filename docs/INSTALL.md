# Installation

## Router requirements

Use an existing OpenWrt installation with `uhttpd` serving `/www/cgi-bin`, `ubus`, `uci`, `curl`, Lua 5.1 and these Lua modules: `luci.jsonc`, `luci.sys`, `nixio`. The installer stops if a dependency is absent; it never edits package feeds.

For opkg-based OpenWrt 23.05/24.10 or compatible iStoreOS:

```sh
opkg update
opkg install lua luci-lib-jsonc luci-lib-nixio luci-lib-base curl
```

For apk-based firmware, use its package manager and matching feeds instead:

```sh
apk update
apk add lua luci-lib-jsonc luci-lib-nixio luci-lib-base curl
```

Package names and availability vary across custom builds. Do not install packages from a different firmware release. OpenClash/Mihomo and UA3F are optional and are not installed by this project.

## Install from source

Copy the **contents of `openwrt/`** into a router directory, for example `/tmp/routerdock`, then:

```sh
sh /tmp/routerdock/install.sh
```

Keep the printed token private. On later installs the same token is retained. Files:

| Path | Purpose |
| --- | --- |
| `/usr/share/routerdock/monitor.lua` | Authenticated API, executed by uhttpd |
| `/usr/share/routerdock/collectors.lua` | System and UA3F collectors |
| `/www/cgi-bin/routerdock` | Symlink to the API |
| `/etc/routerdock/token` | Device token, mode 0600 |
| `/etc/config/routerdock` | Router-specific configuration, mode 0600 |
| `/tmp/routerdock-*.json` | Volatile rates, history and latency cache |

The CGI is reachable wherever your existing uhttpd is reachable, including any WAN exposure you already configured. The installer adds no firewall allowance. Keep it LAN-only. Tokens are independent of LuCI sessions, and can invoke documented service operations.

## Customize another router

Default logical interfaces are `wan` and `lan`. For a different layout:

```sh
uci set routerdock.main.wan_interface='uplink'
uci set routerdock.main.lan_interface='home'
uci commit routerdock
```

Use **logical OpenWrt interface names**, not bridge/physical-device names. The collector resolves their `l3_device`. With the default `wan`, a missing ubus interface can fall back to the kernel default-route device. Missing counters are reported unavailable instead of guessing another NIC.

OpenClash's running `/etc/openclash/config.yaml` and UCI controller settings are autodetected. For a different Mihomo controller:

```sh
uci set routerdock.main.controller_url='http://127.0.0.1:9090'
uci set routerdock.main.controller_secret='YOUR_CONTROLLER_SECRET'
uci commit routerdock
```

Replace the example secret locally; don't paste it in issues. A separately installed Mihomo controller can provide proxy metrics and selection, but restart/subscription actions require the OpenClash init script/updater and report unavailable otherwise. This is not a general service-management shell.

No agent daemon or router reboot is required. Settings are read on each request.

## First device setup

1. Flash the supported ESP32 board using the release bundle or PlatformIO.
2. At first boot, note the unique hotspot name and password on the display.
3. Join it from a phone/laptop. Open `http://192.168.4.1` manually; automatic captive-portal launch is not required or guaranteed.
4. Sign in as `admin`, password from the display.
5. Scan and select a **2.4 GHz** Wi-Fi network. For hidden networks enter the exact SSID. For an open network select **Open network**.
6. Enter the router LAN IPv4/hostname, HTTP port and printed device token. You may save Wi-Fi first and enter the token later.
7. Save. The ESP32 restarts and joins that network. It needs a route to the router's HTTP endpoint; guest isolation can prevent this.
8. Visit the device's new DHCP address to manage it. The System settings screen shows the address; mDNS `routerdock-xxxxxx.local` works only where supported.

For the same SSID, blank Wi-Fi password retains the existing password; for a new secured SSID, a password is required. Blank device token retains the old token. An optional new admin password must be 12–63 characters and also becomes the setup hotspot password after reboot. No secrets are returned by the status API.

## Recovery and maintenance

- Hold **BOOT for four seconds** or use **System → Device settings** to reopen setup. No reset button sequence or reflash is needed. Don't hold BOOT while powering on unless you intend to enter ROM download mode.
- A failed Wi-Fi connection opens setup after 45 seconds. It continues retrying the configured network. Once connected, a setup hotspot closes after ten minutes.
- Touch calibration is separate from configuration and remains in the existing `monitor` NVS namespace. System → Touch calibration starts the crosshair flow.
- Firmware updates via app-only flashing preserve NVS. Full-chip erase removes Wi-Fi/admin settings and touch calibration. Exporting configuration with passwords is intentionally not implemented.
- The serial port at 115200 accepts `status`, `portal`, `calibrate`, `page 0`…`page 4`, and `shot` (RGB565 debug frame). `configure <JSON>` is a physical-access provisioning path with the same validation; it never echoes submitted secrets. Treat physical serial access as privileged.

## Prebuilt flash bundle

Unzip the release's `routerdock-esp32-crowpanel-28.zip`. With Python and esptool installed:

```sh
python -m pip install esptool
python flash.py --port YOUR_SERIAL_PORT
```

The script writes bootloader, partition table, OTA selection data and the app at documented offsets. It checks file presence and invokes esptool once. It does not erase the NVS region. Choose a different serial port as needed.

## Uninstall the router agent

```sh
sh /tmp/routerdock/uninstall.sh
```

This removes only the known CGI symlink and two agent files. The token and UCI config are retained so reinstalling can keep the pairing. Delete `/etc/routerdock/token` and `/etc/config/routerdock` yourself only if you want to revoke pairing permanently; otherwise, generating a fresh token requires updating every paired device.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| Wi-Fi never connects | ESP32 requires 2.4 GHz; verify password and WPA mode. Enterprise/captive-portal sign-in is not implemented. Use BOOT recovery. |
| Router data unavailable | Router host/port, routing/guest isolation, agent installation and matching token. Check HTTP status in serial logs. |
| HTTP 401 from agent | Wrong/missing `Authorization: Bearer` token. Use `/etc/routerdock/token`, not the root password or Clash secret. |
| System works, nodes don't | Controller URL/secret and OpenClash/Mihomo API availability. |
| UA3F shows no statistics | UA3F 3.x logging format/path, logging enabled, and retained log records. |
| White screen or no touch | Exact pinout and driver in Hardware; similar-looking boards vary. |
| Windows linker rejects paths | Use an ASCII checkout path, e.g. `C:\dev\routerdock`, or a verified directory junction. |
| `pio upload` wants a filesystem utility | Use the release flash bundle or esptool directly; no filesystem image is needed. |
