# Compatibility and verification

## Hardware-tested

- One ESP32-D0WD-V3 / ESP32-32E board, 4 MB flash, no PSRAM, ILI9341 + XPT2046, pin mapping in Hardware.
- One x86 iStoreOS 24.10.5 router, OpenClash 0.48.002 with Mihomo, UA3F 3.7.0.
- Real Wi-Fi connection, system/traffic/Clash/UA3F polling, touch calibration and page navigation.
- Real authenticated device browser session, async Wi-Fi scan, language switch, no secret echo, validation and CSRF rejection.
- Settings save → restart → reconnect → changed refresh interval persists → original interval restored. Password/token retained when left blank.
- Read API token rejection; node pagination and idempotent selection; successful and failed latency tests; batch testing on the device.

Service restart/subscription actions are implemented with explicit on-device confirmation and report task submission. They have not been exercised as part of the public-release smoke test, to avoid interrupting the user's live network. A scan is not proof of joining every network security mode; hotspot joining from a separate phone is not part of the automated test.

## Fixture-tested

- ARM-style `/proc/cpuinfo`, CPU deltas, memory calculation/fallback, CPU-vs-ACPI sensor preference, missing temperature sensors, 64-bit-sized counters, renamed logical WAN and absent UA3F.
- Browser configuration form: languages, scan/selection, open-network state, blank-secret retention semantics, CSRF submission, save feedback, responsive width and no JavaScript errors. Documentation web screenshots use these fixtures.

## Build-tested

- PlatformIO Espressif32 8.0.0 / Arduino ESP32 3.3.12, LovyanGFX 1.2.7, ArduinoJson 7.4.2.
- Generic firmware contains no compiled personal Wi-Fi/token header. Local configured NVS is not included in release packages.

## Not yet hardware-verified

Other ARM/MIPS OpenWrt systems, apk-based releases, vendor/custom OpenWrt builds, arbitrary Mihomo launch layouts, UA3F versions with different log formats, ESP32-S3/capacitive-touch displays, WPA Enterprise, captive-login Wi-Fi and IPv6 router URLs. The agent is architecture-neutral source, but this is not a hardware-compatibility guarantee.

Report missing package/module dependencies rather than copying packages from another firmware. LuCI-free images need the listed Lua libraries and an HTTP CGI server. Temperature and some NIC metadata depend on kernel driver support. Use lower refresh rates on resource-constrained routers.
