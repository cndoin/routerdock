#!/bin/sh
# Install from the repository's openwrt/ directory. Requires root.
set -eu
[ "$(id -u)" = 0 ] || { echo 'Run as root.' >&2; exit 1; }
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
for cmd in lua curl ubus uci awk; do
 command -v "$cmd" >/dev/null || { echo "Missing $cmd. See docs/INSTALL.md." >&2; exit 1; }
done
lua -e 'require("luci.jsonc"); require("luci.sys"); require("nixio")' || {
 echo 'Missing Lua libraries. Install lua luci-lib-jsonc luci-lib-nixio luci-lib-base, then retry.' >&2; exit 1;
}
[ -d /www/cgi-bin ] || { echo 'An existing uhttpd /www/cgi-bin directory is required.' >&2; exit 1; }
umask 077
mkdir -p /etc/routerdock /usr/share/routerdock
for f in monitor.lua collectors.lua; do
 [ -f "$HERE/$f" ] || { echo "Missing $HERE/$f" >&2; exit 1; }
 # Remove CRLF and optional UTF-8 BOM when installed from a Windows checkout.
 sed '1s/^\xef\xbb\xbf//;s/\r$//' "$HERE/$f" > "/usr/share/routerdock/$f.new"
 lua -e "assert(loadfile('/usr/share/routerdock/$f.new'))"
 chmod 644 "/usr/share/routerdock/$f.new"
 mv "/usr/share/routerdock/$f.new" "/usr/share/routerdock/$f"
done
chmod 755 /usr/share/routerdock/monitor.lua
if [ ! -s /etc/routerdock/token ]; then
 lua -e 'local f=assert(io.open("/dev/urandom","rb")); local b=assert(f:read(24)); f:close(); assert(#b==24); io.write((b:gsub(".",function(c) return string.format("%02x",c:byte()) end)))' > /etc/routerdock/token.new
 [ "$(wc -c < /etc/routerdock/token.new)" -eq 48 ] || { echo 'Token generation failed.' >&2; exit 1; }
 mv /etc/routerdock/token.new /etc/routerdock/token
fi
chmod 600 /etc/routerdock/token
if [ ! -f /etc/config/routerdock ]; then
 cat > /etc/config/routerdock <<'CONFIG'
config monitor 'main'
        option wan_interface 'wan'
        option lan_interface 'lan'
        # Optional: explicit HTTP controller URL and secret, otherwise OpenClash autodetection.
        # option controller_url 'http://127.0.0.1:9090'
        # option controller_secret 'YOUR_CONTROLLER_SECRET'
CONFIG
 chmod 600 /etc/config/routerdock
fi
ln -sf /usr/share/routerdock/monitor.lua /www/cgi-bin/routerdock
printf '\nRouterDock installed. No service was restarted.\nEndpoint: http://<router-LAN-IP>/cgi-bin/routerdock\nDevice token (paste into the device settings):\n'
cat /etc/routerdock/token
printf '\nKeep this token private. No firewall rules or WAN listeners were added.\n'
