#!/usr/bin/env bash
set -euo pipefail

ovmf_path=${1:?OVMF firmware path is required}
disk_image=${2:?disk image path is required}
qemu_bin=${3:-qemu-system-x86_64}
log_file=$(mktemp "${TMPDIR:-/tmp}/vibe-frame-kit-user-fault.XXXXXX.log")

cleanup() {
    rm -f "$log_file"
}
trap cleanup EXIT

set +e
timeout 5s "$qemu_bin" \
    -display none \
    -debugcon stdio \
    -global isa-debugcon.iobase=0xe9 \
    -drive if=pflash,format=raw,readonly=on,file="$ovmf_path" \
    -drive file="$disk_image",format=raw,if=none,id=bootdisk \
    -device virtio-blk-pci,drive=bootdisk \
    -net none \
    -no-reboot >"$log_file" 2>&1
qemu_exit=$?
set -e

if [[ $qemu_exit -ne 124 ]]; then
    cat "$log_file" >&2
    echo "[FAIL] QEMU exited unexpectedly (exit=$qemu_exit)." >&2
    exit 1
fi

for marker in "[VMM] USER VSPACE ISOLATION VERIFIED" "[KERNEL] USER FAULT RECOVERED TO KERNEL"; do
    if ! grep -Fq "$marker" "$log_file"; then
        cat "$log_file" >&2
        echo "[FAIL] Missing user-fault marker: $marker" >&2
        exit 1
    fi
done

if grep -Eq 'DOUBLE FAULT|\[IDT\] EXCEPTION|USER VSPACE IS NOT PRIVATE' "$log_file"; then
    cat "$log_file" >&2
    echo "[FAIL] User fault escalated into an unexpected kernel exception." >&2
    exit 1
fi

echo "[PASS] User protection-fault recovery test passed."
