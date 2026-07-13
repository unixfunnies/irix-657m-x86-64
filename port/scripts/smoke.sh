#!/bin/sh
# Regression smoke test: boot each milestone's checkpoint and verify
# its expected serial output appears.  Grows one entry per milestone.
cd "$(dirname "$0")/.."

OUT=$(mktemp)
trap 'rm -f "$OUT"' EXIT

timeout 30 qemu-system-x86_64 \
	-M q35 -m 512M \
	-cdrom obj/irix-x86_64.iso \
	-serial file:"$OUT" \
	-display none \
	-no-reboot -no-shutdown >/dev/null 2>&1 &
QPID=$!

# Poll for the last checkpoint marker rather than sleeping blind.
i=0
while [ $i -lt 25 ]; do
	if grep -q "M0 checkpoint reached" "$OUT" 2>/dev/null; then
		break
	fi
	sleep 1
	i=$((i + 1))
done
kill $QPID 2>/dev/null
wait $QPID 2>/dev/null

fail=0
check() {
	if grep -q "$1" "$OUT"; then
		echo "PASS: $1"
	else
		echo "FAIL: $1"
		fail=1
	fi
}

# --- M0 ---
check "IRIX Release 6.5.7m x86-64"
check "Booted by Limine"
check "M0 checkpoint reached"

echo "---- serial transcript ----"
cat "$OUT"
echo "---------------------------"
exit $fail
