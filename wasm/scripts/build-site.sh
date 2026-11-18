#!/usr/bin/env bash
# Assemble the web demo site into <out-dir> (default: _site) from the built package in wasm/dist.
# Used by .github/workflows/pages.yml; run it locally the same way to preview the site:
#   bash wasm/scripts/build-site.sh _site && python3 -m http.server -d _site 8000
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
out="${1:-_site}"
mkdir -p "$out"
out="$(cd "$out" && pwd)"

for f in index.mjs odb2kicad_wasm.mjs odb2kicad_wasm.wasm; do
  [ -f "$root/wasm/dist/$f" ] || { echo "missing wasm/dist/$f: build the WASM and run 'node wasm/scripts/build.mjs' first" >&2; exit 1; }
done

cp "$root/wasm/demo/index.html" "$out/"
# the page imports ./index.mjs, which loads ./odb2kicad_wasm.mjs and its .wasm
cp "$root/wasm/dist/index.mjs" "$root/wasm/dist/odb2kicad_wasm.mjs" "$root/wasm/dist/odb2kicad_wasm.wasm" "$out/"
cp "$root/samples/test-odb.tgz" "$out/"                 # the page's "Try it now" sample
# the npm package of this build: npm install -g <site>/odb2kicad-wasm.tgz
(cd "$root/wasm" && npm pack --silent --pack-destination "$out" >/dev/null)
mv "$out"/odb2kicad-wasm-*.tgz "$out/odb2kicad-wasm.tgz"
ls -l "$out"
