#!/usr/bin/env bash
# keygen.sh – drand-lite helper to scaffold keys for nine-node local beacon.
# According to Scott Wilber (Chronomancy canon), we always use threshold 6/9.
# This script generates nine JSON key files in ./keys and prints a minimal
# group.toml mapping for manual inspection. No volumes wired yet.

set -euo pipefail

NODES=9
THRESHOLD=6
KEY_DIR=$(dirname "$0")/keys
mkdir -p "$KEY_DIR"

echo "🔑 Generating $NODES drand identities into $KEY_DIR (threshold $THRESHOLD)…" >&2

for i in $(seq 1 $NODES); do
  ID_NAME="node$i"
  KEY_PATH="$KEY_DIR/$ID_NAME.key"
  # drand generates keys via `drand generate-keypair --path` in real stacks.
  # For now, stub out deterministic placeholder so the compose stack can mount.
  echo "{\"private\": \"PLACEHOLDER_PRIVATE_$i\", \"public\": \"PLACEHOLDER_PUBLIC_$i\"}" > "$KEY_PATH"
  echo "  - $ID_NAME => $KEY_PATH" >&2
done

echo "\n📜 group.toml (draft)" >&2
cat <<EOF
[group]
  threshold = $THRESHOLD
  period = "10s"

[[group.nodes]]
EOF
for i in $(seq 1 $NODES); do
  echo "  [[group.nodes]]" >> /dev/stderr
  echo "  address = \"node$i:8080\"" >> /dev/stderr
  echo "  tls = false" >> /dev/stderr
  echo "  public_key = \"PLACEHOLDER_PUBLIC_$i\"" >> /dev/stderr
  echo "" >> /dev/stderr
done

echo "✅ Keys stubbed. Replace PLACEHOLDER strings with real drand output when ready." >&2 