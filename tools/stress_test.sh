#!/bin/bash
# Some web server stress tests
#
# Perform a large number of parallel requests, stress testing the web server
# TODO: some kind of performance metrics

# Accepts three command line arguments:
# - first argument - mandatory - IP or hostname of target server
# - second argument - target type (optional)
# - third argument - xfer count (for replicated targets) (optional)
HOST=$1
declare -n TARGET_STR="${2:-JSON_LARGER}_TARGETS"
REPLICATE_COUNT=$(("${3:-10}"))

PARALLEL_MAX=${PARALLEL_MAX:-50}

CURL_ARGS="--compressed --parallel --parallel-immediate --parallel-max ${PARALLEL_MAX}"
CURL_PRINT_RESPONSE_ARGS="-w %{http_code}\n"

JSON_TARGETS=('json/state' 'json/info' 'json/si', 'json/palettes' 'json/fxdata' 'settings/s.js?p=2')
FILE_TARGETS=('' 'iro.js' 'rangetouch.js' 'settings' 'settings/wifi')
# Replicate one target many times
function replicate() {
  printf "${1}?%d " $(seq 1 ${REPLICATE_COUNT})
}
read -a JSON_TINY_TARGETS <<< $(replicate "json/nodes")
read -a JSON_SMALL_TARGETS <<< $(replicate "json/info")
read -a JSON_LARGE_TARGETS <<< $(replicate "json/si")
read -a JSON_LARGER_TARGETS <<< $(replicate "json/fxdata")
read -a INDEX_TARGETS <<< $(replicate "")

# Expand target URLS to full arguments for curl
TARGETS=(${TARGET_STR[@]})
#echo "${TARGETS[@]}"
FULL_TGT_OPTIONS=$(printf "http://${HOST}/%s -o /dev/null " "${TARGETS[@]}")
#echo ${FULL_TGT_OPTIONS}

time curl ${CURL_ARGS} ${FULL_TGT_OPTIONS}
