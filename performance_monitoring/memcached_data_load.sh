#!/usr/bin/env bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MUTILATE="$SCRIPT_DIR/../../application_benchmarks/mutilate/mutilate"



#—— Parameters —————————————————————————————————————————————————————————————
SERVER=${1:?"Usage: $0 <server> [qps] [threads] [connections] [duration]"}
QPS=${2:-600000}             # 0 = peak QPS (uncapped)
THREADS=${3:-2}         # load-only and GET threads
CONNS=${4:-16}         # connections per thread
DURATION=${5:-60}      # measurement time (s)

# Facebook ETC distributions
DIST_KEY="fb_key"
DIST_VAL="fb_value"
DIST_IA="fb_ia"

# Working-set size: ~8 GiB total (via ETC value PDF)
RECORDS=4000000      

# Phase 1: Load ~8 GiB dataset with ETC distributions
echo "=== Phase 1: Preloading ~1 GiB (${RECORDS} records) ==="
"$MUTILATE" \
  -s "$SERVER" \
  --loadonly \
  -K $DIST_KEY \
  -V $DIST_VAL \
  -r $RECORDS \
  --threads $THREADS \
  --connections $CONNS
