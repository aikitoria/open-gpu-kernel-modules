#!/bin/bash
set -e

SRC_DIR="$(cd "$(dirname "$0")" && pwd)"
PACKAGE_NAME="nvidia"
PACKAGE_VERSION="$(sed -n 's/^NVIDIA_VERSION = //p' "${SRC_DIR}/version.mk")"

# Update version in dkms.conf
sed -i "s/^PACKAGE_VERSION=.*/PACKAGE_VERSION=\"${PACKAGE_VERSION}\"/" "${SRC_DIR}/dkms.conf"

# Remove all previous registrations
# Format: "nvidia/590.48.01, 6.18.9+deb14-amd64, x86_64: built"
for ver in $(sudo dkms status -m "${PACKAGE_NAME}" 2>/dev/null | sed -n 's|^nvidia/\([^,]*\),.*|\1|p' | sort -u); do
    sudo dkms remove "${PACKAGE_NAME}/${ver}" --all 2>/dev/null || true
done
sudo rm -rf /usr/src/${PACKAGE_NAME}-*
sudo rm -rf /var/lib/dkms/${PACKAGE_NAME}

sudo ln -s "${SRC_DIR}" "/usr/src/${PACKAGE_NAME}-${PACKAGE_VERSION}"
sudo dkms add "${PACKAGE_NAME}/${PACKAGE_VERSION}"
sudo dkms build "${PACKAGE_NAME}/${PACKAGE_VERSION}"
sudo dkms install --force "${PACKAGE_NAME}/${PACKAGE_VERSION}"

echo "Done. Verify with: dkms status"
