#!/bin/sh
set -eu
[ "$(id -u)" = 0 ] || { echo 'Run as root.' >&2; exit 1; }
if [ -L /www/cgi-bin/routerdock ] && [ "$(readlink /www/cgi-bin/routerdock)" = /usr/share/routerdock/monitor.lua ]; then rm /www/cgi-bin/routerdock; fi
rm -f /usr/share/routerdock/monitor.lua /usr/share/routerdock/collectors.lua
rmdir /usr/share/routerdock 2>/dev/null || true
printf 'Agent removed. /etc/routerdock/token and /etc/config/routerdock retained.\n'
