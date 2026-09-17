#!/usr/bin/env bash
set -euo pipefail

ovmf_path=${1:?OVMF firmware path is required}
disk_image=${2:?disk image path is required}
qemu_bin=${3:-qemu-system-x86_64}
log_file=$(mktemp "${TMPDIR:-/tmp}/vibe-frame-kit-boot.XXXXXX.log")

cleanup() {
    rm -f "$log_file"
}
trap cleanup EXIT

if [[ ! -f "$ovmf_path" ]]; then
    echo "[FAIL] OVMF firmware not found: $ovmf_path" >&2
    exit 1
fi
if [[ ! -f "$disk_image" ]]; then
    echo "[FAIL] disk image not found: $disk_image" >&2
    exit 1
fi

set +e
timeout 10s "$qemu_bin" \
    -display none \
    -debugcon "file:$log_file" \
    -global isa-debugcon.iobase=0xe9 \
    -drive if=pflash,format=raw,readonly=on,file="$ovmf_path" \
    -drive file="$disk_image",format=raw,if=none,id=bootdisk \
    -device virtio-blk-pci,drive=bootdisk \
    -net none \
    -no-reboot 2>>"$log_file"
qemu_exit=$?
set -e

# USERAPP은 의도적으로 무한 루프를 실행한다. 따라서 타임아웃(124)은
# 사용자 코드가 실행 중이라는 정상 기준선이며 즉시 종료는 실패다.
if [[ $qemu_exit -ne 124 ]]; then
    cat "$log_file" >&2
    echo "[FAIL] QEMU exited unexpectedly (exit=$qemu_exit)." >&2
    exit 1
fi

required_markers=(
    "XNU OS KERNEL INIT..."
    "[VIRTIO-BLK] INITIALIZED SUCCESSFULLY!"
    "[FAT32] FS INITIALIZED!"
    "[MACHO] ENTRY POINT LOADED: 0x100000320"
    "[VMM] USER VSPACE ISOLATION VERIFIED"
    "[KERNEL] JUMPING TO USERLAND AT: 0x100000320"
)
for marker in "${required_markers[@]}"; do
    if ! grep -Fq "$marker" "$log_file"; then
        cat "$log_file" >&2
        echo "[FAIL] Missing boot marker: $marker" >&2
        exit 1
    fi
done

if grep -Eq '\[MACHO.*ERROR|\[FAT32.*ERROR|\[IDT\] EXCEPTION|DOUBLE FAULT' "$log_file"; then
    cat "$log_file" >&2
    echo "[FAIL] Kernel reported an exception or loader/storage failure." >&2
    exit 1
fi

echo "[PASS] Phase 0 QEMU boot baseline passed."
