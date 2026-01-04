#!/bin/sh -l

cd /usr/local/app/ns-3

./waf configure --enable-examples --enable-tests
./waf build

./test.py --nowaf --list > test-suites.txt

ospf_suites=$(awk 'NR>2 {print $2}' test-suites.txt | grep -E '^ospf-' | sort -u)
if [ -z "$ospf_suites" ]; then
  echo "No ospf-* test suites found. Check that the ospf module is mounted correctly." >&2
  exit 1
fi

suite_args=""
for s in $ospf_suites; do
  suite_args="$suite_args -s $s"
done

echo "Running OSPF test suites:" 
echo "$ospf_suites" | sed 's/^/ - /'

# shellcheck disable=SC2086
./test.py --nowaf $suite_args -v
