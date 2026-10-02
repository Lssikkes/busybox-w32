# Shell stress test for a freshly built busybox, run natively on its own OS by CI:
#   <busybox> sh scripts/ci_shell_stress.sh
# Mirrors what large real-world scripts do: source a big function library, export many variables, then run many command
# substitutions, subshells and pipelines (each one a forkshell on Windows) while shell errors unwind through longjmp.

ROUNDS=${ROUNDS:-60}
FUNCS=${FUNCS:-300}

fail=0
report() { echo "FAIL $1: expected [$2] got [$3]"; fail=$((fail + 1)); }
expect() { [ "$2" = "$3" ] || report "$1" "$2" "$3"; }

dir=$(mktemp -d)
lib="$dir/lib.sh"

# A function library with every common parse-tree node type: if/elif, case, for, while, until, pipelines, &&/||, subshells,
# brace groups, redirections, heredocs, command substitution, arithmetic and parameter expansion.
i=0
while [ "$i" -lt "$FUNCS" ]; do
	cat >>"$lib" <<EOF
f_$i() {
	local a="\${1:-x}" b n=0
	case "\$a" in
		x|y) b="xy_$i" ;;
		[0-9]*) b="num_\$((a + $i))" ;;
		*) b="\${a#?}_\${a%?}" ;;
	esac
	if [ "\$n" -gt 1 ]; then b=never; elif [ -z "\$b" ]; then b=empty; else :; fi
	for w in one two three; do n=\$((n + \${#w})); done
	while [ "\$n" -gt 20 ]; do n=\$((n - 7)); done
	until [ "\$n" -le 5 ]; do n=\$((n - 3)); done
	{ echo "\$b" | tr a-z A-Z | sed 's/_/-/g'; } 2>/dev/null
	( : ) && true || false
	cat <<'INNER' >/dev/null
literal \$heredoc $i
INNER
	echo "\$(echo "\$n")" >/dev/null
}
export V_$i="value-$i-x\$(printf '%0100d' 0)"
EOF
	i=$((i + 1))
done

. "$lib"

r=0
while [ "$r" -lt "$ROUNDS" ]; do
	k=$((r * 7 % FUNCS))
	expect "subst f_$k" "XY-$k" "$(f_$k)"
	expect "nested subst" "deep$r" "$(echo "$(echo "$(echo deep$r)")")"
	expect "subshell" "sub$r" "$( (echo sub$r) )"
	expect "pipeline" "3" "$(printf 'a\nb\nc\n' | wc -l | tr -d ' ')"
	expect "exported var" "value-$k" "$(sh -c "echo \"\${V_$k%%-x*}\"")"
	expect "eval syntax error" "caught" "$( (eval 'if then fi') 2>/dev/null || echo caught)"
	expect "unset parameter error" "caught" "$( (: "${UNSET_VAR_$r?boom}") 2>/dev/null || echo caught)"
	expect "set -e" "caught" "$( (set -e; false; echo not); [ $? -ne 0 ] && echo caught)"
	expect "break out of nested loops" "1" "$(for a in 1 2; do for b in 1 2; do echo $a; break 2; done; done)"
	expect "return from nested function" "r" "$(g() { h() { return 3; }; h; [ $? -eq 3 ] && echo r; }; g)"
	expect "command not found" "caught" "$(no_such_command_$r 2>/dev/null || echo caught)"
	expect "trap in subshell" "trapped" "$( (trap 'echo trapped' EXIT; exit 4) )"
	expect "background job" "bg$r" "$( (echo bg$r) & wait )"
	r=$((r + 1))
done

rm -rf "$dir"
echo "rounds=$ROUNDS functions=$FUNCS failed=$fail"
[ "$fail" -eq 0 ]
