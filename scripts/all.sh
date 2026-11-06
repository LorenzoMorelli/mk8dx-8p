#!/bin/bash
# Build + deploy in one command. Usage: scripts/all.sh [run]
set -e
HERE="$(cd "$(dirname "$0")/.." && pwd)"
"$HERE/scripts/build.sh"
"$HERE/scripts/deploy.sh"
[ "$1" = "run" ] && "$HERE/scripts/run-eden.sh"
exit 0
