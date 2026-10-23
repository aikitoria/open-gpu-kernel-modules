#!/bin/bash
set -e
DRIVER_VERSION=615.71.10
KDIR=/lib/modules/$(uname -r)
P2P_COPY=/root/nvidia-kmod-p2p-$DRIVER_VERSION
STOCK_BACKUP=/root/nvidia-kmod-backup-official-$DRIVER_VERSION
MODS="nvidia nvidia-uvm nvidia-modeset nvidia-drm nvidia-peermem"

# Best effort: unload a running driver. Do not abort if modules are held
# (e.g. by a live workload) — files can still be replaced and take effect
# at the next boot.
sudo rmmod nvidia_drm nvidia_modeset nvidia_uvm nvidia 2>/dev/null || \
    echo "note: could not unload running nvidia modules (in use?); new build takes effect at next boot"

make modules -j$(nproc)

# Kernels with CONFIG_MODULE_SIG_ALL (e.g. AlmaLinux) reject unsigned
# out-of-tree modules. Sign with the distro DKMS signing key, exactly
# like the stock DKMS build is signed. Skipped when modules are already
# signed (sign-file is not idempotent).
SIGN_FILE=/usr/src/kernels/$(uname -r)/scripts/sign-file
KEY=/var/lib/dkms/mok.key
CERT=/var/lib/dkms/mok.pub
if [ -x "$SIGN_FILE" ] && [ -f "$KEY" ] && [ -f "$CERT" ]; then
    for m in $MODS; do
        if [ -z "$(modinfo -F sig_key kernel-open/$m.ko 2>/dev/null)" ]; then
            "$SIGN_FILE" sha512 "$KEY" "$CERT" kernel-open/$m.ko
            echo "signed: $m"
        fi
    done
fi

# The kernel's own modules_install also signs (CONFIG_MODULE_SIG_ALL) using
# key material it expects at <kerneldir>/certs/signing_key.{pem,x509}.
# Distro kernel-devel trees don't ship these; provide the DKMS signing key
# so the built-in SIGN step succeeds instead of spewing SSL errors.
KSRC=/usr/src/kernels/$(uname -r)
if [ -d "$KSRC" ] && [ -f "$KEY" ] && [ -f "$CERT" ] && [ ! -f "$KSRC/certs/signing_key.pem" ]; then
    sudo mkdir -p "$KSRC/certs"
    sudo cp "$KEY"  "$KSRC/certs/signing_key.pem"
    sudo cp "$CERT" "$KSRC/certs/signing_key.x509"
    sudo chmod 600 "$KSRC/certs/signing_key.pem"
    echo "provisioned kernel-tree signing key: $KSRC/certs/"
fi

sudo make modules_install -j$(nproc)

# Install into extra/ as CRC32 xz.
#
# CRITICAL: the check type MUST be CRC32 (stream header byte 7 = 0x01),
# not the xz default CRC64. kmod hands .ko.xz to the kernel for in-kernel
# decompression (MODULE_INIT_COMPRESSED_FILE); the in-kernel xz decoder
# rejects CRC64 streams: dmesg "decompression failed with status 6" ->
# finit_module -EINVAL -> "modprobe: could not insert: Invalid argument"
# with no other kernel message. Stock DKMS modules are CRC32.
#
# Note: some distro xz builds (e.g. AlmaLinux xz 5.6.2) are compiled
# without the --crc32/--crc64 options and can only emit CRC64, so prefer
# python's lzma when available.
mkdir -p "$KDIR/extra"
for m in $MODS; do
    if python3 -c 'import lzma' 2>/dev/null; then
        KDIR="$KDIR" python3 - "$m" <<'PYEOF'
import lzma, os, sys
m = sys.argv[1]
raw = open(f'kernel-open/{m}.ko', 'rb').read()
data = lzma.compress(raw, format=lzma.FORMAT_XZ, preset=6, check=lzma.CHECK_CRC32)
assert data[7] == 1, f'{m}: check type is not CRC32'
assert lzma.decompress(data) == raw, f'{m}: roundtrip mismatch'
with open(f'{os.environ["KDIR"]}/extra/{m}.ko.xz', 'wb') as f:
    f.write(data)
