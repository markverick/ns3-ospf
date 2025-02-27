#!/bin/sh -l

cd /usr/local/app/ns-3

if [ -z "$GITHUB_OUTPUT" ]; then
  echo "Warning: GITHUB_OUTPUT is not set. Output will not be saved."
else
  ./waf --run=ospf-metric >> "$GITHUB_OUTPUT"
fi
