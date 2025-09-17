# NVIDIA driver 580.82.09 with P2P for 4090 and 5090

This allows using P2P on 4090 and 5090 GPUs with the 580.82.09 driver version.  

## How it works
This modifies the kernel driver to force enable BAR1 P2P mode on GPUs not intended to use it.  
Then, the transfers are done by directly writing to the other GPU physical addresses over DMA.  

IOMMU virtualization must be disabled to use the patch, or transfers will fail.  
Note that this is very dangerous if you run untrusted software or devices.

Proper IOMMU support is possible but would require registering IOVA mappings.  
That would be a lot more work and seems unneeded given the intended use.  

See https://github.com/tinygrad/open-gpu-kernel-modules/tree/550.54.15-p2p for a more detailed explanation.

## How to use

1) Enable DMA passthrough mode for IOMMU:  
    1) Edit `/etc/default/grub`
    2) Add `amd_iommu=on iommu=pt` to `GRUB_CMDLINE_LINUX_DEFAULT`
    3) Run `sudo update-grub`
2) Install https://www.nvidia.com/en-us/drivers/details/254126/
3) Run `./install.sh` in this repo
4) Reboot the server