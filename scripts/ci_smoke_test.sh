# Smoke test for a freshly built busybox, run natively on its own OS by CI:
#   <busybox> sh scripts/ci_smoke_test.sh
# Every command must resolve to a busybox applet, so on Linux and macOS CI runs this with an empty PATH.
# Set BB_TEST_NETWORK=1 to also download over HTTP and HTTPS.

pass=0
fail=0

check() {
	out=$(eval "$2" 2>&1 | tr '\n' ' ')
	case "$out" in
		*"$3"*) pass=$((pass + 1)); echo "PASS $1" ;;
		*) fail=$((fail + 1)); echo "FAIL $1"; echo "     expected to contain: [$3]"; echo "     got: [$out]" ;;
	esac
}

dir=$(mktemp -d)
cd "$dir" || exit 1
echo hi >f.txt

check echo "echo ok" ok
# A child shell that crashes on exit still delivers its output, so check exit statuses and EXIT traps explicitly
check subst-status "x=\$(echo hi); echo status=\$?" status=0
check subshell-exit-trap "( trap 'echo trapped' EXIT; exit 4 )" trapped
check cat "cat f.txt" hi
check grep "grep hi f.txt" hi
check awk "awk '{ print toupper(\$0) }' f.txt" HI
check sed "sed s/h/H/ f.txt" Hi
check subshell "( cd / && echo sub )" sub
check pipeline "printf 'b\\na\\n' | sort | head -n 1" a
check touch-date "touch -d @86400 f.txt && date -r f.txt +%s" 86400
check xargs "echo a b | xargs echo X" "X a b"
check env "env FOO=1 sh -c 'echo v\$FOO'" v1
check find-exec "find f.txt -exec wc -c {} +" "f.txt"
check timeout "timeout 5 sleep 0 && echo ok" ok
check nested-sh "sh -c 'sh -c \"echo deep\"'" deep
check subdir-reexec "mkdir s && cd s && echo x | tr x y" y
check wget-applet "wget --help" "Usage: wget"

# Needs internet access, which not every machine running this has
if [ "${BB_TEST_NETWORK:-0}" = 1 ]; then
	check wget-http "timeout 30 wget -q -O - http://example.com" "Example Domain"
	check wget-https "timeout 30 wget -q -O - https://example.com" "Example Domain"
	check wget-https-quiet "timeout 30 wget -q -O /dev/null https://example.com && echo quiet" quiet
fi

cd / && rm -rf "$dir"
echo "passed=$pass failed=$fail"
[ "$fail" -eq 0 ]
