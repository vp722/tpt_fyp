#!/bin/bash
PERF_PATH=$(which perf)
echo "Using perf at: $PERF_PATH"
perf -e cycles sleep 1
