#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$ROOT/build/go-cache"
cd "$ROOT/source/server"
GOCACHE="$ROOT/build/go-cache" exec go run . -listen 127.0.0.1:7777 "$@"
