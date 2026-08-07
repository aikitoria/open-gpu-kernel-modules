# Same-host GPUDirect RDMA transport validation — 2026-08-06

## Revisions

- Branch: `feature/consumer-geforce-dmabuf-gdr`
- Validated module tree: hardware-tested provenance commit `12f9bb93`
- Driver/kernel: `610.43.03` / `7.0.0-29-generic`
- Secure Boot signer: local Secure Boot Module Signature key (enrolled MOK)
- CUDA userspace: private one-byte-patched `libcuda.so.610.43.03`

## System identity

| Component | Detail |
|---|---|
| GPU 0 | NVIDIA GeForce RTX 5060 Ti, PCI `0000:41:00.0` |
| GPU 1 | NVIDIA GeForce RTX 5060 Ti, PCI `0000:42:00.0` |
| NIC | NVIDIA ConnectX-6 Lx dual-port 25 GbE RoCE (MT2894), PCI `0000:62:00.0`/`.1`, firmware `26.49.1014` |
| RDMA devices | `mlx5_0` (port state Active), `mlx5_1` (port state Active) |
| `nvidia.ko` | `85879824254ecad9febdd0b6cc7944c9b21fe4ca310d3d91eb3ae9dbff147154`, srcversion `97587514A0900FB6CC5FF86` |
| Stock `libcuda` | SHA-256 `ba35b4baccf427f74f1b7c600297ae0cd4ff860f381aba65c3d0b88b8c5e95bc` |
| Patched `libcuda` | SHA-256 `f013ffac50fd6d9bf4d82142164889a81cc0bf4cde0741e0167826a1f3232c7a` |

