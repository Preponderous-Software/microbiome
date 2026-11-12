#!/bin/sh
# Points apt at a fallback Ubuntu mirror when the configured mirror hosts do
# not resolve. Run it before apt-get update, as root or with sudo available.
#
# GitHub's runners have lost DNS for archive.ubuntu.com, security.ubuntu.com
# and azure.archive.ubuntu.com before (2026-11-11) while other hosts still
# resolved, which failed every job at apt-get before anything compiled. When
# every host resolves, this script changes nothing.
#
#   APT_FALLBACK_MIRROR  the mirror to use (default http://mirrors.edge.kernel.org/ubuntu/)
set -eu

FALLBACK="${APT_FALLBACK_MIRROR:-http://mirrors.edge.kernel.org/ubuntu/}"
SUDO=""
[ "$(id -u)" -eq 0 ] || SUDO=sudo

hosts="archive.ubuntu.com security.ubuntu.com"
if [ -f /etc/apt/apt-mirrors.txt ]; then
    hosts="$hosts $(sed -n 's#^[a-z]*://\([^/[:space:]]*\).*#\1#p' /etc/apt/apt-mirrors.txt)"
fi

broken=""
for h in $hosts; do
    ok=""
    for _ in 1 2 3; do
        if getent hosts "$h" > /dev/null 2>&1; then ok=1; break; fi
        sleep 5
    done
    [ -n "$ok" ] || broken="$broken $h"
done

if [ -z "$broken" ]; then
    echo "apt-fallback: all mirror hosts resolve; sources unchanged"
    exit 0
fi

echo "::warning::apt-fallback: cannot resolve$broken; using $FALLBACK"
for f in /etc/apt/sources.list /etc/apt/sources.list.d/*.sources /etc/apt/sources.list.d/*.list /etc/apt/apt-mirrors.txt; do
    [ -f "$f" ] || continue
    for h in $broken; do
        $SUDO sed -i "s#https\{0,1\}://$h/ubuntu/\{0,1\}#$FALLBACK#g" "$f"
    done
done
