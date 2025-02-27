#!/bin/sh -l

cd /usr/local/app/ns-3
./waf --run=ospf-metric >> $GITHUB_OUTPUT