The validated module tree contained the FORCE_PCIE physical-address locking
change represented by this PR series (the "Serialize FORCE_PCIE DMA-BUF
physical-address operations" commit). The subsequent coherent-only ForceSPA
guard does not affect the tested configuration, since
`RmGpuDirectRdmaForceSPA` was not enabled during this test.
`NVreg_ExperimentalDmaBufP2P=1` is set in
`/etc/modprobe.d/nvidia-graphics-drivers.conf` and was active for this test
as confirmed through `/proc/driver/nvidia/params`.

## Required CUDA userspace prerequisite

The kernel branch is necessary on this GB206 system, but it is not sufficient
by itself. CUDA 13.3 in the stock 610.43.03 `libcuda` does not advertise the
DMA-BUF capability for these consumer GPUs. All successful `ib_write_bw` and
NCCL runs in this record used a private patched copy through:

```sh
LD_LIBRARY_PATH=$HOME/libcuda-patched
```

The validation used a version-specific capability-gate modification applied
to a private copy of `libcuda.so.610.43.03`. The system CUDA library was not
modified. The userspace modification is outside the scope of this repository
and is not distributed here. The stock and modified SHA-256 values above,
together with the reproduction plan's provenance checks, identify the exact
libraries used. Without the userspace change, applications stop at CUDA's
consumer-device capability gate before reaching the kernel export path tested
here. This record therefore validates the combination of the experimental
kernel path and modified CUDA userspace, not the kernel branch in isolation
and not stock CUDA support.

## Objective

Validate that NCCL uses the GPUDirect RDMA (NET/IB) transport for same-host
communication between the two RTX 5060 Ti GPUs, and determine whether
collective traffic traverses the external Ethernet links or is completed
entirely within the ConnectX-6 Lx adapter.

## Test configuration

- Single host, two RTX 5060 Ti GPUs (non-coherent, static-BAR1-eligible).
- ConnectX-6 Lx dual-port 25 GbE RoCE adapter.
- GPU-to-NIC topology: `NODE` for both GPUs; the GPUs and NIC occupy separate
  IOMMU groups and the importer uses an identity IOMMU domain.
- NCCL 2.30.7 with CUDA 13.3; nccl-tests 2.19.6 built with MPI
  (`all_reduce_perf_mpi`).
- Two MPI ranks, one rank per GPU.
- `NCCL_P2P_DISABLE=1` to force the network transport instead of CUDA P2P.
- `NCCL_SHM_DISABLE=1`, `NCCL_DMABUF_ENABLE=1`, and
  `NCCL_NET_GDR_LEVEL=SYS` to make the intended transport choice explicit.

The recorded NCCL run used:

```sh
export LD_LIBRARY_PATH=$HOME/libcuda-patched${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
export NCCL_DEBUG=INFO
export NCCL_DEBUG_SUBSYS=INIT,NET,GRAPH,P2P,SHM
export NCCL_P2P_DISABLE=1
export NCCL_SHM_DISABLE=1
export NCCL_IB_DISABLE=0
export NCCL_DMABUF_ENABLE=1
export NCCL_NET_GDR_LEVEL=SYS
export NCCL_SOCKET_IFNAME='=enp98s0f0np0'
export NCCL_IB_HCA='=mlx5_0:1,mlx5_1:1'
export CUDA_VISIBLE_DEVICES=0,1

mpirun -np 2 --bind-to none \
  -x CUDA_VISIBLE_DEVICES -x LD_LIBRARY_PATH -x NCCL_DEBUG -x NCCL_DEBUG_SUBSYS \
  -x NCCL_P2P_DISABLE -x NCCL_SHM_DISABLE -x NCCL_IB_DISABLE \
  -x NCCL_DMABUF_ENABLE -x NCCL_NET_GDR_LEVEL -x NCCL_SOCKET_IFNAME \
  -x NCCL_IB_HCA \
  ./build/all_reduce_perf_mpi -b 64M -e 1G -f 2 -g 1
```

Both ranks saw both GPUs. The MPI-enabled nccl-tests binary assigned rank 0 to
GPU 0 and rank 1 to GPU 1; rank-specific `CUDA_VISIBLE_DEVICES` masking had
failed its local-GPU-count validation and was not used for the recorded run.

## Transport selection

NCCL selected the InfiniBand/RoCE transport for both ranks:

```
Using network IB
NET/IB/.../GDRDMA
GPU Direct RDMA Enabled
```

The communicator initialized using GPUDirect RDMA rather than CUDA IPC,
shared memory, or CUDA P2P.

## GPUDirect RDMA validation

Independent verification using `ib_write_bw --use_cuda_dmabuf` on both GPU
indices confirmed that the patched-userspace/experimental-kernel combination:

- exported CUDA device memory through DMA-BUF,
- registered the exported memory with the mlx5 RDMA driver, and
- completed RDMA write bandwidth testing.

This confirms GPUDirect RDMA is functional end-to-end on this platform through
the non-coherent FORCE_PCIE DMA-BUF export path added on this branch. The tests
used the same private `libcuda` directory described above.

The generic `--use_cuda_dmabuf` path exercises the default DMA-BUF mapping.
The explicit `--use_cuda_pcie_mapping` option is required to request the BAR1
`FORCE_PCIE` translation that is governed by the kernel's non-coherent gate.
On this host, reloading the NVIDIA modules with
`NVreg_ExperimentalDmaBufP2P=0` caused the explicit PCIe mapping run to fail
at `cuMemGetHandleForAddressRange` with CUDA error `801`, which is the expected
rejection for the non-coherent `FORCE_PCIE` path.

The two same-host endpoints were started from the perftest build directory as:

```sh
# Server, terminal 1
LD_LIBRARY_PATH=$HOME/libcuda-patched \
  ./ib_write_bw -d mlx5_0 -i 1 -F --report_gbits \
  --use_cuda=0 --use_cuda_dmabuf

# Client, terminal 2
LD_LIBRARY_PATH=$HOME/libcuda-patched \
  ./ib_write_bw -d mlx5_1 -i 1 -F --report_gbits \
  --use_cuda=0 --use_cuda_dmabuf 10.200.0.1
```

The pair was repeated with `--use_cuda=1` to exercise the other GPU. These are
same-host tests over the two active RoCE ports, not a multi-host result.

## Internal forwarding validation

To determine whether NCCL traffic traversed the physical Ethernet links, the
ConnectX-6 Lx IEEE 802.3 MAC counters (PPCNT, group 0) were sampled before and
after the NCCL workload.

Observed counter deltas:

```
mlx5_0
  TX frames  +4
  RX frames  +4
  TX bytes   +576
  RX bytes   +576
mlx5_1
  TX frames  +4
  RX frames  +4
  TX bytes   +576
  RX bytes   +576
```

These counters measure frames that traverse the Ethernet MAC. The deltas are
consistent with control-plane traffic only (four small frames per port) and
are strong evidence that the NCCL collective payload did not traverse the
external 25 GbE interfaces. They do not, by themselves, constitute a formal
proof of every internal adapter datapath stage.

## Conclusion

- NCCL correctly selects the GPUDirect RDMA (NET/IB/GDRDMA) transport on this
  platform.
- GPUDirect RDMA memory registration and RDMA communication operate correctly
  through the non-coherent FORCE_PCIE DMA-BUF export path when combined with
  the private CUDA userspace capability patch.
- Workload-scale RDMA activity combined with negligible Ethernet MAC activity
  is consistent with, and provides strong evidence for, ConnectX-6 Lx
  adapter-internal forwarding. Physical-wire counter calibration remains
  required before making a categorical forwarding claim.

Inferred data path:

```
GPU0 -> GPUDirect RDMA -> ConnectX-6 Lx internal forwarding -> GPUDirect RDMA -> GPU1
```

rather than:

```
GPU0 -> 25 GbE Port 0 -> external Ethernet fabric -> 25 GbE Port 1 -> GPU1
```

The measurements indicate that NCCL can exercise the GPUDirect RDMA transport
without workload-scale traffic appearing at the external Ethernet MACs. The
specific internal adapter stages, and VF-to-VF RoCE behavior in virtualized
deployments, remain follow-up validation rather than results established by
this record.

## Health and teardown observations

The validated runs completed successfully and both GPUs remained available to
`nvidia-smi` afterward. The post-run kernel-log inspection found no new NVIDIA
Xid, RM assertion, IOMMU fault, AER error, BAR1 failure, mlx5 reset, kernel
warning, or DMA-BUF cleanup warning attributable to the workload. The record
does not claim concurrent registration stress, suspend/resume, driver reload,
or reboot coverage; those remain part of the requested follow-up matrix.

## Validation limits and requested follow-ups

This is one implementation and one host configuration. It does not establish
hardware validation for non-GB206 GPUs, other NIC families, PCIe-switch
(`PIX`/`PXB`), same-host `PHB`, cross-socket `SYS`, translated-IOMMU, or
multi-host topologies. It also does not validate stock `libcuda`, GDS/cuFile,
or VF-to-VF operation. Those configurations should repeat direct DMA-BUF RDMA
registration, bidirectional data validation, NCCL transport inspection,
concurrent register/deregister stress, cleanup, and kernel-fault checks.

The driver intentionally leaves `NVreg_ExperimentalDmaBufP2P` disabled by
default. A negative run with the option disabled should continue to reject the
non-coherent path, and a translated-IOMMU configuration should remain rejected
by the identity-domain gate.

## Feature-gate configuration

The experimental option may be supplied directly or through the driver's
aggregate registry string:

- Module parameter: `NVreg_ExperimentalDmaBufP2P=1`
- Equivalent via the aggregate string: `NVreg_RegistryDwords="ExperimentalDmaBufP2P=1"`

`/etc/modprobe.d/nvidia-graphics-drivers.conf` sets
`options nvidia NVreg_ExperimentalDmaBufP2P=1` using the correct name, and
that is what was in effect for both the `ib_write_bw` and NCCL runs recorded
here — `DMABUF_GDR_NONCOHERENT_ALLOWED()` requires the `enabled` term
unconditionally, so none of this testing would have succeeded otherwise.
