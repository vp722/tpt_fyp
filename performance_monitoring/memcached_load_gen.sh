#!/usr/bin/env bash

# -----------------------------------------------------------------------------
# run_ept_workload.sh
#
# Split EPT‑heavy workload into two phases:
# 1) Load ~8 GiB dataset into Memcached
# 2) Warm up & run peak‑QPS workload at max throughput
#
# We use the binary protocol for both phases because it:
#  - Reduces client‑side parsing overhead compared to the ASCII protocol
#  - Packs keys and values more efficiently on the wire
#  - Achieves higher throughput and lower latency for large‑scale loads
# -----------------------------------------------------------------------------

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MUTILATE="$SCRIPT_DIR/../mutilate/mutilate"

#—— Defaults ————————————————————————————————————————————————————————————————
SERVER=${1:-localhost:11211}
QPS=0                # 0 = peak QPS (uncapped)
THREADS=${2:-16}
CONNS=${3:-64}
DURATION=${4:-120}      # measurement time (s)
WARMUP=10               # warmup time (s)

# Working‑set parameters to hit ~8 GiB
KEYSIZE=32              # bytes per key
VALUESIZE=4096          # bytes per value
RECORDS=2097152         # 2,097,152 × 4,096 B ≃ 8 GiB

# Phase 1: Load dataset
# --binary: use Memcached binary protocol for efficient bulk loading
echo "=== Phase 1: Preloading ~8 GiB into Memcached (${RECORDS} records) ==="
"$MUTILATE" \
  -s "$SERVER" \
  --loadonly \
  --binary \
  -K fixed:$KEYSIZE \
  -V fixed:$VALUESIZE \
  -r $RECORDS \
  --threads $THREADS \
  --connections $CONNS

# Phase 2: Warmup & run peak‑QPS workload
# --binary: maintain high throughput for GET requests as well
echo ""
echo "=== Phase 2: Warmup (${WARMUP}s) + Peak‑QPS run (${DURATION}s) ==="
"$MUTILATE" \
  -s "$SERVER" \
  --noload \
  --binary \
  --threads $THREADS \
  --connections $CONNS \
  -K fixed:$KEYSIZE \
  -V fixed:$VALUESIZE \
  -i uniform:$RECORDS \
  --qps $QPS \
  --warmup $WARMUP \
  --time $DURATION
