#!/bin/sh -l

cd ns-3
./waf --run=ospf-metric >> $GITHUB_OUTPUT
