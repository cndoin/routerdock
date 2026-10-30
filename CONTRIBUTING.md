# Contributing

Thanks for helping make RouterDock portable. Start with [Architecture](docs/ARCHITECTURE.md), [Hardware](docs/HARDWARE.md) and [Compatibility](docs/COMPATIBILITY.md).

Keep changes testable and scoped. Report your exact ESP32 board, LCD/touch controller, pin mapping, OpenWrt release, CPU architecture, package manager and optional plugin versions. Never attach passwords, tokens, controller secrets, subscription URLs, personal log files or NVS dumps.

Development checks:

```sh
python scripts/embed_portal.py
python scripts/check_public.py
luac -p openwrt/monitor.lua openwrt/collectors.lua
sh -n openwrt/install.sh
sh -n openwrt/uninstall.sh
lua tests/collectors_test.lua
npm ci
npx playwright install chromium
npm test
pio run -d firmware
```

A changed web page must regenerate `firmware/include/PortalPage.h`. Browser fixtures verify UI behavior; they do not prove physical Wi-Fi recovery. A new architecture, firmware or display is only marked hardware-verified after a corresponding physical test. Include that distinction in pull requests.

Please retain graceful behavior with OpenClash/UA3F absent. Do not infer traffic totals from unrelated accounting layers. Keep router root credentials out of ESP32 builds and require authentication/CSRF checks for new settings writes.
