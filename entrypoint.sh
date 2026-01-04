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

echo "Running OSPF test suites:" 
echo "$ospf_suites" | sed 's/^/ - /'

# IMPORTANT: ns-3's test.py only runs the last provided -s when multiple -s
# options are given in a single invocation. Run each suite separately.
for s in $ospf_suites; do
  echo ""
  echo "=== Running $s ==="
  ./test.py --nowaf -s "$s" -v
done
