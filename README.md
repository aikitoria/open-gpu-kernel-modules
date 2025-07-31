# NVIDIA driver 575.64.05 with P2P for 4090 and 5090

This allows using P2P on 4090 and 5090 GPUs with the 575.64.05 driver version.
See https://github.com/tinygrad/open-gpu-kernel-modules (various branches) for more info.

## How to use

1) Disable IOMMU in BIOS settings
2) Install https://www.nvidia.com/en-us/drivers/details/250991/
3) Run `./install.sh`
4) Reboot

