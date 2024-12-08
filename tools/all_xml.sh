#!/bin/bash
# Pull all settings pages for comparison
HOST=$1
TGT_PATH=$2
CURL_ARGS="--compressed"

# Replicate one target many times
function replicate() {
  for i in {0..10}
  do  
    echo -n " http://${HOST}/settings.js?p=$i -o ${TGT_PATH}/$i.xml"
  done
}
read -a TARGETS <<< $(replicate)

mkdir -p ${TGT_PATH}
curl ${CURL_ARGS} ${TARGETS[@]}
