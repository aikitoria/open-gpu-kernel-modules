# NVIDIA driver 595.71.05 with P2P for RTX 3090, RTX 4090, and RTX 5090

This enables P2P on consumer GPUs with the 595.71.05 driver version. No kernel parameters
are needed for the default behavior, just build, install, and go.

See the [tinygrad 550.54.15-p2p README](https://github.com/tinygrad/open-gpu-kernel-modules/blob/550.54.15-p2p/README.md)
for the original description of the approach.

## Supported configurations

| GPU      | P2P path                                                               |
| -------- | ---------------------------------------------------------------------- |
| RTX 3090 | Pairwise NVLink where available, PCIe BAR1 otherwise                   |
| RTX 4090 | PCIe BAR1                                                              |
| RTX 5090 | PCIe BAR1                                                              |

P2P also works between different devices of the same generation, for example RTX 5090
to RTX PRO 6000 Blackwell.

## How it works

This enables BAR1 P2P on consumer GPUs where NVLink isn't available, and falls back to
NVLink where it is. For PCIe pairs, transfers write directly to the other GPU's physical
address over DMA.

> [!WARNING]
> IOMMU must be in passthrough mode (`iommu=pt`), not translating, or DMA will go through
> IOMMU page tables and transfers will fail. This is very dangerous if you run untrusted
> software or devices.

## How to use

1. Enable DMA passthrough mode for the IOMMU:
   - Edit `/etc/default/grub`
   - Add `amd_iommu=on iommu=pt` to `GRUB_CMDLINE_LINUX_DEFAULT` (use `intel_iommu=on iommu=pt` on Intel)
   - Run `sudo update-grub`
2. Install the [NVIDIA 595.71.05 driver](https://www.nvidia.com/en-us/drivers/details/267223/)
3. Run `./install.sh` in this repo
4. Reboot the server

## Forcing 3090s to use PCIe instead of NVLink

For testing, you can make 3090 pairs fall back to PCIe BAR1 even when NVLink is present by
passing `NVreg_RegistryDwords="RMForceP2PType=1"` to the nvidia module. Add this to
`/etc/modprobe.d/nvidia.conf`:

```
options nvidia NVreg_RegistryDwords="RMForceP2PType=1"
```

## Experimental: faster cudaHostRegister for hugepage-backed memory

This branch also includes an experimental path that accelerates `cudaHostRegister` by
several orders of magnitude when the registered buffer is backed by 1G hugepages, and
shrinks the device page tables used for such mappings. It is enabled automatically. This
path skips some of the per-4K-page bookkeeping the stock driver performs, so it may
misbehave in edge cases the stock driver handles correctly.

## Potential issues

If P2P transfers are slow, make sure your IOMMU is in passthrough (`pt`) mode and that ACS
is disabled. ACS on root ports forces all GPU-to-GPU traffic through the CPU root complex,
killing P2P bandwidth. ACS can be disabled in BIOS, with the
`pcie_acs_override=downstream,multifunction` kernel parameter (if your kernel supports it),
or with an ACS override patch applied to the kernel.

## Sample `p2pBandwidthLatencyTest` output

9-GPU system (1x RTX PRO 6000 Blackwell + 8x RTX 5090) on a dual-socket AMD EPYC 9575F (Turin) host:

```
./p2pBandwidthLatencyTest
```

```
[P2P (Peer-to-Peer) GPU Bandwidth Latency Test]
Device: 0, NVIDIA RTX PRO 6000 Blackwell Workstation Edition, pciBusID: c1, pciDeviceID: 0, pciDomainID:0
Device: 1, NVIDIA GeForce RTX 5090, pciBusID: 1, pciDeviceID: 0, pciDomainID:0
Device: 2, NVIDIA GeForce RTX 5090, pciBusID: 11, pciDeviceID: 0, pciDomainID:0
Device: 3, NVIDIA GeForce RTX 5090, pciBusID: 61, pciDeviceID: 0, pciDomainID:0
Device: 4, NVIDIA GeForce RTX 5090, pciBusID: 71, pciDeviceID: 0, pciDomainID:0
Device: 5, NVIDIA GeForce RTX 5090, pciBusID: 81, pciDeviceID: 0, pciDomainID:0
Device: 6, NVIDIA GeForce RTX 5090, pciBusID: 91, pciDeviceID: 0, pciDomainID:0
Device: 7, NVIDIA GeForce RTX 5090, pciBusID: e1, pciDeviceID: 0, pciDomainID:0
Device: 8, NVIDIA GeForce RTX 5090, pciBusID: f1, pciDeviceID: 0, pciDomainID:0

***NOTE: In case a device doesn't have P2P access to other one, it falls back to normal memcopy procedure.
So you can see lesser Bandwidth (GB/s) and unstable Latency (us) in those cases.

P2P Connectivity Matrix
     D\D     0     1     2     3     4     5     6     7     8
     0	     1     1     1     1     1     1     1     1     1
     1	     1     1     1     1     1     1     1     1     1
     2	     1     1     1     1     1     1     1     1     1
     3	     1     1     1     1     1     1     1     1     1
     4	     1     1     1     1     1     1     1     1     1
     5	     1     1     1     1     1     1     1     1     1
     6	     1     1     1     1     1     1     1     1     1
     7	     1     1     1     1     1     1     1     1     1
     8	     1     1     1     1     1     1     1     1     1
Unidirectional P2P=Disabled Bandwidth Matrix (GB/s)
   D\D     0      1      2      3      4      5      6      7      8 
     0 1611.24  43.30  42.82  42.88  42.89  43.69  43.61  43.63  43.74 
     1  43.38 1658.76  42.71  42.69  42.83  43.34  43.47  43.33  43.50 
     2  43.53  42.82 1664.06  42.71  42.74  43.19  43.31  43.24  43.34 
     3  43.51  42.91  42.80 1660.58  42.77  43.20  43.27  43.17  43.36 
     4  43.47  43.00  42.83  42.82 1664.06  43.43  43.32  43.32  43.49 
     5  43.81  42.95  42.84  42.84  42.94 1665.83  43.52  43.45  43.48 
     6  43.76  43.04  42.93  42.99  43.01  43.85 1662.29  43.66  43.74 
     7  43.77  42.95  42.90  43.10  43.00  43.85  43.77 1662.29  43.74 
     8  43.84  43.08  43.04  43.03  43.25  43.91  43.87  43.89 1658.70 
Unidirectional P2P=Enabled Bandwidth (P2P Writes) Matrix (GB/s)
   D\D     0      1      2      3      4      5      6      7      8 
     0 1617.49  55.59  55.62  55.64  55.64  56.58  56.58  56.58  56.58 
     1  55.60 1656.95  56.57  56.55  56.57  55.63  55.63  55.62  55.64 
     2  55.63  56.55 1660.47  56.55  56.57  55.62  55.62  55.63  55.61 
     3  55.63  56.58  56.55 1658.70  56.55  55.63  55.63  55.59  55.63 
     4  55.63  56.55  56.58  56.58 1656.95  55.62  55.61  55.61  55.64 
     5  56.57  55.62  55.62  55.64  55.64 1662.23  56.55  56.58  56.58 
     6  56.50  55.62  55.62  55.61  55.64  56.58 1662.23  56.58  56.58 
     7  56.58  55.62  55.62  55.64  55.65  56.58  56.58 1665.78  56.57 
     8  56.55  55.62  55.62  55.64  55.63  56.58  56.55  56.58 1662.23 
Bidirectional P2P=Disabled Bandwidth Matrix (GB/s)
   D\D     0      1      2      3      4      5      6      7      8 
     0 1600.87  56.70  56.88  57.10  56.47  57.06  56.93  57.22  57.30 
     1  57.17 1642.93  56.50  56.74  56.48  57.19  56.77  56.87  56.75 
     2  56.87  56.90 1645.55  56.69  56.67  56.93  57.00  56.76  56.83 
     3  56.95  56.85  56.50 1640.37  56.66  56.78  56.98  57.48  57.40 
     4  56.50  56.25  56.58  56.61 1644.68  57.09  57.05  56.91  57.09 
     5  57.31  56.94  56.67  56.83  56.78 1642.06  57.21  57.17  57.47 
     6  56.85  56.95  56.87  56.99  56.64  57.29 1642.09  57.09  57.02 
     7  57.15  57.05  56.85  56.54  56.95  56.71  57.20 1646.42  56.96 
     8  57.00  56.73  57.09  56.82  56.49  57.08  56.90  57.06 1642.95 
Bidirectional P2P=Enabled Bandwidth Matrix (GB/s)
   D\D     0      1      2      3      4      5      6      7      8 
     0 1600.87 111.17 111.04 111.14 111.12 111.35 111.34 111.39 111.38 
     1 111.12 1636.90 111.38 111.39 111.34 111.10 111.08 111.11 111.08 
     2 111.08 111.38 1639.51 111.41 111.38 111.08 110.95 111.13 110.98 
     3 111.13 111.40 111.39 1641.23 111.39 111.12 111.15 110.83 111.09 
     4 111.13 111.40 111.34 111.40 1642.95 111.07 111.12 111.15 111.11 
     5 111.35 111.13 111.11 111.15 110.97 1640.34 111.45 111.39 111.43 
     6 111.45 111.01 111.09 111.14 111.17 111.39 1642.95 111.39 111.45 
     7 111.45 111.11 111.18 111.12 111.17 111.40 111.40 1640.37 111.40 
     8 111.34 111.19 111.10 111.04 111.17 111.39 111.40 111.39 1637.76 
P2P=Disabled Latency Matrix (us)
   GPU     0      1      2      3      4      5      6      7      8 
     0   1.04  14.31  14.22  14.35  14.30  14.05  14.22   7.98  14.35 
     1  14.44   0.98  14.31  14.31  14.31  14.32  14.30  14.32  14.34 
     2  14.32  14.31   0.98  14.32  14.31  14.30  14.31  14.32  14.35 
     3  14.32  14.32  14.32   1.01  14.32  14.31  14.32  14.32  14.32 
     4  14.33  14.31  14.32  14.30   0.97  14.30  14.32  14.31  14.33 
     5  14.33  14.24  14.31  14.16  14.15   0.95  14.32  14.32  14.31 
     6  12.61  14.06  14.32  14.32  14.33  14.33   0.98  14.32  14.32 
     7  14.33  14.33  14.30  14.32  14.26  14.33  14.24   0.92  12.56 
     8  12.51  14.32  14.22  14.31  14.32  14.25  14.33  14.31   0.96 

   CPU     0      1      2      3      4      5      6      7      8 
     0   1.76   5.83   6.08   5.98   5.66   4.93   5.29   5.23   4.88 
     1   5.48   1.87   6.36   6.34   6.04   5.30   5.60   5.58   5.26 
     2   5.70   6.24   2.01   6.56   6.26   5.48   5.87   5.83   5.46 
     3   5.64   6.27   6.54   2.00   6.24   5.52   5.83   5.79   5.45 
     4   5.46   6.03   6.34   6.31   1.89   5.31   5.62   5.59   5.24 
     5   4.93   5.50   5.84   5.83   5.48   1.62   5.12   5.08   4.76 
     6   5.15   5.71   6.04   6.04   5.74   4.98   1.75   5.34   4.97 
     7   5.11   5.73   5.99   5.99   5.71   5.02   5.32   1.73   4.97 
     8   4.93   5.47   5.78   5.80   5.48   4.74   5.05   5.05   1.63 
P2P=Enabled Latency (P2P Writes) Matrix (us)
   GPU     0      1      2      3      4      5      6      7      8 
     0   1.02   0.38   0.42   0.36   0.37   0.37   0.44   0.36   0.36 
     1   0.45   0.98   0.37   0.44   0.38   0.43   0.45   0.45   0.39 
     2   0.37   0.38   0.96   0.36   0.38   0.37   0.38   0.38   0.38 
     3   0.44   0.44   0.43   1.01   0.38   0.37   0.45   0.38   0.44 
     4   0.43   0.36   0.37   0.37   0.95   0.35   0.38   0.44   0.44 
     5   0.37   0.36   0.36   0.36   0.35   0.95   0.36   0.36   0.36 
     6   0.36   0.43   0.43   0.36   0.43   0.36   0.98   0.43   0.36 
     7   0.37   0.35   0.36   0.35   0.36   0.36   0.43   0.92   0.43 
     8   0.38   0.44   0.36   0.44   0.37   0.38   0.37   0.37   0.96 

   CPU     0      1      2      3      4      5      6      7      8 
     0   1.75   1.34   1.31   1.31   1.31   1.32   1.36   1.32   1.31 
     1   1.52   1.88   1.49   1.53   1.52   1.53   1.52   1.58   1.52 
     2   1.67   1.64   2.04   1.64   1.64   1.65   1.65   1.64   1.66 
     3   1.66   1.63   1.63   2.00   1.64   1.63   1.64   1.63   1.63 
     4   1.55   1.53   1.53   1.53   1.93   1.53   1.54   1.53   1.53 
     5   1.32   1.31   1.30   1.30   1.31   1.68   1.30   1.31   1.30 
     6   1.42   1.39   1.40   1.41   1.41   1.41   1.77   1.41   1.41 
     7   1.42   1.39   1.45   1.39   1.39   1.40   1.40   1.74   1.40 
     8   1.33   1.28   1.29   1.32   1.30   1.29   1.29   1.30   1.66 
```

## Sample `nccl-tests` `all_reduce_perf` output

8x RTX 5090 on the same host:

```
CUDA_VISIBLE_DEVICES=1,2,3,4,5,6,7,8 NCCL_P2P_LEVEL=SYS ./build/all_reduce_perf -b 8 -e 128M -f 2 -g 8
```

```
# nccl-tests version 2.18.2 nccl-headers=22907 nccl-library=22907
# Collective test starting: all_reduce_perf
# nThread 1 nGpus 8 minBytes 8 maxBytes 134217728 step: 2(factor) warmup iters: 1 iters: 20 agg iters: 1 validation: 1 graph: 0 unalign: 0
#
# Using devices
#  Rank  0 Group  0 Pid  27238 on      crazy device  0 [0000:01:00] NVIDIA GeForce RTX 5090
#  Rank  1 Group  0 Pid  27238 on      crazy device  1 [0000:11:00] NVIDIA GeForce RTX 5090
#  Rank  2 Group  0 Pid  27238 on      crazy device  2 [0000:61:00] NVIDIA GeForce RTX 5090
#  Rank  3 Group  0 Pid  27238 on      crazy device  3 [0000:71:00] NVIDIA GeForce RTX 5090
#  Rank  4 Group  0 Pid  27238 on      crazy device  4 [0000:81:00] NVIDIA GeForce RTX 5090
#  Rank  5 Group  0 Pid  27238 on      crazy device  5 [0000:91:00] NVIDIA GeForce RTX 5090
#  Rank  6 Group  0 Pid  27238 on      crazy device  6 [0000:e1:00] NVIDIA GeForce RTX 5090
#  Rank  7 Group  0 Pid  27238 on      crazy device  7 [0000:f1:00] NVIDIA GeForce RTX 5090
#
#                                                              out-of-place                       in-place          
#       size         count      type   redop    root     time   algbw   busbw  #wrong     time   algbw   busbw  #wrong 
#        (B)    (elements)                               (us)  (GB/s)  (GB/s)             (us)  (GB/s)  (GB/s)         
           8             2     float     sum      -1    25.47    0.00    0.00       0    24.82    0.00    0.00       0
          16             4     float     sum      -1    24.52    0.00    0.00       0    24.60    0.00    0.00       0
          32             8     float     sum      -1    24.65    0.00    0.00       0    24.70    0.00    0.00       0
          64            16     float     sum      -1    24.72    0.00    0.00       0    24.78    0.00    0.00       0
         128            32     float     sum      -1    24.67    0.01    0.01       0    24.79    0.01    0.01       0
         256            64     float     sum      -1    24.77    0.01    0.02       0    24.56    0.01    0.02       0
         512           128     float     sum      -1    24.62    0.02    0.04       0    24.62    0.02    0.04       0
        1024           256     float     sum      -1    24.66    0.04    0.07       0    24.65    0.04    0.07       0
        2048           512     float     sum      -1    24.54    0.08    0.15       0    24.66    0.08    0.15       0
        4096          1024     float     sum      -1    36.29    0.11    0.20       0    35.21    0.12    0.20       0
        8192          2048     float     sum      -1    37.00    0.22    0.39       0    36.06    0.23    0.40       0
       16384          4096     float     sum      -1    38.06    0.43    0.75       0    37.14    0.44    0.77       0
       32768          8192     float     sum      -1    39.33    0.83    1.46       0    38.42    0.85    1.49       0
       65536         16384     float     sum      -1    40.96    1.60    2.80       0    40.07    1.64    2.86       0
      131072         32768     float     sum      -1    42.19    3.11    5.44       0    41.28    3.18    5.56       0
      262144         65536     float     sum      -1    45.87    5.71   10.00       0    45.00    5.83   10.19       0
      524288        131072     float     sum      -1    56.96    9.20   16.11       0    57.42    9.13   15.98       0
     1048576        262144     float     sum      -1    74.74   14.03   24.55       0    72.38   14.49   25.35       0
     2097152        524288     float     sum      -1   104.50   20.07   35.12       0   107.73   19.47   34.07       0
     4194304       1048576     float     sum      -1   176.93   23.71   41.48       0   178.56   23.49   41.11       0
     8388608       2097152     float     sum      -1   326.13   25.72   45.01       0   336.41   24.94   43.64       0
    16777216       4194304     float     sum      -1   646.48   25.95   45.42       0   647.96   25.89   45.31       0
    33554432       8388608     float     sum      -1  1284.81   26.12   45.70       0  1286.28   26.09   45.65       0
    67108864      16777216     float     sum      -1  2585.56   25.96   45.42       0  2583.09   25.98   45.47       0
   134217728      33554432     float     sum      -1  5355.16   25.06   43.86       0  5329.43   25.18   44.07       0
```

---

# NVIDIA Linux Open GPU Kernel Module Source

This is the source release of the NVIDIA Linux open GPU kernel modules,
version 595.71.05.


## How to Build

To build:

    make modules -j$(nproc)

To install, first uninstall any existing NVIDIA kernel modules.  Then,
as root:

    make modules_install -j$(nproc)

Note that the kernel modules built here must be used with GSP
firmware and user-space NVIDIA GPU driver components from a corresponding
595.71.05 driver release.  This can be achieved by installing
the NVIDIA GPU driver from the .run file using the `--no-kernel-modules`
option.  E.g.,

    sh ./NVIDIA-Linux-[...].run --no-kernel-modules


## Supported Target CPU Architectures

Currently, the kernel modules can be built for x86_64 or aarch64.
If cross-compiling, set these variables on the make command line:

    TARGET_ARCH=aarch64|x86_64
    CC
    LD
    AR
    CXX
    OBJCOPY

E.g.,

    # compile on x86_64 for aarch64
    make modules -j$(nproc)         \
        TARGET_ARCH=aarch64         \
        CC=aarch64-linux-gnu-gcc    \
        LD=aarch64-linux-gnu-ld     \
        AR=aarch64-linux-gnu-ar     \
        CXX=aarch64-linux-gnu-g++   \
        OBJCOPY=aarch64-linux-gnu-objcopy


## Other Build Knobs

NV_VERBOSE - Set this to "1" to print each complete command executed;
    otherwise, a succinct "CC" line is printed.

DEBUG - Set this to "1" to build the kernel modules as debug.  By default, the
    build compiles without debugging information.  This also enables
    various debug log messages in the kernel modules.

These variables can be set on the make command line.  E.g.,

    make modules -j$(nproc) NV_VERBOSE=1


## Supported Toolchains

Any reasonably modern version of GCC or Clang can be used to build the
kernel modules.  Note that the kernel interface layers of the kernel
modules must be built with the toolchain that was used to build the
kernel.


## Supported Linux Kernel Versions

The NVIDIA open kernel modules support the same range of Linux kernel
versions that are supported with the proprietary NVIDIA kernel modules.
This is currently Linux kernel 4.15 or newer.


## How to Contribute

Contributions can be made by creating a pull request on
https://github.com/NVIDIA/open-gpu-kernel-modules
We'll respond via GitHub.

Note that when submitting a pull request, you will be prompted to accept
a Contributor License Agreement.

This code base is shared with NVIDIA's proprietary drivers, and various
processing is performed on the shared code to produce the source code that is
published here.  This has several implications for the foreseeable future:

* The GitHub repository will function mostly as a snapshot of each driver
  release.

* We do not expect to be able to provide revision history for individual
  changes that were made to NVIDIA's shared code base.  There will likely
  only be one git commit per driver release.

* We may not be able to reflect individual contributions as separate
  git commits in the GitHub repository.

* Because the code undergoes various processing prior to publishing here,
  contributions made here require manual merging to be applied to the shared
  code base.  Therefore, large refactoring changes made here may be difficult to
  merge and accept back into the shared code base.  If you have large
  refactoring to suggest, please contact us in advance, so we can coordinate.


## How to Report Issues

Problems specific to the Open GPU Kernel Modules can be reported in the
Issues section of the https://github.com/NVIDIA/open-gpu-kernel-modules
repository.

Further, any of the existing bug reporting venues can be used to communicate
problems to NVIDIA, such as our forum:

https://forums.developer.nvidia.com/c/gpu-graphics/linux/148

or linux-bugs@nvidia.com.

Please see the 'NVIDIA Contact Info and Additional Resources' section
of the NVIDIA GPU Driver README for details.

Please see the separate [SECURITY.md](SECURITY.md) document if you
believe you have discovered a security vulnerability in this software.


## Kernel Interface and OS-Agnostic Components of Kernel Modules

Most of NVIDIA's kernel modules are split into two components:

* An "OS-agnostic" component: this is the component of each kernel module
  that is independent of operating system.

* A "kernel interface layer": this is the component of each kernel module
  that is specific to the Linux kernel version and configuration.

When packaged in the NVIDIA .run installation package, the OS-agnostic
component is provided as a binary: it is large and time-consuming to
compile, so pre-built versions are provided so that the user does
not have to compile it during every driver installation.  For the
nvidia.ko kernel module, this component is named "nv-kernel.o_binary".
For the nvidia-modeset.ko kernel module, this component is named
"nv-modeset-kernel.o_binary".  Neither nvidia-drm.ko nor nvidia-uvm.ko
have OS-agnostic components.

The kernel interface layer component for each kernel module must be built
for the target kernel.


## Directory Structure Layout

- `kernel-open/`                The kernel interface layer
- `kernel-open/nvidia/`         The kernel interface layer for nvidia.ko
- `kernel-open/nvidia-drm/`     The kernel interface layer for nvidia-drm.ko
- `kernel-open/nvidia-modeset/` The kernel interface layer for nvidia-modeset.ko
- `kernel-open/nvidia-uvm/`     The kernel interface layer for nvidia-uvm.ko

- `src/`                        The OS-agnostic code
- `src/nvidia/`                 The OS-agnostic code for nvidia.ko
- `src/nvidia-modeset/`         The OS-agnostic code for nvidia-modeset.ko
- `src/common/`                 Utility code used by one or more of nvidia.ko and nvidia-modeset.ko
- `nouveau/`                    Tools for integration with the Nouveau device driver


## Nouveau device driver integration

The Python script in the 'nouveau' directory is used to extract some of the
firmware binary images (and related data) encoded in the source code and
store them as distinct files.  These files are used by the Nouveau device
driver to load and communicate with the GSP firmware.

The layout of the binary files is described in nouveau_firmware_layout.ods,
which is an OpenDocument Spreadsheet file, compatible with most spreadsheet
software applications.


## Compatible GPUs

The NVIDIA open kernel modules can be used on any Turing or later GPU (see the
table below).

For details on feature support and limitations, see the NVIDIA GPU driver
end user README here:

https://us.download.nvidia.com/XFree86/Linux-x86_64/595.71.05/README/kernel_open.html

For vGPU support, please refer to the README.vgpu packaged in the vGPU Host
Package for more details.

In the below table, if three IDs are listed, the first is the PCI Device 
ID, the second is the PCI Subsystem Vendor ID, and the third is the PCI
Subsystem Device ID.

| Product Name                                            | PCI ID         |
| ------------------------------------------------------- | -------------- |
| NVIDIA TITAN RTX                                        | 1E02           |
| NVIDIA GeForce RTX 2080 Ti                              | 1E04           |
| NVIDIA GeForce RTX 2080 Ti                              | 1E07           |
| NVIDIA CMP 50HX                                         | 1E09           |
| Quadro RTX 6000                                         | 1E30           |
| Quadro RTX 8000                                         | 1E30 1028 129E |
| Quadro RTX 8000                                         | 1E30 103C 129E |
| Quadro RTX 8000                                         | 1E30 10DE 129E |
| Quadro RTX 6000                                         | 1E36           |
| Quadro RTX 8000                                         | 1E78 10DE 13D8 |
| Quadro RTX 6000                                         | 1E78 10DE 13D9 |
| NVIDIA GeForce RTX 2080 SUPER                           | 1E81           |
| NVIDIA GeForce RTX 2080                                 | 1E82           |
| NVIDIA GeForce RTX 2070 SUPER                           | 1E84           |
| NVIDIA GeForce RTX 2080                                 | 1E87           |
| NVIDIA GeForce RTX 2060                                 | 1E89           |
| NVIDIA GeForce RTX 2080                                 | 1E90           |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 1025 1375 |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 1028 08A1 |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 1028 08A2 |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 1028 08EA |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 1028 08EB |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 1028 08EC |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 1028 08ED |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 1028 08EE |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 1028 08EF |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 1028 093B |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 1028 093C |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 103C 8572 |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 103C 8573 |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 103C 8602 |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 103C 8606 |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 103C 86C6 |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 103C 86C7 |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 103C 87A6 |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 103C 87A7 |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 1043 131F |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 1043 137F |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 1043 141F |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 1043 1751 |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 1458 1660 |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 1458 1661 |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 1458 1662 |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 1458 75A6 |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 1458 75A7 |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 1458 86A6 |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 1458 86A7 |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 1462 1274 |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 1462 1277 |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 152D 1220 |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 1558 95E1 |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 1558 97E1 |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 1A58 2002 |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 1A58 2005 |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 1A58 2007 |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 1A58 3000 |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 1A58 3001 |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1E90 1D05 1069 |
| NVIDIA GeForce RTX 2070 Super                           | 1E91           |
| NVIDIA GeForce RTX 2070 Super with Max-Q Design         | 1E91 103C 8607 |
| NVIDIA GeForce RTX 2070 Super with Max-Q Design         | 1E91 103C 8736 |
| NVIDIA GeForce RTX 2070 Super with Max-Q Design         | 1E91 103C 8738 |
| NVIDIA GeForce RTX 2070 Super with Max-Q Design         | 1E91 103C 8772 |
| NVIDIA GeForce RTX 2070 Super with Max-Q Design         | 1E91 103C 878A |
| NVIDIA GeForce RTX 2070 Super with Max-Q Design         | 1E91 103C 878B |
| NVIDIA GeForce RTX 2070 Super with Max-Q Design         | 1E91 1043 1E61 |
| NVIDIA GeForce RTX 2070 Super with Max-Q Design         | 1E91 1458 1511 |
| NVIDIA GeForce RTX 2070 Super with Max-Q Design         | 1E91 1458 75B3 |
| NVIDIA GeForce RTX 2070 Super with Max-Q Design         | 1E91 1458 75B4 |
| NVIDIA GeForce RTX 2070 Super with Max-Q Design         | 1E91 1458 76B2 |
| NVIDIA GeForce RTX 2070 Super with Max-Q Design         | 1E91 1458 76B3 |
| NVIDIA GeForce RTX 2070 Super with Max-Q Design         | 1E91 1458 78A2 |
| NVIDIA GeForce RTX 2070 Super with Max-Q Design         | 1E91 1458 78A3 |
| NVIDIA GeForce RTX 2070 Super with Max-Q Design         | 1E91 1458 86B2 |
| NVIDIA GeForce RTX 2070 Super with Max-Q Design         | 1E91 1458 86B3 |
| NVIDIA GeForce RTX 2070 Super with Max-Q Design         | 1E91 1462 12AE |
| NVIDIA GeForce RTX 2070 Super with Max-Q Design         | 1E91 1462 12B0 |
| NVIDIA GeForce RTX 2070 Super with Max-Q Design         | 1E91 1462 12C6 |
| NVIDIA GeForce RTX 2070 Super with Max-Q Design         | 1E91 17AA 22C3 |
| NVIDIA GeForce RTX 2070 Super with Max-Q Design         | 1E91 17AA 22C5 |
| NVIDIA GeForce RTX 2070 Super with Max-Q Design         | 1E91 1A58 2009 |
| NVIDIA GeForce RTX 2070 Super with Max-Q Design         | 1E91 1A58 200A |
| NVIDIA GeForce RTX 2070 Super with Max-Q Design         | 1E91 1A58 3002 |
| NVIDIA GeForce RTX 2070 Super with Max-Q Design         | 1E91 8086 3012 |
| NVIDIA GeForce RTX 2080 Super                           | 1E93           |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1E93 1025 1401 |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1E93 1025 149C |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1E93 1028 09D2 |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1E93 103C 8607 |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1E93 103C 86C7 |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1E93 103C 8736 |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1E93 103C 8738 |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1E93 103C 8772 |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1E93 103C 87A6 |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1E93 103C 87A7 |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1E93 1458 75B1 |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1E93 1458 75B2 |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1E93 1458 76B0 |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1E93 1458 76B1 |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1E93 1458 78A0 |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1E93 1458 78A1 |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1E93 1458 86B0 |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1E93 1458 86B1 |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1E93 1462 12AE |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1E93 1462 12B0 |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1E93 1462 12B4 |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1E93 1462 12C6 |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1E93 1558 50D3 |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1E93 1558 70D1 |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1E93 17AA 22C3 |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1E93 17AA 22C5 |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1E93 1A58 2009 |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1E93 1A58 200A |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1E93 1A58 3002 |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1E93 1D05 1089 |
| Quadro RTX 5000                                         | 1EB0           |
| Quadro RTX 4000                                         | 1EB1           |
| Quadro RTX 5000                                         | 1EB5           |
| Quadro RTX 5000 with Max-Q Design                       | 1EB5 1025 1375 |
| Quadro RTX 5000 with Max-Q Design                       | 1EB5 1025 1401 |
| Quadro RTX 5000 with Max-Q Design                       | 1EB5 1025 149C |
| Quadro RTX 5000 with Max-Q Design                       | 1EB5 1028 09C3 |
| Quadro RTX 5000 with Max-Q Design                       | 1EB5 103C 8736 |
| Quadro RTX 5000 with Max-Q Design                       | 1EB5 103C 8738 |
| Quadro RTX 5000 with Max-Q Design                       | 1EB5 103C 8772 |
| Quadro RTX 5000 with Max-Q Design                       | 1EB5 103C 8780 |
| Quadro RTX 5000 with Max-Q Design                       | 1EB5 103C 8782 |
| Quadro RTX 5000 with Max-Q Design                       | 1EB5 103C 8783 |
| Quadro RTX 5000 with Max-Q Design                       | 1EB5 103C 8785 |
| Quadro RTX 5000 with Max-Q Design                       | 1EB5 1043 1DD1 |
| Quadro RTX 5000 with Max-Q Design                       | 1EB5 1462 1274 |
| Quadro RTX 5000 with Max-Q Design                       | 1EB5 1462 12B0 |
| Quadro RTX 5000 with Max-Q Design                       | 1EB5 1462 12C6 |
| Quadro RTX 5000 with Max-Q Design                       | 1EB5 17AA 22B8 |
| Quadro RTX 5000 with Max-Q Design                       | 1EB5 17AA 22BA |
| Quadro RTX 5000 with Max-Q Design                       | 1EB5 1A58 2005 |
| Quadro RTX 5000 with Max-Q Design                       | 1EB5 1A58 2007 |
| Quadro RTX 5000 with Max-Q Design                       | 1EB5 1A58 2008 |
| Quadro RTX 5000 with Max-Q Design                       | 1EB5 1A58 200A |
| Quadro RTX 4000                                         | 1EB6           |
| Quadro RTX 4000 with Max-Q Design                       | 1EB6 1028 09C3 |
| Quadro RTX 4000 with Max-Q Design                       | 1EB6 103C 8736 |
| Quadro RTX 4000 with Max-Q Design                       | 1EB6 103C 8738 |
| Quadro RTX 4000 with Max-Q Design                       | 1EB6 103C 8772 |
| Quadro RTX 4000 with Max-Q Design                       | 1EB6 103C 8780 |
| Quadro RTX 4000 with Max-Q Design                       | 1EB6 103C 8782 |
| Quadro RTX 4000 with Max-Q Design                       | 1EB6 103C 8783 |
| Quadro RTX 4000 with Max-Q Design                       | 1EB6 103C 8785 |
| Quadro RTX 4000 with Max-Q Design                       | 1EB6 1462 1274 |
| Quadro RTX 4000 with Max-Q Design                       | 1EB6 1462 1277 |
| Quadro RTX 4000 with Max-Q Design                       | 1EB6 1462 12B0 |
| Quadro RTX 4000 with Max-Q Design                       | 1EB6 1462 12C6 |
| Quadro RTX 4000 with Max-Q Design                       | 1EB6 17AA 22B8 |
| Quadro RTX 4000 with Max-Q Design                       | 1EB6 17AA 22BA |
| Tesla T4                                                | 1EB8 10DE 12A2 |
| NVIDIA GeForce RTX 2070 SUPER                           | 1EC2           |
| NVIDIA GeForce RTX 2070 SUPER                           | 1EC7           |
| NVIDIA GeForce RTX 2080                                 | 1ED0           |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1ED0 1025 132D |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1ED0 1028 08ED |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1ED0 1028 08EE |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1ED0 1028 08EF |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1ED0 103C 8572 |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1ED0 103C 8573 |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1ED0 103C 8600 |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1ED0 103C 8605 |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1ED0 1043 138F |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1ED0 1043 15C1 |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1ED0 17AA 3FEE |
| NVIDIA GeForce RTX 2080 with Max-Q Design               | 1ED0 17AA 3FFE |
| NVIDIA GeForce RTX 2070 Super                           | 1ED1           |
| NVIDIA GeForce RTX 2070 Super with Max-Q Design         | 1ED1 1025 1432 |
| NVIDIA GeForce RTX 2070 Super with Max-Q Design         | 1ED1 103C 8746 |
| NVIDIA GeForce RTX 2070 Super with Max-Q Design         | 1ED1 103C 878A |
| NVIDIA GeForce RTX 2070 Super with Max-Q Design         | 1ED1 1043 165F |
| NVIDIA GeForce RTX 2070 Super with Max-Q Design         | 1ED1 144D C192 |
| NVIDIA GeForce RTX 2070 Super with Max-Q Design         | 1ED1 17AA 3FCE |
| NVIDIA GeForce RTX 2070 Super with Max-Q Design         | 1ED1 17AA 3FCF |
| NVIDIA GeForce RTX 2070 Super with Max-Q Design         | 1ED1 17AA 3FD0 |
| NVIDIA GeForce RTX 2080 Super                           | 1ED3           |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1ED3 1025 1432 |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1ED3 1028 09D1 |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1ED3 103C 8746 |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1ED3 103C 878A |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1ED3 1043 1D61 |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1ED3 1043 1E51 |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1ED3 1043 1F01 |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1ED3 17AA 3FCE |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1ED3 17AA 3FCF |
| NVIDIA GeForce RTX 2080 Super with Max-Q Design         | 1ED3 17AA 3FD0 |
| Quadro RTX 5000                                         | 1EF5           |
| NVIDIA GeForce RTX 2070                                 | 1F02           |
| NVIDIA GeForce RTX 2060                                 | 1F03           |
| NVIDIA GeForce RTX 2060 SUPER                           | 1F06           |
| NVIDIA GeForce RTX 2070                                 | 1F07           |
| NVIDIA GeForce RTX 2060                                 | 1F08           |
| NVIDIA GeForce GTX 1650                                 | 1F0A           |
| NVIDIA CMP 40HX                                         | 1F0B           |
| NVIDIA GeForce RTX 2070                                 | 1F10           |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 1025 132D |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 1025 1342 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 1028 08A1 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 1028 08A2 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 1028 08EA |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 1028 08EB |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 1028 08EC |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 1028 08ED |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 1028 08EE |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 1028 08EF |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 1028 093B |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 1028 093C |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 103C 8572 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 103C 8573 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 103C 8602 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 103C 8606 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 1043 132F |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 1043 136F |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 1043 1881 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 1043 1E6E |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 1458 1658 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 1458 1663 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 1458 1664 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 1458 75A4 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 1458 75A5 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 1458 86A4 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 1458 86A5 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 1462 1274 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 1462 1277 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 1558 95E1 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 1558 97E1 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 1A58 2002 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 1A58 2005 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 1A58 2007 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 1A58 3000 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 1A58 3001 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 1D05 105E |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 1D05 1070 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 1D05 2087 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F10 8086 2087 |
| NVIDIA GeForce RTX 2060                                 | 1F11           |
| NVIDIA GeForce RTX 2060                                 | 1F12           |
| NVIDIA GeForce RTX 2060 with Max-Q Design               | 1F12 1028 098F |
| NVIDIA GeForce RTX 2060 with Max-Q Design               | 1F12 103C 8741 |
| NVIDIA GeForce RTX 2060 with Max-Q Design               | 1F12 103C 8744 |
| NVIDIA GeForce RTX 2060 with Max-Q Design               | 1F12 103C 878E |
| NVIDIA GeForce RTX 2060 with Max-Q Design               | 1F12 103C 880E |
| NVIDIA GeForce RTX 2060 with Max-Q Design               | 1F12 1043 1E11 |
| NVIDIA GeForce RTX 2060 with Max-Q Design               | 1F12 1043 1F11 |
| NVIDIA GeForce RTX 2060 with Max-Q Design               | 1F12 1462 12D9 |
| NVIDIA GeForce RTX 2060 with Max-Q Design               | 1F12 17AA 3801 |
| NVIDIA GeForce RTX 2060 with Max-Q Design               | 1F12 17AA 3802 |
| NVIDIA GeForce RTX 2060 with Max-Q Design               | 1F12 17AA 3803 |
| NVIDIA GeForce RTX 2070                                 | 1F14           |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F14 1025 1401 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F14 1025 1432 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F14 1025 1442 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F14 1025 1446 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F14 1025 147D |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F14 1028 09E2 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F14 1028 09F3 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F14 103C 8607 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F14 103C 86C6 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F14 103C 86C7 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F14 103C 8736 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F14 103C 8738 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F14 103C 8746 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F14 103C 8772 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F14 103C 878A |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F14 103C 878B |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F14 103C 87A6 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F14 103C 87A7 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F14 1043 174F |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F14 1458 1512 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F14 1458 75B5 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F14 1458 75B6 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F14 1458 76B4 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F14 1458 76B5 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F14 1458 78A4 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F14 1458 78A5 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F14 1458 86B4 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F14 1458 86B5 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F14 1462 12AE |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F14 1462 12B0 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F14 1462 12C6 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F14 1558 50D3 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F14 1558 70D1 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F14 1A58 200C |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F14 1A58 2011 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F14 1A58 3002 |
| NVIDIA GeForce RTX 2060                                 | 1F15           |
| Quadro RTX 3000                                         | 1F36           |
| Quadro RTX 3000 with Max-Q Design                       | 1F36 1028 0990 |
| Quadro RTX 3000 with Max-Q Design                       | 1F36 103C 8736 |
| Quadro RTX 3000 with Max-Q Design                       | 1F36 103C 8738 |
| Quadro RTX 3000 with Max-Q Design                       | 1F36 103C 8772 |
| Quadro RTX 3000 with Max-Q Design                       | 1F36 1043 13CF |
| Quadro RTX 3000 with Max-Q Design                       | 1F36 1414 0032 |
| NVIDIA GeForce RTX 2060 SUPER                           | 1F42           |
| NVIDIA GeForce RTX 2060 SUPER                           | 1F47           |
| NVIDIA GeForce RTX 2070                                 | 1F50           |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F50 1028 08ED |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F50 1028 08EE |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F50 1028 08EF |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F50 103C 8572 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F50 103C 8573 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F50 103C 8574 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F50 103C 8600 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F50 103C 8605 |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F50 17AA 3FEE |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F50 17AA 3FFE |
| NVIDIA GeForce RTX 2060                                 | 1F51           |
| NVIDIA GeForce RTX 2070                                 | 1F54           |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F54 103C 878A |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F54 17AA 3FCE |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F54 17AA 3FCF |
| NVIDIA GeForce RTX 2070 with Max-Q Design               | 1F54 17AA 3FD0 |
| NVIDIA GeForce RTX 2060                                 | 1F55           |
| Quadro RTX 3000                                         | 1F76           |
| Matrox D-Series D2450                                   | 1F76 102B 2800 |
| Matrox D-Series D2480                                   | 1F76 102B 2900 |
| NVIDIA GeForce GTX 1650                                 | 1F82           |
| NVIDIA GeForce GTX 1630                                 | 1F83           |
| NVIDIA GeForce GTX 1650                                 | 1F91           |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F91 103C 863E |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F91 103C 86E7 |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F91 103C 86E8 |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F91 1043 12CF |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F91 1043 156F |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F91 1414 0032 |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F91 144D C822 |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F91 1462 127E |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F91 1462 1281 |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F91 1462 1284 |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F91 1462 1285 |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F91 1462 129C |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F91 17AA 229F |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F91 17AA 3802 |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F91 17AA 3806 |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F91 17AA 3F1A |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F91 1A58 1001 |
| NVIDIA GeForce GTX 1650 Ti                              | 1F95           |
| NVIDIA GeForce GTX 1650 Ti with Max-Q Design            | 1F95 1025 1479 |
| NVIDIA GeForce GTX 1650 Ti with Max-Q Design            | 1F95 1025 147A |
| NVIDIA GeForce GTX 1650 Ti with Max-Q Design            | 1F95 1025 147B |
| NVIDIA GeForce GTX 1650 Ti with Max-Q Design            | 1F95 1025 147C |
| NVIDIA GeForce GTX 1650 Ti with Max-Q Design            | 1F95 103C 86E7 |
| NVIDIA GeForce GTX 1650 Ti with Max-Q Design            | 1F95 103C 86E8 |
| NVIDIA GeForce GTX 1650 Ti with Max-Q Design            | 1F95 103C 8815 |
| NVIDIA GeForce GTX 1650 Ti with Max-Q Design            | 1F95 1043 1DFF |
| NVIDIA GeForce GTX 1650 Ti with Max-Q Design            | 1F95 1043 1E1F |
| NVIDIA GeForce GTX 1650 Ti with Max-Q Design            | 1F95 144D C838 |
| NVIDIA GeForce GTX 1650 Ti with Max-Q Design            | 1F95 1462 12BD |
| NVIDIA GeForce GTX 1650 Ti with Max-Q Design            | 1F95 1462 12C5 |
| NVIDIA GeForce GTX 1650 Ti with Max-Q Design            | 1F95 1462 12D2 |
| NVIDIA GeForce GTX 1650 Ti with Max-Q Design            | 1F95 17AA 22C0 |
| NVIDIA GeForce GTX 1650 Ti with Max-Q Design            | 1F95 17AA 22C1 |
| NVIDIA GeForce GTX 1650 Ti with Max-Q Design            | 1F95 17AA 3837 |
| NVIDIA GeForce GTX 1650 Ti with Max-Q Design            | 1F95 17AA 3F95 |
| NVIDIA GeForce GTX 1650 Ti with Max-Q Design            | 1F95 1A58 1003 |
| NVIDIA GeForce GTX 1650 Ti with Max-Q Design            | 1F95 1A58 1006 |
| NVIDIA GeForce GTX 1650 Ti with Max-Q Design            | 1F95 1A58 1007 |
| NVIDIA GeForce GTX 1650 Ti with Max-Q Design            | 1F95 1E83 3E30 |
| NVIDIA GeForce GTX 1650                                 | 1F96           |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F96 1462 1297 |
| NVIDIA GeForce MX450                                    | 1F97           |
| NVIDIA GeForce MX450                                    | 1F98           |
| NVIDIA GeForce GTX 1650                                 | 1F99           |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F99 1025 1479 |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F99 1025 147A |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F99 1025 147B |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F99 1025 147C |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F99 103C 8815 |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F99 1043 13B2 |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F99 1043 1402 |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F99 1043 1902 |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F99 1462 12BD |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F99 1462 12C5 |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F99 1462 12D2 |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F99 17AA 22DA |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F99 17AA 3F93 |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F99 1E83 3E30 |
| NVIDIA GeForce MX450                                    | 1F9C           |
| NVIDIA GeForce GTX 1650                                 | 1F9D           |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F9D 1043 128D |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F9D 1043 130D |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F9D 1043 149C |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F9D 1043 185C |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F9D 1043 189C |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F9D 1462 12F4 |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F9D 1462 1302 |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F9D 1462 131B |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F9D 1462 1326 |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F9D 1462 132A |
| NVIDIA GeForce GTX 1650 with Max-Q Design               | 1F9D 1462 132E |
| NVIDIA GeForce MX550                                    | 1F9F           |
| NVIDIA GeForce MX550                                    | 1FA0           |
| NVIDIA T1000                                            | 1FB0 1028 12DB |
| NVIDIA T1000                                            | 1FB0 103C 12DB |
| NVIDIA T1000                                            | 1FB0 103C 8A80 |
| NVIDIA T1000                                            | 1FB0 10DE 12DB |
| NVIDIA DGX Display                                      | 1FB0 10DE 1485 |
| NVIDIA T1000                                            | 1FB0 17AA 12DB |
| NVIDIA T600                                             | 1FB1 1028 1488 |
| NVIDIA T600                                             | 1FB1 103C 1488 |
| NVIDIA T600                                             | 1FB1 103C 8A80 |
| NVIDIA T600                                             | 1FB1 10DE 1488 |
| NVIDIA T600                                             | 1FB1 17AA 1488 |
| NVIDIA T400                                             | 1FB2 1028 1489 |
| NVIDIA T400                                             | 1FB2 103C 1489 |
| NVIDIA T400                                             | 1FB2 103C 8A80 |
| NVIDIA T400                                             | 1FB2 10DE 1489 |
| NVIDIA T400                                             | 1FB2 17AA 1489 |
| NVIDIA T600 Laptop GPU                                  | 1FB6           |
| NVIDIA T550 Laptop GPU                                  | 1FB7           |
| Quadro T2000                                            | 1FB8           |
| Quadro T2000 with Max-Q Design                          | 1FB8 1028 097E |
| Quadro T2000 with Max-Q Design                          | 1FB8 103C 8736 |
| Quadro T2000 with Max-Q Design                          | 1FB8 103C 8738 |
| Quadro T2000 with Max-Q Design                          | 1FB8 103C 8772 |
| Quadro T2000 with Max-Q Design                          | 1FB8 103C 8780 |
| Quadro T2000 with Max-Q Design                          | 1FB8 103C 8782 |
| Quadro T2000 with Max-Q Design                          | 1FB8 103C 8783 |
| Quadro T2000 with Max-Q Design                          | 1FB8 103C 8785 |
| Quadro T2000 with Max-Q Design                          | 1FB8 103C 87F0 |
| Quadro T2000 with Max-Q Design                          | 1FB8 1462 1281 |
| Quadro T2000 with Max-Q Design                          | 1FB8 1462 12BD |
| Quadro T2000 with Max-Q Design                          | 1FB8 17AA 22C0 |
| Quadro T2000 with Max-Q Design                          | 1FB8 17AA 22C1 |
| Quadro T1000                                            | 1FB9           |
| Quadro T1000 with Max-Q Design                          | 1FB9 1025 1479 |
| Quadro T1000 with Max-Q Design                          | 1FB9 1025 147A |
| Quadro T1000 with Max-Q Design                          | 1FB9 1025 147B |
| Quadro T1000 with Max-Q Design                          | 1FB9 1025 147C |
| Quadro T1000 with Max-Q Design                          | 1FB9 103C 8736 |
| Quadro T1000 with Max-Q Design                          | 1FB9 103C 8738 |
| Quadro T1000 with Max-Q Design                          | 1FB9 103C 8772 |
| Quadro T1000 with Max-Q Design                          | 1FB9 103C 8780 |
| Quadro T1000 with Max-Q Design                          | 1FB9 103C 8782 |
| Quadro T1000 with Max-Q Design                          | 1FB9 103C 8783 |
| Quadro T1000 with Max-Q Design                          | 1FB9 103C 8785 |
| Quadro T1000 with Max-Q Design                          | 1FB9 103C 87F0 |
| Quadro T1000 with Max-Q Design                          | 1FB9 1462 12BD |
| Quadro T1000 with Max-Q Design                          | 1FB9 17AA 22C0 |
| Quadro T1000 with Max-Q Design                          | 1FB9 17AA 22C1 |
| NVIDIA T600 Laptop GPU                                  | 1FBA           |
| NVIDIA T500                                             | 1FBB           |
| NVIDIA T1200 Laptop GPU                                 | 1FBC           |
| NVIDIA GeForce GTX 1650                                 | 1FDD           |
| NVIDIA T1000 8GB                                        | 1FF0 1028 1612 |
| NVIDIA T1000 8GB                                        | 1FF0 103C 1612 |
| NVIDIA T1000 8GB                                        | 1FF0 103C 8A80 |
| NVIDIA T1000 8GB                                        | 1FF0 10DE 1612 |
| NVIDIA T1000 8GB                                        | 1FF0 17AA 1612 |
| NVIDIA T400 4GB                                         | 1FF2 1028 1613 |
| NVIDIA T400 4GB                                         | 1FF2 103C 1613 |
| NVIDIA T400E                                            | 1FF2 103C 18FF |
| NVIDIA T400 4GB                                         | 1FF2 103C 8A80 |
| NVIDIA T400 4GB                                         | 1FF2 10DE 1613 |
| NVIDIA T400E                                            | 1FF2 10DE 18FF |
| NVIDIA T400 4GB                                         | 1FF2 17AA 1613 |
| NVIDIA T400E                                            | 1FF2 17AA 18FF |
| Quadro T1000                                            | 1FF9           |
| NVIDIA A100-SXM4-40GB                                   | 20B0           |
| NVIDIA A100-PG509-200                                   | 20B0 10DE 1450 |
| NVIDIA A100-SXM4-80GB                                   | 20B2 10DE 1463 |
| NVIDIA A100-SXM4-80GB                                   | 20B2 10DE 147F |
| NVIDIA A100-SXM4-80GB                                   | 20B2 10DE 1622 |
| NVIDIA A100-SXM4-80GB                                   | 20B2 10DE 1623 |
| NVIDIA PG509-210                                        | 20B2 10DE 1625 |
| NVIDIA A100-SXM-64GB                                    | 20B3 10DE 14A7 |
| NVIDIA A100-SXM-64GB                                    | 20B3 10DE 14A8 |
| NVIDIA A100 80GB PCIe                                   | 20B5 10DE 1533 |
| NVIDIA A100 80GB PCIe                                   | 20B5 10DE 1642 |
| NVIDIA PG506-232                                        | 20B6 10DE 1492 |
| NVIDIA A30                                              | 20B7 10DE 1532 |
| NVIDIA A30                                              | 20B7 10DE 1804 |
| NVIDIA A30                                              | 20B7 10DE 1852 |
| NVIDIA A800-SXM4-40GB                                   | 20BD 10DE 17F4 |
| NVIDIA A100-PCIE-40GB                                   | 20F1 10DE 145F |
| NVIDIA A800-SXM4-80GB                                   | 20F3 10DE 179B |
| NVIDIA A800-SXM4-80GB                                   | 20F3 10DE 179C |
| NVIDIA A800-SXM4-80GB                                   | 20F3 10DE 179D |
| NVIDIA A800-SXM4-80GB                                   | 20F3 10DE 179E |
| NVIDIA A800-SXM4-80GB                                   | 20F3 10DE 179F |
| NVIDIA A800-SXM4-80GB                                   | 20F3 10DE 17A0 |
| NVIDIA A800-SXM4-80GB                                   | 20F3 10DE 17A1 |
| NVIDIA A800-SXM4-80GB                                   | 20F3 10DE 17A2 |
| NVIDIA A800 80GB PCIe                                   | 20F5 10DE 1799 |
| NVIDIA A800 80GB PCIe LC                                | 20F5 10DE 179A |
| NVIDIA A800 40GB Active                                 | 20F6 1028 180A |
| NVIDIA A800 40GB Active                                 | 20F6 103C 180A |
| NVIDIA A800 40GB Active                                 | 20F6 10DE 180A |
| NVIDIA A800 40GB Active                                 | 20F6 17AA 180A |
| NVIDIA AX800                                            | 20FD 10DE 17F8 |
| NVIDIA GeForce GTX 1660 Ti                              | 2182           |
| NVIDIA GeForce GTX 1660                                 | 2184           |
| NVIDIA GeForce GTX 1650 SUPER                           | 2187           |
| NVIDIA GeForce GTX 1650                                 | 2188           |
| NVIDIA CMP 30HX                                         | 2189           |
| NVIDIA GeForce GTX 1660 Ti                              | 2191           |
| NVIDIA GeForce GTX 1660 Ti with Max-Q Design            | 2191 1028 0949 |
| NVIDIA GeForce GTX 1660 Ti with Max-Q Design            | 2191 103C 85FB |
| NVIDIA GeForce GTX 1660 Ti with Max-Q Design            | 2191 103C 85FE |
| NVIDIA GeForce GTX 1660 Ti with Max-Q Design            | 2191 103C 86D6 |
| NVIDIA GeForce GTX 1660 Ti with Max-Q Design            | 2191 103C 8741 |
| NVIDIA GeForce GTX 1660 Ti with Max-Q Design            | 2191 103C 8744 |
| NVIDIA GeForce GTX 1660 Ti with Max-Q Design            | 2191 103C 878D |
| NVIDIA GeForce GTX 1660 Ti with Max-Q Design            | 2191 103C 87AF |
| NVIDIA GeForce GTX 1660 Ti with Max-Q Design            | 2191 103C 87B3 |
| NVIDIA GeForce GTX 1660 Ti with Max-Q Design            | 2191 1043 171F |
| NVIDIA GeForce GTX 1660 Ti with Max-Q Design            | 2191 1043 17EF |
| NVIDIA GeForce GTX 1660 Ti with Max-Q Design            | 2191 1043 18D1 |
| NVIDIA GeForce GTX 1660 Ti with Max-Q Design            | 2191 1414 0032 |
| NVIDIA GeForce GTX 1660 Ti with Max-Q Design            | 2191 1462 128A |
| NVIDIA GeForce GTX 1660 Ti with Max-Q Design            | 2191 1462 128B |
| NVIDIA GeForce GTX 1660 Ti with Max-Q Design            | 2191 1462 12C6 |
| NVIDIA GeForce GTX 1660 Ti with Max-Q Design            | 2191 1462 12CB |
| NVIDIA GeForce GTX 1660 Ti with Max-Q Design            | 2191 1462 12CC |
| NVIDIA GeForce GTX 1660 Ti with Max-Q Design            | 2191 1462 12D9 |
| NVIDIA GeForce GTX 1660 Ti with Max-Q Design            | 2191 17AA 380C |
| NVIDIA GeForce GTX 1660 Ti with Max-Q Design            | 2191 17AA 381D |
| NVIDIA GeForce GTX 1660 Ti with Max-Q Design            | 2191 17AA 381E |
| NVIDIA GeForce GTX 1650 Ti                              | 2192           |
| NVIDIA GeForce GTX 1660 SUPER                           | 21C4           |
| NVIDIA GeForce GTX 1660 Ti                              | 21D1           |
| NVIDIA GeForce RTX 3090 Ti                              | 2203           |
| NVIDIA GeForce RTX 3090                                 | 2204           |
| NVIDIA GeForce RTX 3080                                 | 2206           |
| NVIDIA GeForce RTX 3070 Ti                              | 2207           |
| NVIDIA GeForce RTX 3080 Ti                              | 2208           |
| NVIDIA GeForce RTX 3080                                 | 220A           |
| NVIDIA CMP 90HX                                         | 220D           |
| NVIDIA GeForce RTX 3080                                 | 2216           |
| NVIDIA RTX A6000                                        | 2230 1028 1459 |
| NVIDIA RTX A6000                                        | 2230 103C 1459 |
| NVIDIA RTX A6000                                        | 2230 10DE 1459 |
| NVIDIA RTX A6000                                        | 2230 17AA 1459 |
| NVIDIA RTX A5000                                        | 2231 1028 147E |
| NVIDIA RTX A5000                                        | 2231 103C 147E |
| NVIDIA RTX A5000                                        | 2231 10DE 147E |
| NVIDIA RTX A5000                                        | 2231 17AA 147E |
| NVIDIA RTX A4500                                        | 2232 1028 163C |
| NVIDIA RTX A4500                                        | 2232 103C 163C |
| NVIDIA RTX A4500                                        | 2232 10DE 163C |
| NVIDIA RTX A4500                                        | 2232 17AA 163C |
| NVIDIA RTX A5500                                        | 2233 1028 165A |
| NVIDIA RTX A5500                                        | 2233 103C 165A |
| NVIDIA RTX A5500                                        | 2233 10DE 165A |
| NVIDIA RTX A5500                                        | 2233 17AA 165A |
| NVIDIA A40                                              | 2235 10DE 145A |
| NVIDIA A10                                              | 2236 10DE 1482 |
| NVIDIA A10G                                             | 2237 10DE 152F |
| NVIDIA A10M                                             | 2238 10DE 1677 |
| NVIDIA H20 NVL16                                        | 230E 10DE 20DF |
| NVIDIA H100 NVL                                         | 2321 10DE 1839 |
| NVIDIA H800 PCIe                                        | 2322 10DE 17A4 |
| NVIDIA H800                                             | 2324 10DE 17A6 |
| NVIDIA H800                                             | 2324 10DE 17A8 |
| NVIDIA H20                                              | 2329 10DE 198B |
| NVIDIA H20                                              | 2329 10DE 198C |
| NVIDIA H20-3e                                           | 232C 10DE 2063 |
| NVIDIA H100 80GB HBM3                                   | 2330 10DE 16C0 |
| NVIDIA H100 80GB HBM3                                   | 2330 10DE 16C1 |
| NVIDIA H100 PCIe                                        | 2331 10DE 1626 |
| NVIDIA H200                                             | 2335 10DE 18BE |
| NVIDIA H200                                             | 2335 10DE 18BF |
| NVIDIA H100                                             | 2339 10DE 17FC |
| NVIDIA H800 NVL                                         | 233A 10DE 183A |
| NVIDIA H200 NVL                                         | 233B 10DE 1996 |
| NVIDIA GH200 120GB                                      | 2342 10DE 16EB |
| NVIDIA GH200 120GB                                      | 2342 10DE 1805 |
| NVIDIA GH200 480GB                                      | 2342 10DE 1809 |
| NVIDIA GH200 144G HBM3e                                 | 2348 10DE 18D2 |
| NVIDIA GeForce RTX 3060 Ti                              | 2414           |
| NVIDIA GeForce RTX 3080 Ti Laptop GPU                   | 2420           |
| NVIDIA RTX A5500 Laptop GPU                             | 2438           |
| NVIDIA GeForce RTX 3080 Ti Laptop GPU                   | 2460           |
| NVIDIA GeForce RTX 3070 Ti                              | 2482           |
| NVIDIA GeForce RTX 3070                                 | 2484           |
| NVIDIA GeForce RTX 3060 Ti                              | 2486           |
| NVIDIA GeForce RTX 3060                                 | 2487           |
| NVIDIA GeForce RTX 3070                                 | 2488           |
| NVIDIA GeForce RTX 3060 Ti                              | 2489           |
| NVIDIA CMP 70HX                                         | 248A           |
| NVIDIA GeForce RTX 3080 Laptop GPU                      | 249C           |
| NVIDIA GeForce RTX 3060 Laptop GPU                      | 249C 1D05 1194 |
| NVIDIA GeForce RTX 3070 Laptop GPU                      | 249D           |
| NVIDIA GeForce RTX 3070 Ti Laptop GPU                   | 24A0           |
| NVIDIA GeForce RTX 3060 Laptop GPU                      | 24A0 1D05 1192 |
| NVIDIA RTX A4000                                        | 24B0 1028 14AD |
| NVIDIA RTX A4000                                        | 24B0 103C 14AD |
| NVIDIA RTX A4000                                        | 24B0 10DE 14AD |
| NVIDIA RTX A4000                                        | 24B0 17AA 14AD |
| NVIDIA RTX A4000H                                       | 24B1 10DE 1658 |
| NVIDIA RTX A5000 Laptop GPU                             | 24B6           |
| NVIDIA RTX A4000 Laptop GPU                             | 24B7           |
| NVIDIA RTX A3000 Laptop GPU                             | 24B8           |
| NVIDIA RTX A3000 12GB Laptop GPU                        | 24B9           |
| NVIDIA RTX A4500 Laptop GPU                             | 24BA           |
| NVIDIA RTX A3000 12GB Laptop GPU                        | 24BB           |
| NVIDIA GeForce RTX 3060                                 | 24C7           |
| NVIDIA GeForce RTX 3060 Ti                              | 24C9           |
| NVIDIA GeForce RTX 3080 Laptop GPU                      | 24DC           |
| NVIDIA GeForce RTX 3070 Laptop GPU                      | 24DD           |
| NVIDIA GeForce RTX 3070 Ti Laptop GPU                   | 24E0           |
| NVIDIA RTX A4500 Embedded GPU                           | 24FA           |
| NVIDIA GeForce RTX 3060                                 | 2503           |
| NVIDIA GeForce RTX 3060                                 | 2504           |
| NVIDIA GeForce RTX 3050                                 | 2507           |
| NVIDIA GeForce RTX 3050 OEM                             | 2508           |
| NVIDIA GeForce RTX 3060 Laptop GPU                      | 2520           |
| NVIDIA GeForce RTX 3060 Laptop GPU                      | 2521           |
| NVIDIA GeForce RTX 3050 Ti Laptop GPU                   | 2523           |
| NVIDIA RTX A2000                                        | 2531 1028 151D |
| NVIDIA RTX A2000                                        | 2531 103C 151D |
| NVIDIA RTX A2000                                        | 2531 10DE 151D |
| NVIDIA RTX A2000                                        | 2531 17AA 151D |
| NVIDIA GeForce RTX 3060                                 | 2544           |
| NVIDIA GeForce RTX 3060 Laptop GPU                      | 2560           |
| NVIDIA GeForce RTX 3050 Ti Laptop GPU                   | 2563           |
| NVIDIA RTX A2000 12GB                                   | 2571 1028 1611 |
| NVIDIA RTX A2000 12GB                                   | 2571 103C 1611 |
| NVIDIA RTX A2000 12GB                                   | 2571 10DE 1611 |
| NVIDIA RTX A2000 12GB                                   | 2571 17AA 1611 |
| NVIDIA GeForce RTX 3050                                 | 2582           |
| NVIDIA GeForce RTX 3050                                 | 2584           |
| NVIDIA GeForce RTX 3050 Ti Laptop GPU                   | 25A0           |
| NVIDIA GeForce RTX 3050Ti Laptop GPU                    | 25A0 103C 8928 |
| NVIDIA GeForce RTX 3050Ti Laptop GPU                    | 25A0 103C 89F9 |
| NVIDIA GeForce RTX 3060 Laptop GPU                      | 25A0 1D05 1196 |
| NVIDIA GeForce RTX 3050 Laptop GPU                      | 25A2           |
| NVIDIA GeForce RTX 3050 Ti Laptop GPU                   | 25A2 1028 0BAF |
| NVIDIA GeForce RTX 3060 Laptop GPU                      | 25A2 1D05 1195 |
| NVIDIA GeForce RTX 3050 Laptop GPU                      | 25A5           |
| NVIDIA GeForce MX570                                    | 25A6           |
| NVIDIA GeForce RTX 2050                                 | 25A7           |
| NVIDIA GeForce RTX 2050                                 | 25A9           |
| NVIDIA GeForce MX570 A                                  | 25AA           |
| NVIDIA GeForce RTX 3050 4GB Laptop GPU                  | 25AB           |
| NVIDIA GeForce RTX 3050 6GB Laptop GPU                  | 25AC           |
| NVIDIA GeForce RTX 2050                                 | 25AD           |
| NVIDIA RTX A1000                                        | 25B0 1028 1878 |
| NVIDIA RTX A1000                                        | 25B0 103C 1878 |
| NVIDIA RTX A1000                                        | 25B0 103C 8D96 |
| NVIDIA RTX A1000                                        | 25B0 10DE 1878 |
| NVIDIA RTX A1000                                        | 25B0 17AA 1878 |
| NVIDIA RTX A400                                         | 25B2 1028 1879 |
| NVIDIA RTX A400                                         | 25B2 103C 1879 |
| NVIDIA RTX A400                                         | 25B2 103C 8D95 |
| NVIDIA RTX A400                                         | 25B2 103C 8F5B |
| NVIDIA RTX A400                                         | 25B2 10DE 1879 |
| NVIDIA RTX A400                                         | 25B2 17AA 1879 |
| NVIDIA A16                                              | 25B6 10DE 14A9 |
| NVIDIA A2                                               | 25B6 10DE 157E |
| NVIDIA RTX A2000 Laptop GPU                             | 25B8           |
| NVIDIA RTX A1000 Laptop GPU                             | 25B9           |
| NVIDIA RTX A2000 8GB Laptop GPU                         | 25BA           |
| NVIDIA RTX A500 Laptop GPU                              | 25BB           |
| NVIDIA RTX A1000 6GB Laptop GPU                         | 25BC           |
| NVIDIA RTX A500 Laptop GPU                              | 25BD           |
| NVIDIA GeForce RTX 3050 Ti Laptop GPU                   | 25E0           |
| NVIDIA GeForce RTX 3050 Laptop GPU                      | 25E2           |
| NVIDIA GeForce RTX 3050 Laptop GPU                      | 25E5           |
| NVIDIA GeForce RTX 3050 6GB Laptop GPU                  | 25EC           |
| NVIDIA GeForce RTX 2050                                 | 25ED           |
| NVIDIA RTX A1000 Embedded GPU                           | 25F9           |
| NVIDIA RTX A2000 Embedded GPU                           | 25FA           |
| NVIDIA RTX A500 Embedded GPU                            | 25FB           |
| NVIDIA GeForce RTX 4090                                 | 2684           |
| NVIDIA GeForce RTX 4090 D                               | 2685           |
| NVIDIA GeForce RTX 4070 Ti SUPER                        | 2689           |
| NVIDIA RTX 6000 Ada Generation                          | 26B1 1028 16A1 |
| NVIDIA RTX 6000 Ada Generation                          | 26B1 103C 16A1 |
| NVIDIA RTX 6000 Ada Generation                          | 26B1 10DE 16A1 |
| NVIDIA RTX 6000 Ada Generation                          | 26B1 17AA 16A1 |
| NVIDIA RTX 5000 Ada Generation                          | 26B2 1028 17FA |
| NVIDIA RTX 5000 Ada Generation                          | 26B2 103C 17FA |
| NVIDIA RTX 5000 Ada Generation                          | 26B2 10DE 17FA |
| NVIDIA RTX 5000 Ada Generation                          | 26B2 17AA 17FA |
| NVIDIA RTX 5880 Ada Generation                          | 26B3 1028 1934 |
| NVIDIA RTX 5880 Ada Generation                          | 26B3 103C 1934 |
| NVIDIA RTX 5880 Ada Generation                          | 26B3 10DE 1934 |
| NVIDIA RTX 5880 Ada Generation                          | 26B3 17AA 1934 |
| NVIDIA L40                                              | 26B5 10DE 169D |
| NVIDIA L40                                              | 26B5 10DE 17DA |
| NVIDIA L40S                                             | 26B9 10DE 1851 |
| NVIDIA L40S                                             | 26B9 10DE 18CF |
| NVIDIA L20                                              | 26BA 10DE 1957 |
| NVIDIA L20                                              | 26BA 10DE 1990 |
| NVIDIA GeForce RTX 4080 SUPER                           | 2702           |
| NVIDIA GeForce RTX 4080                                 | 2704           |
| NVIDIA GeForce RTX 4070 Ti SUPER                        | 2705           |
| NVIDIA GeForce RTX 4070                                 | 2709           |
| NVIDIA GeForce RTX 4090 Laptop GPU                      | 2717           |
| NVIDIA RTX 5000 Ada Generation Laptop GPU               | 2730           |
| NVIDIA GeForce RTX 4090 Laptop GPU                      | 2757           |
| NVIDIA RTX 5000 Ada Generation Embedded GPU             | 2770           |
| NVIDIA GeForce RTX 4070 Ti                              | 2782           |
| NVIDIA GeForce RTX 4070 SUPER                           | 2783           |
| NVIDIA GeForce RTX 4070                                 | 2786           |
| NVIDIA GeForce RTX 4060 Ti                              | 2788           |
| NVIDIA GeForce RTX 4080 Laptop GPU                      | 27A0           |
| NVIDIA RTX 4000 SFF Ada Generation                      | 27B0 1028 16FA |
| NVIDIA RTX 4000 SFF Ada Generation                      | 27B0 103C 16FA |
| NVIDIA RTX 4000 SFF Ada Generation                      | 27B0 10DE 16FA |
| NVIDIA RTX 4000 SFF Ada Generation                      | 27B0 17AA 16FA |
| NVIDIA RTX 4500 Ada Generation                          | 27B1 1028 180C |
| NVIDIA RTX 4500 Ada Generation                          | 27B1 103C 180C |
| NVIDIA RTX 4500 Ada Generation                          | 27B1 10DE 180C |
| NVIDIA RTX 4500 Ada Generation                          | 27B1 17AA 180C |
| NVIDIA RTX 4000 Ada Generation                          | 27B2 1028 181B |
| NVIDIA RTX 4000 Ada Generation                          | 27B2 103C 181B |
| NVIDIA RTX 4000 Ada Generation                          | 27B2 10DE 181B |
| NVIDIA RTX 4000 Ada Generation                          | 27B2 17AA 181B |
| NVIDIA L2                                               | 27B6 10DE 1933 |
| NVIDIA L4                                               | 27B8 10DE 16CA |
| NVIDIA L4                                               | 27B8 10DE 16EE |
| NVIDIA RTX 4000 Ada Generation Laptop GPU               | 27BA           |
| NVIDIA RTX 3500 Ada Generation Laptop GPU               | 27BB           |
| NVIDIA GeForce RTX 4080 Laptop GPU                      | 27E0           |
| NVIDIA RTX 3500 Ada Generation Embedded GPU             | 27FB           |
| NVIDIA GeForce RTX 4060 Ti                              | 2803           |
| NVIDIA GeForce RTX 4060 Ti                              | 2805           |
| NVIDIA GeForce RTX 4060                                 | 2808           |
| NVIDIA GeForce RTX 4070 Laptop GPU                      | 2820           |
| NVIDIA GeForce RTX 3050 A Laptop GPU                    | 2822           |
| NVIDIA RTX 3000 Ada Generation Laptop GPU               | 2838           |
| NVIDIA GeForce RTX 4070 Laptop GPU                      | 2860           |
| NVIDIA GeForce RTX 4060                                 | 2882           |
| NVIDIA GeForce RTX 4060 Laptop GPU                      | 28A0           |
| NVIDIA GeForce RTX 4050 Laptop GPU                      | 28A1           |
| NVIDIA GeForce RTX 3050 A Laptop GPU                    | 28A3           |
| NVIDIA RTX 2000 Ada Generation                          | 28B0 1028 1870 |
| NVIDIA RTX 2000 Ada Generation                          | 28B0 103C 1870 |
| NVIDIA RTX 2000E Ada Generation                         | 28B0 103C 1871 |
| NVIDIA RTX 2000 Ada Generation                          | 28B0 10DE 1870 |
| NVIDIA RTX 2000E Ada Generation                         | 28B0 10DE 1871 |
| NVIDIA RTX 2000 Ada Generation                          | 28B0 17AA 1870 |
| NVIDIA RTX 2000E Ada Generation                         | 28B0 17AA 1871 |
| NVIDIA RTX 2000 Ada Generation Laptop GPU               | 28B8           |
| NVIDIA RTX 1000 Ada Generation Laptop GPU               | 28B9           |
| NVIDIA RTX 500 Ada Generation Laptop GPU                | 28BA           |
| NVIDIA RTX 500 Ada Generation Laptop GPU                | 28BB           |
| NVIDIA GeForce RTX 4060 Laptop GPU                      | 28E0           |
| NVIDIA GeForce RTX 4050 Laptop GPU                      | 28E1           |
| NVIDIA GeForce RTX 3050 A Laptop GPU                    | 28E3           |
| NVIDIA RTX 2000 Ada Generation Embedded GPU             | 28F8           |
| NVIDIA B200                                             | 2901 10DE 1999 |
| NVIDIA B200                                             | 2901 10DE 199B |
| NVIDIA B200                                             | 2901 10DE 20DA |
| NVIDIA B200                                             | 2909 10DE 22EB |
| NVIDIA GB200                                            | 2941 10DE 2046 |
| NVIDIA GB200                                            | 2941 10DE 20CA |
| NVIDIA GB200                                            | 2941 10DE 20D5 |
| NVIDIA GB200                                            | 2941 10DE 21C9 |
| NVIDIA GB200                                            | 2941 10DE 21CA |
| NVIDIA DRIVE P2021                                      | 29BB 10DE 207C |
| NVIDIA GeForce RTX 5090                                 | 2B85           |
| NVIDIA GeForce RTX 5090 D                               | 2B87           |
| NVIDIA GeForce RTX 5090 D v2                            | 2B8C           |
| NVIDIA RTX PRO 6000 Blackwell Workstation Edition       | 2BB1 1028 204B |
| NVIDIA RTX PRO 6000 Blackwell Workstation Edition       | 2BB1 103C 204B |
| NVIDIA RTX PRO 6000 Blackwell Workstation Edition       | 2BB1 10DE 204B |
| NVIDIA RTX PRO 6000 Blackwell Workstation Edition       | 2BB1 17AA 204B |
| NVIDIA RTX PRO 5000 Blackwell                           | 2BB3 1028 204D |
| NVIDIA RTX PRO 5000 72GB Blackwell                      | 2BB3 1028 227A |
| NVIDIA RTX PRO 5000 Blackwell                           | 2BB3 103C 204D |
| NVIDIA RTX PRO 5000 72GB Blackwell                      | 2BB3 103C 227A |
| NVIDIA RTX PRO 5000 Blackwell                           | 2BB3 10DE 204D |
| NVIDIA RTX PRO 5000 72GB Blackwell                      | 2BB3 10DE 227A |
| NVIDIA RTX PRO 5000 Blackwell                           | 2BB3 17AA 204D |
| NVIDIA RTX PRO 5000 72GB Blackwell                      | 2BB3 17AA 227A |
| NVIDIA RTX PRO 6000 Blackwell Max-Q Workstation Edition | 2BB4 1028 204C |
| NVIDIA RTX PRO 6000 Blackwell Max-Q Workstation Edition | 2BB4 103C 204C |
| NVIDIA RTX PRO 6000 Blackwell Max-Q Workstation Edition | 2BB4 10DE 204C |
| NVIDIA RTX PRO 6000 Blackwell Max-Q Workstation Edition | 2BB4 17AA 204C |
| NVIDIA RTX PRO 6000 Blackwell Server Edition            | 2BB5 10DE 204E |
| NVIDIA RTX PRO 6000 Blackwell Server Edition            | 2BB5 10DE 220B |
| NVIDIA RTX 6000D                                        | 2BB9 10DE 2091 |
| NVIDIA RTX 6000D                                        | 2BB9 10DE 2092 |
| NVIDIA RTX 6000D                                        | 2BB9 10DE 2279 |
| NVIDIA GeForce RTX 5080                                 | 2C02           |
| NVIDIA GeForce RTX 5070 Ti                              | 2C05           |
| NVIDIA GeForce RTX 5090 Laptop GPU                      | 2C18           |
| NVIDIA GeForce RTX 5080 Laptop GPU                      | 2C19           |
| NVIDIA RTX PRO 4500 Blackwell                           | 2C31 1028 2051 |
| NVIDIA RTX PRO 4500 Blackwell                           | 2C31 103C 2051 |
| NVIDIA RTX PRO 4500 Blackwell                           | 2C31 10DE 2051 |
| NVIDIA RTX PRO 4500 Blackwell                           | 2C31 17AA 2051 |
| NVIDIA RTX PRO 4000 Blackwell SFF Edition               | 2C33 1028 2053 |
| NVIDIA RTX PRO 4000 Blackwell SFF Edition               | 2C33 103C 2053 |
| NVIDIA RTX PRO 4000 Blackwell SFF Edition               | 2C33 10DE 2053 |
| NVIDIA RTX PRO 4000 Blackwell SFF Edition               | 2C33 17AA 2053 |
| NVIDIA RTX PRO 4000 Blackwell                           | 2C34 1028 2052 |
| NVIDIA RTX PRO 4000 Blackwell                           | 2C34 103C 2052 |
| NVIDIA RTX PRO 4000 Blackwell                           | 2C34 10DE 2052 |
| NVIDIA RTX PRO 4000 Blackwell                           | 2C34 17AA 2052 |
| NVIDIA RTX PRO 5000 Blackwell Generation Laptop GPU     | 2C38           |
| NVIDIA RTX PRO 4000 Blackwell Generation Laptop GPU     | 2C39           |
| NVIDIA RTX PRO 4500 Blackwell Server Edition            | 2C3A 10DE 21F4 |
| NVIDIA GeForce RTX 5090 Laptop GPU                      | 2C58           |
| NVIDIA GeForce RTX 5080 Laptop GPU                      | 2C59           |
| NVIDIA RTX PRO 5000 Blackwell Embedded GPU              | 2C77           |
| NVIDIA RTX PRO 4000 Blackwell Embedded GPU              | 2C79           |
| NVIDIA GeForce RTX 5060 Ti                              | 2D04           |
| NVIDIA GeForce RTX 5060                                 | 2D05           |
| NVIDIA GeForce RTX 5070 Laptop GPU                      | 2D18           |
| NVIDIA GeForce RTX 5060 Laptop GPU                      | 2D19           |
| NVIDIA RTX PRO 2000 Blackwell                           | 2D30 1028 2054 |
| NVIDIA RTX PRO 2000 Blackwell                           | 2D30 103C 2054 |
| NVIDIA RTX PRO 2000 Blackwell                           | 2D30 10DE 2054 |
| NVIDIA RTX PRO 2000 Blackwell                           | 2D30 17AA 2054 |
| NVIDIA RTX PRO 2000 Blackwell Generation Laptop GPU     | 2D39           |
| NVIDIA GeForce RTX 5070 Laptop GPU                      | 2D58           |
| NVIDIA GeForce RTX 5060 Laptop GPU                      | 2D59           |
| NVIDIA RTX PRO 2000 Blackwell Embedded GPU              | 2D79           |
| NVIDIA GeForce RTX 5050                                 | 2D83           |
| NVIDIA GeForce RTX 5050 Laptop GPU                      | 2D98           |
| NVIDIA RTX PRO 1000 Blackwell Generation Laptop GPU     | 2DB8           |
| NVIDIA RTX PRO 500 Blackwell Generation Laptop GPU      | 2DB9           |
| NVIDIA GeForce RTX 5050 Laptop GPU                      | 2DD8           |
| NVIDIA RTX PRO 500 Blackwell Embedded GPU               | 2DF9           |
| NVIDIA GB10                                             | 2E12 10DE 21EC |
| NVIDIA GeForce RTX 5070                                 | 2F04           |
| NVIDIA GeForce RTX 5070 Ti Laptop GPU                   | 2F18           |
| NVIDIA RTX PRO 3000 Blackwell Generation Laptop GPU     | 2F38           |
| NVIDIA GeForce RTX 5070 Ti Laptop GPU                   | 2F58           |
| NVIDIA B300 SXM6 AC                                     | 3182 10DE 20E6 |
| NVIDIA GB300                                            | 31C2 10DE 21F1 |
| NVIDIA GB300                                            | 31C3 10DE 22F8 |
