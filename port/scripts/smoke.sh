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
	if grep -q "main() returned" "$OUT" 2>/dev/null; then
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

# --- M1 ---
check "IDT ok"
check "pmm: .* pages"
check "pmap: kernel page tables active"
check "timer: .* ticks at 100 Hz"
check "M1 checkpoint reached"

# --- M2 ---
check "kmem: .* KB arena"
check "kmem_zalloc zeroed: b\[0\]=0 b\[4095\]=0"
check "reused, coalesce ok"
check "kmem: all freed, used 0 KB"
check "M2 checkpoint reached"

# --- M3: IRIX main() runs ---
check "Entering irix/kern/os/main.c:main()"
check "IRIX Release 6.5.7m IP99 Version .* System V - 64 Bit"
check "Total real memory  = [1-9]"
check "1 CPU(s)"

# --- M6: root filesystem mounts, main() completes ---
check "memfs: synthetic root mounted"
check "Root on device memfs (fstype memfs)"
check "main() returned"

echo "---- serial transcript ----"
cat "$OUT"
echo "---------------------------"
exit $fail
