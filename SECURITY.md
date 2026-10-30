# Security

Use RouterDock only on a trusted LAN. Device settings and router requests use HTTP without transport encryption. Setup uses a unique per-device Wi-Fi password; device administration uses HTTP Basic authentication and CSRF-protected writes. Credentials are stored in ESP32 NVS and a mode-0600 router token file; the project does not encrypt those stores.

The device token authorizes both reads and the documented service/node actions. Anyone with that token can trigger those actions from a reachable network. Physical display/serial access is privileged: the setup screen can reveal the admin password, and serial provisioning can replace settings. Do not publish NVS dumps, configured device binaries containing manually added credentials, subscription URLs, tokens or real private network logs.

The installer does not change uhttpd's binding or firewall. If your router exposes its web server on WAN, the agent shares that exposure. Network isolation remains your responsibility.

For a vulnerability, use GitHub's private vulnerability reporting for this repository if available. Otherwise open a minimal issue requesting a private reporting channel without exploit details or credentials. Do not paste sensitive evidence into public issues.
