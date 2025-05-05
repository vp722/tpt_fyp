#!/bin/bash 

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MUTILATE="$SCRIPT_DIR/../mutilate/mutilate"

SERVER="localhost"
KEYSIZE="16"
VALUESIZE="256"
THREADS=8                 
CONNECTIONS=32            
DISTRIBUTION="fb_ia"
QPS=10000                 
RECORDS=1000000           # Preload more records into Memcached

echo "Step 1: Preloading $RECORDS records into Memcached..."
"$MUTILATE" -s "$SERVER" \
  --loadonly \
  --keysize="$KEYSIZE" \
  --valuesize="$VALUESIZE" \
  --records="$RECORDS"

echo ""
echo "Step 2: Running random access test with higher operations..."
"$MUTILATE" -s "$SERVER" \
  --noload \
  --threads="$THREADS" \
  --connections="$CONNECTIONS" \
  --keysize="$KEYSIZE" \
  --valuesize="$VALUESIZE" \
  --iadist="$DISTRIBUTION" \
  --qps="$QPS"   # Set QPS to increase operations
