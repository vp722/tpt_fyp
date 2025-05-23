#!/usr/bin/env bash

# -----------------------------------------------------------------------------
# run_ept_workload.sh
#
#
# Usage:
#   ./run_ept_workload.sh [server] [threads] [connections] [duration]
# -----------------------------------------------------------------------------

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MUTILATE="$SCRIPT_DIR/../../application_benchmarks/mutilate/mutilate"

#—— Defaults ————————————————————————————————————————————————————————————————
SERVER=${1:-localhost:11211}
QPS=0                # 0 = peak QPS (uncapped)
THREADS=${2:-16}
CONNS=${3:-64}
DURATION=${4:-120}      # measurement time (s)
WARMUP=10               # warmup time (s)

# Working‐set parameters to hit ~16 GiB with 4 KiB values
KEYSIZE=32              # bytes per key
VALUESIZE=4096          # bytes per value
RECORDS=4194304         # 4 194 304 × 4 096 B ≃ 16 GiB

echo "=== One‐Step EPT‐Heavy Peak QPS Workload Runner ==="
echo "Server       : $SERVER"
echo "QPS          : $QPS (peak)"
echo "Threads      : $THREADS"
echo "Connections  : $CONNS"
echo "Warmup       : $WARMUP s"
echo "Duration     : $DURATION s"
echo "Key size     : $KEYSIZE B"
echo "Value size   : $VALUESIZE B"
echo "Records      : $RECORDS  (~16 GiB)"
echo "-----------------------------------"

# Single invocation: load, warmup, run
"$MUTILATE" \
  -s "$SERVER" \
  -K fixed:$KEYSIZE \
  -V fixed:$VALUESIZE \
  -r $RECORDS \
  --threads $THREADS \
  --connections $CONNS \
  -i uniform:$RECORDS \
  --qps $QPS \
  --warmup $WARMUP \
  --time $DURATION