print(f'compressed (CRC32): {m}')
PYEOF
    elif xz --help 2>/dev/null | grep -q -- '--crc32'; then
        xz -c --crc32 "kernel-open/$m.ko" > "$KDIR/extra/$m.ko.xz"
        echo "compressed (CRC32): $m"
    else
        echo "ERROR: need python3 with lzma or an xz built with --crc32." >&2
        echo "       CRC64 xz is rejected by the in-kernel module decompressor." >&2
        exit 1
    fi
done

sudo depmod

# Keep a copy of this build and deploy the auto-rollback boot service:
# at every boot it loads the P2P modules and, if that fails, restores the
# stock driver from $STOCK_BACKUP so the system always comes up with a
# working NVIDIA driver. Requires the stock backup convention
# /root/nvidia-kmod-backup-official-$DRIVER_VERSION/*.xz.
if [ -d "$STOCK_BACKUP" ]; then
    mkdir -p "$P2P_COPY" /var/lib/nvidia-p2p
    for m in $MODS; do
        cp "$KDIR/extra/$m.ko.xz" "$P2P_COPY/"
    done
    cat > /var/lib/nvidia-p2p/load-nvidia-p2p.sh <<EOS
#!/bin/bash
# Boot loader for the P2P-patched NVIDIA $DRIVER_VERSION open kernel modules.
# Installs the P2P modules, tries to load them; on any failure, falls back
# to the stock $DRIVER_VERSION modules so the system always gets a working driver.
LOG=/var/log/nvidia-p2p.log
KDIR=/lib/modules/\$(uname -r)
P2P=$P2P_COPY
STOCK=$STOCK_BACKUP

log() { echo "[\$(date '+%F %T')] \$*" | tee -a "\$LOG"; }
banner() { dmesg 2>/dev/null | grep "NVIDIA UNIX Open Kernel Module" | tail -1; }

log "=== nvidia-p2p boot attempt (P2P modules from \$P2P) ==="
cp "\$P2P"/*.xz "\$KDIR"/extra/ 2>>"\$LOG" || log "WARN: could not copy P2P modules"
depmod -a 2>>"\$LOG"

if modprobe nvidia 2>>"\$LOG"; then
    loaded_sv=\$(cat /sys/module/nvidia/srcversion 2>/dev/null)
    our_sv=\$(modinfo -F srcversion "\$KDIR/extra/nvidia.ko.xz" 2>/dev/null)
    if [ -n "\$our_sv" ] && [ "\$loaded_sv" = "\$our_sv" ]; then
        log "P2P module loaded OK (srcversion \$loaded_sv)"
        log "banner: \$(banner)"
    else
        log "nvidia already loaded as a DIFFERENT build (loaded=\$loaded_sv P2P=\$our_sv) - P2P takes effect at next clean reboot"
    fi
    exit 0
fi

log "P2P module FAILED to load (dmesg tail follows in \$LOG)"
dmesg 2>/dev/null | tail -30 >> "\$LOG"
log "Rolling back to stock $DRIVER_VERSION modules from \$STOCK"
cp "\$STOCK"/*.xz "\$KDIR"/extra/ 2>>"\$LOG" || log "FATAL: could not copy stock modules"
depmod -a 2>>"\$LOG"

if modprobe nvidia 2>>"\$LOG"; then
    log "Stock module loaded OK (fallback successful)"
    log "banner: \$(banner)"
    exit 0
fi
log "FATAL: stock module also failed to load"
exit 1
EOS
    chmod 755 /var/lib/nvidia-p2p/load-nvidia-p2p.sh
    # On SELinux systems init_t cannot execute var_lib_t files.
    command -v chcon >/dev/null 2>&1 && chcon -t bin_t /var/lib/nvidia-p2p/load-nvidia-p2p.sh 2>/dev/null || true
    cat > /etc/systemd/system/nvidia-p2p.service <<EOS
[Unit]
Description=Load patched NVIDIA P2P kernel modules at boot ($DRIVER_VERSION) with stock fallback
After=systemd-modules-load.service
Wants=systemd-modules-load.service

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/var/lib/nvidia-p2p/load-nvidia-p2p.sh

[Install]
WantedBy=multi-user.target
EOS
    systemctl daemon-reload
    systemctl enable nvidia-p2p.service
    echo "deployed boot fallback service (log: /var/log/nvidia-p2p.log)"
else
    echo "NOTE: $STOCK_BACKUP not found - no boot fallback service deployed."
    echo "      Keep a stock driver backup there so a broken P2P build can never"
    echo "      leave the host without a working driver."
fi

nvidia-smi
