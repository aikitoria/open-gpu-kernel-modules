# NVIDIA driver 590.48.01 with P2P for 4090 and 5090

This allows using P2P on 4090 and 5090 GPUs with the 590.48.01 driver version.  
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
2) Install https://www.nvidia.com/en-us/drivers/details/259267/
3) Run `./install.sh` in this repo
4) Reboot the server

## Potential issues

- On some systems you might additionally need to disable ACS.
- On some systems resizable BAR might be unavailable.
- 4090s come up with large BAR by default, but 5090s don't.
