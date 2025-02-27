#!/bin/sh -l

cd /usr/local/app/ns-3

./waf configure --enable-examples --enable-test
./waf build

if [ -z "$GITHUB_OUTPUT" ]; then
  echo "Warning: GITHUB_OUTPUT is not set. Output will not be saved."
  ./waf --run=ospf-metric
else
  ./waf --run=ospf-metric >> "$GITHUB_OUTPUT"
fi
