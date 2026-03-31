# NVIDIA driver 595.58.03 with P2P for 4090 and 5090

This allows using P2P on 4090 and 5090 GPUs with the 595.45.04 driver version.
See https://github.com/tinygrad/open-gpu-kernel-modules (various branches) for more info.

## How it works

This modifies the kernel driver to force enable BAR1 P2P mode on GPUs not intended to use it.
Then, the transfers are done by directly writing to the other GPU physical addresses over DMA.

IOMMU virtualization must be disabled to use the patch, or transfers will fail.
Note that this is very dangerous if you run untrusted software or devices.

## How to use

1) Enable DMA passthrough mode for IOMMU:
    1) Edit `/etc/default/grub`
    2) Add `amd_iommu=on iommu=pt` to `GRUB_CMDLINE_LINUX_DEFAULT`
    3) Run `sudo update-grub`
2) Install https://www.nvidia.com/fr-fr/drivers/details/265902/
3) Run `./install.sh` in this repo
4) Reboot the server

## Potential issues

If P2P transfers are slow, make sure that your IOMMU is in passthrough (pt) mode and that ACS is disabled.
ACS can be disabled in BIOS or with [a kernel patch](https://github.com/baskerville/acs).
