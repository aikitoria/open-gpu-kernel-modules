#!/bin/bash
sudo rmmod nvidia_drm nvidia_modeset nvidia_uvm nvidia
set -e
make modules -j$(nproc)

# Kernels with CONFIG_MODULE_SIG_ALL (e.g. AlmaLinux) reject unsigned
# out-of-tree modules. Sign with the distro DKMS signing key, exactly
# like the stock DKMS build is signed. Skipped when modules are already
# signed (sign-file is not idempotent).
SIGN_FILE=/usr/src/kernels/$(uname -r)/scripts/sign-file
KEY=/var/lib/dkms/mok.key
CERT=/var/lib/dkms/mok.pub
if [ -x "$SIGN_FILE" ] && [ -f "$KEY" ] && [ -f "$CERT" ]; then
    for m in nvidia nvidia-uvm nvidia-modeset nvidia-drm nvidia-peermem; do
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

# Also install into extra/: depmod prefers extra/ over kernel/, which
# matters when the stock DKMS driver has installed its modules there.
KDIR=/lib/modules/$(uname -r)
mkdir -p "$KDIR/extra"
for m in nvidia nvidia-uvm nvidia-modeset nvidia-drm nvidia-peermem; do
    xz -c kernel-open/$m.ko > "$KDIR/extra/$m.ko.xz"
done

sudo depmod
nvidia-smi
