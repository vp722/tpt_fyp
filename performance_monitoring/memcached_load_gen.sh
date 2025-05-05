#!/bin/bash 

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MUTILATE="$SCRIPT_DIR/../mutilate/mutilate"

SERVER="localhost"
KEYSIZE="16"
VALUESIZE="256"
THREADS=4
CONNECTIONS=16
DISTRIBUTION="fb_ia"


echo "Step 1: Preloading $RECORDS records into Memcached..."
"$MUTILATE" -s "$SERVER" \
  --loadonly \
  --keysize="$KEYSIZE" \
  --valuesize="$VALUESIZE"


echo ""
echo "Step 2: Running random access test..."
"$MUTILATE" -s "$SERVER" \
  --noload \
  --threads="$THREADS" \
  --connections="$CONNECTIONS" \
  --keysize="$KEYSIZE" \
  --valuesize="$VALUESIZE" \
  --distribution="$DISTRIBUTION" \
  --qps=0 
