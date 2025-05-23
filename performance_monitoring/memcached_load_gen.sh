#!/usr/bin/env bash

# -----------------------------------------------------------------------------
# run_ept_workload.sh
#
# EPT-heavy workload using Facebook "ETC" distributions:
#   fb_key   = "gev:30.7984,8.20449,0.078688"
#   fb_value = hard-coded discrete & GPareto PDF of value sizes
#   fb_ia    = "pareto:0.0,16.0292,0.154971"
#
# Usage:
#   ./run_ept_workload.sh <server> [qps] [threads] [connections] [duration]
#
# Example:
#   ./run_ept_workload.sh localhost:11211 0 1 50 120
# -----------------------------------------------------------------------------

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MUTILATE="$SCRIPT_DIR/../../application_benchmarks/mutilate/mutilate"

#—— Parameters —————————————————————————————————————————————————————————————
SERVER=${1:?"Usage: $0 <server> [qps] [threads] [connections] [duration]"}
QPS=${2:-0}             # 0 = peak QPS (uncapped)
THREADS=${3:-1}         # load-only and GET threads
CONNS=${4:-32}         # connections per thread
DURATION=${5:-120}      # measurement time (s)
WARMUP=10               # warmup time (s)

# Facebook ETC distributions
DIST_KEY="32"
DIST_VAL="4096"
DIST_IA="uniform"

# Working-set size: ~8 GiB total (via ETC value PDF)
RECORDS=2097152         

# Phase 1: Load ~8 GiB dataset with ETC distributions
echo "=== Phase 1: Preloading ~8 GiB (${RECORDS} records) ==="
"$MUTILATE" \
  -s "$SERVER" \
  --loadonly \
  --binary \
  -K $DIST_KEY \
  -V $DIST_VAL \
  -r $RECORDS \
  --threads $THREADS \
  --connections $CONNS

# Phase 2: Warmup & run GETs using ETC inter-arrival times
echo -e "\n=== Phase 2: Warmup (${WARMUP}s) + ETC GETs at $QPS QPS for ${DURATION}s ==="
"$MUTILATE" \
  -s "$SERVER" \
  --noload \
  --binary \
  --threads $THREADS \
  --connections $CONNS \
  -K $DIST_KEY \
  -V $DIST_VAL \
  -i $DIST_IA \
  --qps $QPS \
  --warmup $WARMUP \
  --time $DURATION

# This script exclusively uses Facebook ETC distributions for key sizes, value sizes,
# and inter-arrival times to drive EPT page-walk overhead above 50% of cycles.
