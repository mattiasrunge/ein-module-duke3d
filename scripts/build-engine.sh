#!/bin/sh
# Builds EDuke32 to WebAssembly in a container (engine/Containerfile) and writes
# assets/eduke32.js, assets/eduke32.wasm and assets/eduke32-build.txt. Needs only docker with buildx,
# or podman; run from anywhere.
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
if command -v docker >/dev/null 2>&1; then
  docker buildx build --file "$root/engine/Containerfile" --output "type=local,dest=$root/assets" "$root/engine"
else
  podman build --file "$root/engine/Containerfile" --output "type=local,dest=$root/assets" "$root/engine"
fi
ls -l "$root/assets/eduke32.js" "$root/assets/eduke32.wasm"
