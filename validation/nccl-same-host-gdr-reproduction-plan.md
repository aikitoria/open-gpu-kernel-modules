# Same-Host DMA-BUF GPUDirect RDMA Reproduction Plan

## Purpose

Reproduce and independently validate the experimental same-host DMA-BUF GPUDirect RDMA path introduced by the driver change.

The validation must establish five distinct properties:

1. The intended experimental kernel modules are installed and loaded.
2. The experimental non-coherent DMA-BUF P2P path is enabled only when explicitly requested.
3. CUDA device memory can be exported through DMA-BUF and registered with mlx5 for RDMA without falling back to legacy pointer-based registration.
4. NCCL can establish and execute a two-rank `NET/IB/.../GDRDMA` collective between two GPUs on the same host.
5. The resulting same-host RDMA workload shows strong evidence of ConnectX internal forwarding rather than external Ethernet MAC/PHY traversal.

The plan must preserve raw evidence sufficient for independent review and must include positive, negative, stress, teardown, and physical-wire calibration tests.

---

## 1. Test Variables and Repository Locations

Use absolute paths so that perftest, NCCL, driver, and evidence capture do not depend on the invoking shell's working directory.

Run the command blocks in Bash. Enable pipeline failure propagation before
capturing any workload through `tee`, so a failed workload cannot be reported
as successful merely because `tee` exited successfully:

```bash
set -o pipefail
```

Define:

```bash
export DRIVER_SRC="$HOME/src/open-gpu-kernel-modules"
export PERFTEST_DIR="$HOME/src/perftest"
export NCCL_TESTS_DIR="$HOME/src/nccl-tests"
export PATCHED_LIBCUDA_DIR="$HOME/libcuda-patched"

export OUT="$HOME/gdr-validation/$(date +%Y%m%d-%H%M%S)"
mkdir -p "$OUT"
```

Define expected binaries:

```bash
export IB_WRITE_BW="$PERFTEST_DIR/ib_write_bw"
export NCCL_ALLREDUCE="$NCCL_TESTS_DIR/build/all_reduce_perf_mpi"
```

Do not proceed if either binary is missing:

```bash
test -x "$IB_WRITE_BW"
test -x "$NCCL_ALLREDUCE"
```

Record these variables:

```bash
env | grep -E \
'^(DRIVER_SRC|PERFTEST_DIR|NCCL_TESTS_DIR|PATCHED_LIBCUDA_DIR|OUT|IB_WRITE_BW|NCCL_ALLREDUCE)=' \
> "$OUT/test-paths.txt"
```

---

## 2. Preserve Repository and Build Provenance

Record the exact source revisions used for every relevant repository.

```bash
for repo in \
    "$DRIVER_SRC" \
    "$PERFTEST_DIR" \
    "$NCCL_TESTS_DIR"
do
    {
        echo "=== $repo ==="
        git -C "$repo" status --short
        git -C "$repo" rev-parse HEAD
        git -C "$repo" branch --show-current
        git -C "$repo" log -1 --oneline
    } >> "$OUT/repository-state.txt"
done
```

For the driver tree, additionally preserve:

```bash
git -C "$DRIVER_SRC" diff \
    > "$OUT/driver-working-tree.diff"

git -C "$DRIVER_SRC" diff --cached \
    > "$OUT/driver-index.diff"
```

If validation is intended for a particular PR commit, record the expected commit hash separately and compare it against the checked-out tree.

Do not claim reproduction of a specific PR state if the source revision is ambiguous or contains unrecorded modifications.

---

## 3. Capture Baseline Platform State

Before loading, reloading, or exercising the experimental driver path, capture:

```bash
uname -a > "$OUT/uname.txt"

cat /proc/cmdline > "$OUT/kernel-cmdline.txt"

lspci -nnk > "$OUT/lspci-nnk.txt"
lspci -tv > "$OUT/lspci-tree.txt"

nvidia-smi -q > "$OUT/nvidia-smi-q-before.txt"
nvidia-smi topo -m > "$OUT/nvidia-smi-topo.txt"

nvidia-smi \
    --query-gpu=index,name,pci.bus_id,memory.total,memory.used \
    --format=csv,noheader \
    > "$OUT/gpu-state-before.txt"

ibv_devices > "$OUT/ibv-devices.txt"
ibv_devinfo > "$OUT/ibv-devinfo.txt"
ibdev2netdev > "$OUT/ibdev2netdev.txt"

ip -br addr > "$OUT/ip-addresses.txt"
ip -br link > "$OUT/ip-links.txt"

sudo mst status -v > "$OUT/mst-status.txt"

sudo devlink port show \
    > "$OUT/devlink-ports.txt"

sudo devlink dev show \
    > "$OUT/devlink-devices.txt"

sudo dmesg -T > "$OUT/dmesg-before.txt"
```

Record software versions:

```bash
nvidia-smi > "$OUT/nvidia-smi-version.txt"

nvcc --version \
    > "$OUT/nvcc-version.txt" 2>&1 || true

git -C "$NCCL_TESTS_DIR" describe --tags --always --dirty \
    > "$OUT/nccl-tests-version.txt"

mpirun --version \
    > "$OUT/mpi-version.txt" 2>&1 || true

"$IB_WRITE_BW" --version \
    > "$OUT/perftest-version.txt" 2>&1 || true

mst version \
    > "$OUT/mft-version.txt" 2>&1 || true
```

---

## 4. Discover Devices Instead of Hard-Coding Them

The procedure must discover and then explicitly record:

- GPU 0 PCI BDF
- GPU 1 PCI BDF
- RDMA device A
- RDMA device B
- corresponding Ethernet interfaces
- corresponding MST devices
- NIC PCI functions
- RoCE IP addresses

Reference-system values may be recorded in examples, but the executable procedure must not depend on them.

Example discovery:

```bash
nvidia-smi \
    --query-gpu=index,pci.bus_id,name \
    --format=csv,noheader \
    | tee "$OUT/gpu-pci-map.txt"

ibdev2netdev \
    | tee "$OUT/rdma-netdev-map.txt"

sudo mst status -v \
    | tee "$OUT/mst-map.txt"
```

After discovery, define explicit environment variables:

```bash
export GPU0=0
export GPU1=1

export RDMA0="mlx5_0"
export RDMA1="mlx5_1"

export NETDEV0="enp98s0f0np0"
export NETDEV1="enp98s0f1np1"

export MST0="/dev/mst/mt4127_pciconf0"
export MST1="/dev/mst/mt4127_pciconf0.1"

export NIC_BDF0="0000:62:00.0"
export NIC_BDF1="0000:62:00.1"

export ROCE_IP0="10.200.0.1"
export ROCE_IP1="10.200.0.2"
```

Those values are examples from the reference host. A reproducing agent must derive and set the local equivalents.

Record the resolved values:

```bash
env | grep -E \
'^(GPU[01]|RDMA[01]|NETDEV[01]|MST[01]|NIC_BDF[01]|ROCE_IP[01])=' \
> "$OUT/resolved-devices.txt"
```

---

## 5. Verify the Experimental Kernel Module Provenance

This is a driver-validation prerequisite, not optional metadata.

Record loaded NVIDIA module information:

```bash
for mod in nvidia nvidia_modeset nvidia_drm nvidia_uvm nvidia_peermem; do
    {
        echo "=== $mod ==="
        modinfo "$mod" 2>&1 || true
        if [ -r "/sys/module/$mod/srcversion" ]; then
            echo -n "loaded srcversion: "
            cat "/sys/module/$mod/srcversion"
        fi
    } >> "$OUT/module-provenance.txt"
done
```

Record hashes of installed module files:

```bash
for mod in nvidia nvidia_modeset nvidia_drm nvidia_uvm nvidia_peermem; do
    path=$(modinfo -n "$mod" 2>/dev/null || true)
    if [ -n "$path" ] && [ -f "$path" ]; then
        sha256sum "$path"
    fi
done > "$OUT/module-sha256.txt"
```

Record Secure Boot signer metadata:

```bash
for mod in nvidia nvidia_modeset nvidia_drm nvidia_uvm nvidia_peermem; do
    {
        echo "=== $mod ==="
        modinfo -F signer "$mod" 2>/dev/null || true
        modinfo -F sig_key "$mod" 2>/dev/null || true
        modinfo -F sig_hashalgo "$mod" 2>/dev/null || true
    }
done > "$OUT/module-signatures.txt"
```

The reproducing agent must verify that the loaded module `srcversion` and installed hashes correspond to the intended experimental build.

Do not proceed with the positive validation if the module identity is ambiguous.

---

## 6. Verify the Feature Gate

Require:

```bash
grep '^ExperimentalDmaBufP2P:' /proc/driver/nvidia/params \
    | tee "$OUT/experimental-dmabuf-p2p.txt"
```

Positive testing requires:

```text
ExperimentalDmaBufP2P: 1
```

If it is `0`, positive-path testing must not proceed.

Record the entire NVIDIA parameter set:

```bash
cat /proc/driver/nvidia/params \
    > "$OUT/nvidia-params.txt"
```

The explicit feature gate is part of the validation contract. A successful transfer obtained through another configuration does not validate the experimental path.

---

## 7. Verify BAR1 and Topology Preconditions

Record BAR1 state for every GPU:

```bash
nvidia-smi -q -d MEMORY \
    > "$OUT/nvidia-memory-details.txt"
```

Capture any driver-specific BAR1 geometry/topology diagnostics added by the PR.

The validation must establish:

- static BAR1 space required by the experimental path is available;
- the selected GPU and NIC satisfy the driver's topology acceptance criteria;
- the selected GPU and NIC are not in an unsupported IOMMU relationship;
- the driver accepts the relevant identity-domain or topology condition.

Record IOMMU groups:

```bash
for bdf in \
    "$(nvidia-smi --query-gpu=pci.bus_id --format=csv,noheader | sed -n '1p')" \
    "$(nvidia-smi --query-gpu=pci.bus_id --format=csv,noheader | sed -n '2p')" \
    "$NIC_BDF0" \
    "$NIC_BDF1"
do
    bdf=${bdf#00000000:}
    bdf=${bdf#0000:}

    dev="/sys/bus/pci/devices/0000:$bdf"

    echo "=== 0000:$bdf ==="
    readlink -f "$dev/iommu_group" 2>/dev/null || true
done > "$OUT/iommu-groups.txt"
```

Capture IOMMU kernel messages:

```bash
sudo dmesg -T | grep -Ei \
'iommu|amd-vi|dmar' \
> "$OUT/iommu-kernel-log.txt"
```

IOMMU-group membership alone does not establish the active domain type. During
the first successful DMA-BUF attachment, require and preserve the driver's
runtime topology decision:

```bash
sudo dmesg -T | grep 'DMA-BUF GDR topology:' \
    > "$OUT/dmabuf-gdr-topology.txt"
```

For every importer used by the positive test, require a corresponding accepted
decision containing values equivalent to:

```text
identityIommu=1
p2pDistance=<nonnegative>
barAddressable=1
skipIommu=0
result=1
```

Capture the diagnostic after the positive attachment if it is not present yet
at this precondition stage. A missing accepted runtime decision makes the
topology result incomplete.

A reproduction cannot be considered equivalent if it bypasses topology checks that are part of the feature's safety boundary.

---

## 8. Verify No Unexpected `nvidia-peermem` Fallback

Record whether `nvidia_peermem` is loaded:

```bash
lsmod | grep -E '^nvidia_peermem\b' \
    > "$OUT/nvidia-peermem-loaded.txt" || true
```

Record module parameters if present:

```bash
find /sys/module/nvidia_peermem/parameters \
    -maxdepth 1 -type f -print -exec cat {} \; \
    > "$OUT/nvidia-peermem-params.txt" 2>&1 || true
```

Positive DMA-BUF validation must establish that the successful registration used DMA-BUF, not the legacy `nvidia-peermem` pointer-registration path.

If necessary, run a controlled validation with `nvidia_peermem` unloaded when system configuration permits it, provided doing so does not destabilize display or other required services.

The test must not infer DMA-BUF usage merely from transfer success.

---

## 9. Validate Patched `libcuda` Provenance

The private CUDA userspace library is part of the experiment and must be independently identified.

Record all files in the private library directory:

```bash
find "$PATCHED_LIBCUDA_DIR" \
    -maxdepth 1 \( -type f -o -type l \) \
    -ls \
    > "$OUT/patched-libcuda-files.txt"
```

Hash relevant library files:

```bash
find "$PATCHED_LIBCUDA_DIR" \
    -maxdepth 1 -type f \
    -name 'libcuda.so*' \
    -exec sha256sum {} \; \
    > "$OUT/patched-libcuda-sha256.txt"
```

Record stock CUDA library resolution and hashes:

```bash
ldconfig -p | grep 'libcuda.so' \
    > "$OUT/system-libcuda-resolution.txt"

while read -r path; do
    [ -f "$path" ] && sha256sum "$path"
done < <(
    ldconfig -p |
    awk '/libcuda\.so/{print $NF}' |
    sort -u
) > "$OUT/system-libcuda-sha256.txt"
```

Preserve the userspace patch manifest or diff:

```bash
find "$PATCHED_LIBCUDA_DIR" -maxdepth 1 -type f \
    \( -name '*patch*.json' -o -name 'PATCH*' \) \
    -exec cp -- {} "$OUT/" \;
```

Record the expected source driver version. For the reference validation, the patched library must correspond to the reviewed 610.43.03 userspace implementation.

Verify that the system library itself has not been replaced by the experiment.

---

## 10. Verify Runtime Loading of the Private `libcuda`

Setting `LD_LIBRARY_PATH` is insufficient evidence.

Use:

```bash
export LD_LIBRARY_PATH="$PATCHED_LIBCUDA_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
```

For at least one paired perftest run, capture dynamic-loader resolution on the
server. This command waits for a client; run the matching Section 12 client in
a second terminal rather than invoking it alone:

```bash
LD_DEBUG=libs \
"$IB_WRITE_BW" \
    -d "$RDMA0" \
    -i 1 \
    -F \
    --report_gbits \
    --use_cuda="$GPU0" \
    --use_cuda_dmabuf \
    > "$OUT/perftest-loader-run.txt" \
    2> "$OUT/perftest-loader-debug.txt"

loader_status=$?
printf '%s\n' "$loader_status" > "$OUT/perftest-loader-status.txt"
test "$loader_status" -eq 0
```

If practical, also inspect the live process:

```bash
grep -E 'libcuda\.so' /proc/<PID>/maps
```

and preserve that output.

The acceptance criterion is that the perftest and NCCL processes map the private patched `libcuda.so`, not the stock system library.

---

## 11. Verify Both GPUs and RoCE Interfaces

Require at least two visible GPUs:

```bash
nvidia-smi -L \
    | tee "$OUT/gpu-list.txt"
```

Require both RoCE ports to be active:

```bash
for dev in "$NETDEV0" "$NETDEV1"; do
    ethtool "$dev"
done > "$OUT/ethernet-link-state.txt"
```

For the reference hardware, expected link state is:

```text
Speed: 25000Mb/s
Link detected: yes
```

Record RDMA state:

```bash
ibv_devinfo -d "$RDMA0" \
    > "$OUT/${RDMA0}-devinfo.txt"

ibv_devinfo -d "$RDMA1" \
    > "$OUT/${RDMA1}-devinfo.txt"
```

---

## 12. Validate DMA-BUF GPUDirect RDMA with `perftest`

This is the low-level positive-path validation.

### Server

Run:

```bash
set -o pipefail
cd "$PERFTEST_DIR"

LD_LIBRARY_PATH="$PATCHED_LIBCUDA_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
"$IB_WRITE_BW" \
    -d "$RDMA0" \
    -i 1 \
    -F \
    --report_gbits \
    --use_cuda="$GPU0" \
    --use_cuda_dmabuf \
    2>&1 | tee "$OUT/ib-write-server.txt"

server_status=${PIPESTATUS[0]}
printf '%s\n' "$server_status" > "$OUT/ib-write-server.status"
test "$server_status" -eq 0
```

### Client

In a second terminal:

```bash
set -o pipefail
cd "$PERFTEST_DIR"

LD_LIBRARY_PATH="$PATCHED_LIBCUDA_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
"$IB_WRITE_BW" \
    -d "$RDMA1" \
    -i 1 \
    -F \
    --report_gbits \
    --use_cuda="$GPU0" \
    --use_cuda_dmabuf \
    "$ROCE_IP0" \
    2>&1 | tee "$OUT/ib-write-client.txt"

client_status=${PIPESTATUS[0]}
printf '%s\n' "$client_status" > "$OUT/ib-write-client.status"
test "$client_status" -eq 0
```

The exact GPU assignment may be varied later for topology coverage.

### Acceptance criteria

Require all of the following:

1. CUDA device-memory allocation succeeds.
2. CUDA DMA-BUF FD export succeeds.
3. A DMA-BUF registration API succeeds:
   - `ibv_reg_dmabuf_mr`, or
   - a DMA-BUF-aware `ibv_reg_mr_ex` path.
4. Registration does not fall back to ordinary pointer-based `ibv_reg_mr`.
5. RDMA transfer completes successfully.
6. The test produces plausible bandwidth.

Do not require a specific verbs function name if perftest implementation differences permit equivalent DMA-BUF registration paths.

Preserve the complete server and client logs.

Repeat the server/client pair with `--use_cuda="$GPU1"` on both endpoints and
write to distinct `ib-write-gpu1-*.txt` logs. Positive validation requires
successful DMA-BUF registration and transfer from both GPUs; a single-GPU pass
is incomplete for the two-GPU NCCL configuration.

---

## 13. Verify the MPI-Enabled NCCL Binary

Use only:

```bash
"$NCCL_ALLREDUCE"
```

Verify linkage:

```bash
ldd "$NCCL_ALLREDUCE" \
    | tee "$OUT/nccl-allreduce-ldd.txt"

ldd "$NCCL_ALLREDUCE" \
    | grep -i libmpi \
    > "$OUT/nccl-mpi-linkage.txt"
```

Verify MPI:

```bash
set -o pipefail
mpirun -np 2 \
    bash -c '
        echo "rank=$OMPI_COMM_WORLD_RANK size=$OMPI_COMM_WORLD_SIZE pid=$$"
    ' \
    | tee "$OUT/mpi-rank-check.txt"

mpi_status=${PIPESTATUS[0]}
printf '%s\n' "$mpi_status" > "$OUT/mpi-rank-check.status"
test "$mpi_status" -eq 0
```

Require:

```text
rank=0 size=2
rank=1 size=2
```

---

## 14. GPU Visibility for `nccl-tests`

Expose both GPUs to both MPI processes:

```bash
export CUDA_VISIBLE_DEVICES=0,1
```

Do **not** use:

```bash
export CUDA_VISIBLE_DEVICES=$OMPI_COMM_WORLD_LOCAL_RANK
```

for this MPI-enabled `nccl-tests` configuration.

That rank-specific masking was experimentally shown to fail because each MPI process sees only one GPU while `nccl-tests` validates the total local GPU requirement for the two local MPI ranks:

```text
Invalid number of GPUs: 2 requested but only 1 were found.
```

The successful configuration exposes both GPUs and allows `all_reduce_perf_mpi` to assign:

```text
Rank 0 → GPU 0
Rank 1 → GPU 1
```

internally.

This assignment must be confirmed from the NCCL log rather than assumed.

---

## 15. Configure NCCL to Exercise the Network Path

Set:

```bash
export LD_LIBRARY_PATH="$PATCHED_LIBCUDA_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

export NCCL_DEBUG=INFO
export NCCL_DEBUG_SUBSYS=INIT,NET,GRAPH,P2P,SHM

export NCCL_P2P_DISABLE=1
export NCCL_SHM_DISABLE=1

export NCCL_IB_DISABLE=0
export NCCL_DMABUF_ENABLE=1
export NCCL_NET_GDR_LEVEL=SYS

export NCCL_IB_HCA="=${RDMA0}:1,${RDMA1}:1"
export NCCL_SOCKET_IFNAME="=$NETDEV0"
```

The intention is to prevent ordinary same-host CUDA P2P and SHM transport from satisfying the collective.

Record all relevant environment variables:

```bash
env | grep -E \
'^(CUDA_VISIBLE_DEVICES|LD_LIBRARY_PATH|NCCL_)' \
> "$OUT/nccl-environment.txt"
```

---

## 16. Capture RDMA Counters Before NCCL

Use labeled output.

```bash
capture_rdma_counters() {
    for dev in "$RDMA0" "$RDMA1"; do
        for counter in \
            port_xmit_data \
            port_rcv_data \
            port_xmit_packets \
            port_rcv_packets
        do
            path="/sys/class/infiniband/$dev/ports/1/counters/$counter"

            if [ -r "$path" ]; then
                printf '%s %-24s %s\n' \
                    "$dev" \
                    "$counter" \
                    "$(cat "$path")"
            fi
        done
    done
}

capture_rdma_counters \
    > "$OUT/rdma-counters-before.txt"
```

Also capture available RoCE hardware counters:

```bash
capture_roce_hw_counters() {
    for dev in "$RDMA0" "$RDMA1"; do
        for counter in \
            rx_write_requests \
            rx_read_requests \
            rx_atomic_requests \
            req_transport_retries_exceeded \
            req_rnr_retries_exceeded \
            local_ack_timeout_err \
            packet_seq_err \
            out_of_sequence \
            roce_adp_retrans \
            roce_adp_retrans_to
        do
            path="/sys/class/infiniband/$dev/ports/1/hw_counters/$counter"

            if [ -r "$path" ]; then
                printf '%s %-36s %s\n' \
                    "$dev" \
                    "$counter" \
                    "$(cat "$path")"
            fi
        done
    done
}

capture_roce_hw_counters \
    > "$OUT/rdma-hw-counters-before.txt"
```

---

## 17. Capture Firmware IEEE 802.3 MAC Counters Before NCCL

Use the ConnectX `PPCNT` register through MFT.

First verify the register layout:

```bash
sudo mlxreg \
    -d "$MST0" \
    --show_reg PPCNT \
    > "$OUT/ppcnt-definition.txt"
```

Require that `grp=0` exposes IEEE 802.3 counters including:

```text
a_frames_transmitted_ok
a_frames_received_ok
a_octets_transmitted_ok
a_octets_received_ok
```

Capture both functions:

```bash
sudo mlxreg \
    -d "$MST0" \
    --reg_name PPCNT \
    --get \
    --indexes "local_port=1,grp=0" \
    > "$OUT/ppcnt-port0-before.txt"

sudo mlxreg \
    -d "$MST1" \
    --reg_name PPCNT \
    --get \
    --indexes "local_port=1,grp=0" \
    > "$OUT/ppcnt-port1-before.txt"
```

Relevant 64-bit values are:

```text
a_frames_transmitted_ok = (high << 32) | low
a_frames_received_ok    = (high << 32) | low

a_octets_transmitted_ok = (high << 32) | low
a_octets_received_ok    = (high << 32) | low
```

These counters form the principal MAC-level evidence for determining whether workload-scale traffic reaches the external Ethernet MAC.

---

## 18. Capture Kernel and GPU Health Baseline Immediately Before Workload

Record:

```bash
sudo dmesg -T \
    > "$OUT/dmesg-pre-nccl.txt"

nvidia-smi \
    --query-gpu=index,memory.used,utilization.gpu,temperature.gpu \
    --format=csv,noheader \
    > "$OUT/gpu-health-pre-nccl.txt"
```

Capture any PR-specific diagnostics relevant to:

- BAR1 mapping state
- DMA-BUF registration state
- active mappings
- topology classification
- FORCE_PCIE or equivalent internal path state

---

## 19. Run the Two-Rank NCCL Collective

Run:

```bash
set -o pipefail
cd "$NCCL_TESTS_DIR"

mpirun -np 2 \
    --bind-to none \
    -x CUDA_VISIBLE_DEVICES \
    -x LD_LIBRARY_PATH \
    -x NCCL_DEBUG \
    -x NCCL_DEBUG_SUBSYS \
    -x NCCL_P2P_DISABLE \
    -x NCCL_SHM_DISABLE \
    -x NCCL_IB_DISABLE \
    -x NCCL_DMABUF_ENABLE \
    -x NCCL_NET_GDR_LEVEL \
    -x NCCL_IB_HCA \
    -x NCCL_SOCKET_IFNAME \
    "$NCCL_ALLREDUCE" \
        -b 64M \
        -e 1G \
        -f 2 \
        -g 1 \
    2>&1 | tee "$OUT/nccl-mpi.log"

nccl_status=${PIPESTATUS[0]}
printf '%s\n' "$nccl_status" > "$OUT/nccl-mpi.status"
test "$nccl_status" -eq 0
```

Require successful completion.

---

## 20. Verify the NCCL Communicator and GPU Assignment

Extract:

```bash
grep -E \
'Rank|nranks|nRanks|nNodes|localRanks|Channel .*0.*1|Using network IB|GPU Direct RDMA Enabled|GDRDMA' \
"$OUT/nccl-mpi.log" \
> "$OUT/nccl-validation-lines.txt"
```

Require:

```text
Rank 0 ... device 0
Rank 1 ... device 1
```

and:

```text
rank 0 nranks 2
rank 1 nranks 2
```

and channel topology containing both ranks:

```text
Channel ... : 0 1
```

Reject:

- two independent rank-0 communicators;
- both ranks mapped to the same GPU;
- single-rank operation.

---

## 21. Verify the Actual NCCL Transport

Backend initialization alone is insufficient.

Require established connector lines equivalent to:

```text
0[0] -> 1[1] ... via NET/IB/.../GDRDMA
1[1] -> 0[0] ... via NET/IB/.../GDRDMA
```

Also preserve:

```text
Using network IB
GPU Direct RDMA Enabled
```

Record any aggregated network device selected by NCCL, such as:

```text
mlx5_0+mlx5_1
```

Acceptance requires actual `GDRDMA` channel connectors, not merely discovery of a GDR-capable HCA.

---

## 22. Capture RDMA and MAC Counters After NCCL

Immediately repeat the RDMA captures:

```bash
capture_rdma_counters \
    > "$OUT/rdma-counters-after.txt"

capture_roce_hw_counters \
    > "$OUT/rdma-hw-counters-after.txt"
```

Capture `PPCNT grp=0`:

```bash
sudo mlxreg \
    -d "$MST0" \
    --reg_name PPCNT \
    --get \
    --indexes "local_port=1,grp=0" \
    > "$OUT/ppcnt-port0-after.txt"

sudo mlxreg \
    -d "$MST1" \
    --reg_name PPCNT \
    --get \
    --indexes "local_port=1,grp=0" \
    > "$OUT/ppcnt-port1-after.txt"
```

Calculate and preserve numeric deltas.

The reference run observed:

```text
mlx5_0:
  a_frames_transmitted_ok  +4
  a_frames_received_ok     +4
  a_octets_transmitted_ok  +576
  a_octets_received_ok     +576

mlx5_1:
  a_frames_transmitted_ok  +4
  a_frames_received_ok     +4
  a_octets_transmitted_ok  +576
  a_octets_received_ok     +576
```

Exact values are not an acceptance requirement. The criterion is that MAC traffic remains negligible relative to the NCCL workload while RDMA activity increases materially.

---

## 23. Required Physical-Wire Counter Calibration

This control is required if the final report makes a statement about internal forwarding.

Use a documented physical path: either two hosts connected through the tested
ports or a verified external loop/switch path between the two ports. Do not use
this control when the route could be satisfied internally without traversing
the MAC/PHY.

On the single-host two-port reference configuration, capture `PPCNT grp=0`, run
a bounded port-to-port transfer, and capture the counters again. Use a control
port distinct from other perftest processes:

```bash
export WIRE_CONTROL_PORT=18525

sudo mlxreg -d "$MST0" --reg_name PPCNT --get \
    --indexes "local_port=1,grp=0" \
    > "$OUT/wire-ppcnt-port0-before.txt"
sudo mlxreg -d "$MST1" --reg_name PPCNT --get \
    --indexes "local_port=1,grp=0" \
    > "$OUT/wire-ppcnt-port1-before.txt"

LD_LIBRARY_PATH="$PATCHED_LIBCUDA_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
timeout 45s "$IB_WRITE_BW" -d "$RDMA0" -i 1 -F --report_gbits -D 10 \
    -p "$WIRE_CONTROL_PORT" --use_cuda="$GPU0" --use_cuda_dmabuf \
    > "$OUT/wire-server.txt" 2>&1 &
wire_server_pid=$!

sleep 1

LD_LIBRARY_PATH="$PATCHED_LIBCUDA_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
timeout 45s "$IB_WRITE_BW" -d "$RDMA1" -i 1 -F --report_gbits -D 10 \
    -p "$WIRE_CONTROL_PORT" --use_cuda="$GPU0" --use_cuda_dmabuf \
    "$ROCE_IP0" > "$OUT/wire-client.txt" 2>&1
wire_client_status=$?

wait "$wire_server_pid"
wire_server_status=$?

printf 'server=%s\nclient=%s\n' \
    "$wire_server_status" "$wire_client_status" \
    > "$OUT/wire-control.status"
test "$wire_server_status" -eq 0
test "$wire_client_status" -eq 0

sudo mlxreg -d "$MST0" --reg_name PPCNT --get \
    --indexes "local_port=1,grp=0" \
    > "$OUT/wire-ppcnt-port0-after.txt"
sudo mlxreg -d "$MST1" --reg_name PPCNT --get \
    --indexes "local_port=1,grp=0" \
    > "$OUT/wire-ppcnt-port1-after.txt"
```

Decode the four 64-bit counters using the same high/low-word calculation as
Section 17. Preserve the numeric before, after, and delta values in
`$OUT/wire-ppcnt-deltas.txt`.

Expected physical-wire behavior:

```text
MST0 TX octets ≈ MST1 RX octets
MST1 TX octets ≈ MST0 RX octets
```

with increases commensurate with the transfer volume.

This establishes experimentally that:

1. the selected `PPCNT` counters do respond to actual external Ethernet traffic; and
2. the near-zero MAC deltas observed during same-host NCCL are not caused by a dead or irrelevant counter source.

The evidentiary comparison is:

```text
Physical-wire control:
  large MAC counter deltas

Same-host NCCL GDR:
  large RDMA activity
  negligible MAC counter deltas
```

This differential result provides strong evidence for adapter-internal forwarding.

---

## 24. Feature-Gate Negative Control

This is the primary negative control for the driver change.

Disable:

```text
NVreg_ExperimentalDmaBufP2P=0
```

using the supported module reload or reboot procedure for the test system.

After reload/reboot, require:

```bash
grep '^ExperimentalDmaBufP2P:' /proc/driver/nvidia/params
```

to report:

```text
ExperimentalDmaBufP2P: 0
```

Repeat the low-level DMA-BUF registration test.

The expected result is that the experimental non-coherent path is unavailable or rejected.

Acceptance criteria:

- the new experimental path does not become usable when the feature gate is disabled;
- failure occurs before successful RDMA use of the prohibited mapping;
- no crash, Xid, IOMMU fault, BAR1 corruption, leaked registration, or other unsafe behavior occurs.

Do not require a specific errno unless the driver ABI explicitly guarantees one.

Restore:

```text
NVreg_ExperimentalDmaBufP2P=1
```

before subsequent positive or stress tests.

---

## 25. NCCL Transport Negative Controls

These are secondary controls. Run each in an isolated subshell restored from
the known-positive environment, and write each result to a distinct log. Do
not carry a variable changed by one control into the next.

Define a reusable bounded workload:

```bash
run_nccl_control() {
    local label=$1
    set -o pipefail

    mpirun -np 2 --bind-to none \
        -x CUDA_VISIBLE_DEVICES -x LD_LIBRARY_PATH \
        -x NCCL_DEBUG -x NCCL_DEBUG_SUBSYS \
        -x NCCL_P2P_DISABLE -x NCCL_SHM_DISABLE \
        -x NCCL_IB_DISABLE -x NCCL_DMABUF_ENABLE \
        -x NCCL_NET_GDR_LEVEL -x NCCL_IB_HCA \
        -x NCCL_SOCKET_IFNAME \
        "$NCCL_ALLREDUCE" -b 64M -e 256M -f 2 -g 1 \
        2>&1 | tee "$OUT/nccl-${label}.log"

    local status=${PIPESTATUS[0]}
    printf '%s\n' "$status" > "$OUT/nccl-${label}.status"
    return "$status"
}

set_positive_nccl_environment() {
    export CUDA_VISIBLE_DEVICES=0,1
    export NCCL_P2P_DISABLE=1
    export NCCL_SHM_DISABLE=1
    export NCCL_IB_DISABLE=0
    export NCCL_DMABUF_ENABLE=1
    export NCCL_NET_GDR_LEVEL=SYS
    export NCCL_IB_HCA="=${RDMA0}:1,${RDMA1}:1"
    export NCCL_SOCKET_IFNAME="=$NETDEV0"
}
```

### P2P allowed

Run with only CUDA P2P restored:

```bash
(
    set_positive_nccl_environment
    export NCCL_P2P_DISABLE=0
    run_nccl_control p2p-allowed
)
```

Expected outcome:

- NCCL may select CUDA P2P for the same-host topology;
- `NET/IB/.../GDRDMA` is no longer required.

### IB disabled

Run with only the IB backend disabled:

```bash
(
    set_positive_nccl_environment
    export NCCL_IB_DISABLE=1
    run_nccl_control ib-disabled
)
```

Expected outcome:

- no `NET/IB/.../GDRDMA` connectors.

### DMA-BUF disabled

Run with only DMA-BUF disabled:

```bash
(
    set_positive_nccl_environment
    export NCCL_DMABUF_ENABLE=0
    run_nccl_control dmabuf-disabled
)
```

Interpret the result carefully because NCCL may have another supported registration path.

Do not describe a resulting successful collective as DMA-BUF GDR unless the registration mechanism is independently established.

---

## 26. Registration/Deregistration Stress

Because the driver change modifies DMA/BAR1 mapping behavior and the relevant locking path, execute repeated registration and teardown under concurrency.

At minimum:

- multiple processes;
- repeated CUDA allocation;
- DMA-BUF export;
- RDMA registration;
- transfer;
- deregistration;
- FD close;
- CUDA free.

Run enough iterations to exercise overlapping registration and deregistration.

Example conceptual stress matrix:

```text
1 process  × 1 GPU  × repeated registration
2 processes × same GPU
2 processes × different GPUs
concurrent register/deregister
rapid teardown/recreate
```

The following bounded reference loop exercises overlapping registrations on
both GPUs. Adjust the duration or round count upward only after this baseline
passes. Each pair uses a distinct TCP control port and preserves separate
endpoint logs:

```bash
export STRESS_ROUNDS=20
: > "$OUT/stress-failures.txt"

run_stress_pair() {
    local gpu=$1
    local port=$2
    local label=$3

    LD_LIBRARY_PATH="$PATCHED_LIBCUDA_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
    timeout 45s "$IB_WRITE_BW" -d "$RDMA0" -i 1 -F --report_gbits \
        -D 5 -p "$port" --use_cuda="$gpu" --use_cuda_dmabuf \
        > "$OUT/stress-${label}-server.log" 2>&1 &
    local server_pid=$!

    sleep 1

    LD_LIBRARY_PATH="$PATCHED_LIBCUDA_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
    timeout 45s "$IB_WRITE_BW" -d "$RDMA1" -i 1 -F --report_gbits \
        -D 5 -p "$port" --use_cuda="$gpu" --use_cuda_dmabuf \
        "$ROCE_IP0" > "$OUT/stress-${label}-client.log" 2>&1
    local client_status=$?

    wait "$server_pid"
    local server_status=$?

    printf 'server=%s\nclient=%s\n' \
        "$server_status" "$client_status" \
        > "$OUT/stress-${label}.status"

    if [ "$server_status" -ne 0 ] || [ "$client_status" -ne 0 ]; then
        printf '%s server=%s client=%s\n' \
            "$label" "$server_status" "$client_status" \
            >> "$OUT/stress-failures.txt"
        return 1
    fi
}

for round in $(seq 1 "$STRESS_ROUNDS"); do
    port0=$((19000 + round * 2))
    port1=$((port0 + 1))

    run_stress_pair "$GPU0" "$port0" "r${round}-gpu0" &
    stress_pid0=$!
    run_stress_pair "$GPU1" "$port1" "r${round}-gpu1" &
    stress_pid1=$!

    wait "$stress_pid0" || true
    wait "$stress_pid1" || true
done

test ! -s "$OUT/stress-failures.txt"
```

This provides a minimum of 40 overlapping registration/transfer/teardown
cycles on the two-GPU reference host. Record any host-specific reduction in
parallelism or iteration count rather than silently changing the matrix.

Where practical, include both NIC functions.

Preserve:

- process exit status;
- iteration count;
- kernel log;
- GPU memory before/after;
- BAR1 diagnostics before/after.

Any deadlock, refcount leak, stale mapping, BAR1 exhaustion, use-after-free warning, GPU reset, mlx5 reset, or IOMMU fault is a failure.

---

## 27. Post-Test Teardown and Resource Validation

After all workloads complete, verify that no registrations remain unexpectedly retained.

Capture:

```bash
nvidia-smi \
    --query-gpu=index,memory.used,utilization.gpu,temperature.gpu \
    --format=csv,noheader \
    > "$OUT/gpu-health-after.txt"

nvidia-smi -q \
    > "$OUT/nvidia-smi-q-after.txt"

sudo dmesg -T \
    > "$OUT/dmesg-after.txt"
```

Compare GPU memory usage against the baseline.

Small runtime allocator differences may be acceptable, but persistent workload-sized memory retention requires investigation.

Capture any driver diagnostics exposing:

- outstanding DMA-BUF mappings;
- BAR1 mappings;
- registration counts;
- peer mappings;
- cleanup/refcount state.

---

## 28. Kernel Health Audit

Generate a focused before/after kernel-log report.

Search for:

```bash
grep -Ei \
'Xid|NVRM|assert|BAR1|dma.?buf|IOMMU|AMD-Vi|DMAR|AER|PCIe.*error|mlx5.*(error|reset|fatal)|page fault|use-after-free|refcount|WARN|BUG|Call Trace' \
"$OUT/dmesg-after.txt" \
> "$OUT/kernel-health-findings.txt"
```

Compare against the pre-test log so pre-existing messages are not misclassified as test regressions.

A successful validation requires no new unexpected:

- NVIDIA Xids;
- RM assertions;
- BAR1 failures;
- IOMMU faults;
- AER faults;
- mlx5 resets;
- DMA-BUF lifecycle warnings;
- kernel WARN/BUG reports;
- memory-corruption indicators.

---

## 29. Post-Test CUDA and GPU P2P Health Check

After the stress and negative-control phases, verify that the GPUs still function normally.

Run an ordinary CUDA smoke test or known-good CUDA sample.

Also verify ordinary CUDA P2P capability/state where expected.

At minimum:

```bash
nvidia-smi
```

must succeed and both GPUs must remain operational.

Prefer an actual CUDA allocation/kernel/copy test rather than relying solely on `nvidia-smi`.

The validation is incomplete if the experimental workload succeeds but leaves the GPU or driver in a degraded state.

---

## 30. Result Classification

### DMA-BUF GPUDirect RDMA: PASS

Require:

- intended experimental kernel module loaded;
- `ExperimentalDmaBufP2P: 1`;
- topology prerequisites accepted;
- patched `libcuda` verified at runtime;
- CUDA DMA-BUF FD export succeeds;
- DMA-BUF verbs registration succeeds;
- no pointer-registration fallback;
- RDMA transfer completes.

### NCCL GPUDirect RDMA: PASS

Additionally require:

- one two-rank MPI communicator;
- rank 0 and rank 1 use separate GPUs;
- `Using network IB`;
- actual connectors report `NET/IB/.../GDRDMA`;
- workload-scale RDMA activity is observed.

### Internal ConnectX forwarding: STRONGLY SUPPORTED

Require:

- successful NCCL GDRDMA result;
- substantial RDMA activity;
- negligible IEEE 802.3 MAC deltas during NCCL;
- required physical-wire control shows that the same MAC counters increase substantially when traffic actually traverses the external ports.

Describe this as **strong evidence for adapter-internal forwarding**, not formal proof of every internal ASIC stage.

### Feature-gate safety: PASS

Require:

- `NVreg_ExperimentalDmaBufP2P=0` prevents use of the experimental path;
- rejection occurs safely;
- no crash or resource leak occurs;
- re-enabling the feature restores the positive path.

### Stability: PASS

Require:

- no new Xid;
- no kernel assertion;
- no IOMMU fault;
- no AER fault;
- no mlx5 reset;
- no BAR1 failure;
- no DMA-BUF cleanup warning;
- no persistent registration or workload-sized GPU-memory leak;
- post-test CUDA operation succeeds.

---

## 31. Required Evidence Artifacts

The commands above write a flat evidence directory. Preserve at minimum:

```text
repository-state.txt
driver-working-tree.diff
driver-index.diff
module-provenance.txt
module-sha256.txt
module-signatures.txt
patched-libcuda-files.txt
patched-libcuda-sha256.txt
system-libcuda-resolution.txt
system-libcuda-sha256.txt
perftest-loader-debug.txt
perftest-loader-run.txt
perftest-loader-status.txt
uname.txt
kernel-cmdline.txt
lspci-tree.txt
nvidia-smi-topo.txt
gpu-pci-map.txt
iommu-groups.txt
ibdev2netdev.txt
mst-status.txt
devlink-ports.txt
nvidia-params.txt
experimental-dmabuf-p2p.txt
dmabuf-gdr-topology.txt
ib-write-server.txt
ib-write-server.status
ib-write-client.txt
ib-write-client.status
ib-write-gpu1-*.txt
nccl-environment.txt
nccl-mpi.log
nccl-mpi.status
nccl-validation-lines.txt
nccl-p2p-allowed.{log,status}
nccl-ib-disabled.{log,status}
nccl-dmabuf-disabled.{log,status}
rdma-counters-{before,after}.txt
rdma-hw-counters-{before,after}.txt
ppcnt-port{0,1}-{before,after}.txt
wire-ppcnt-port{0,1}-{before,after}.txt
wire-ppcnt-deltas.txt
wire-control.status
wire-{server,client}.txt
gpu-state-before.txt
gpu-health-pre-nccl.txt
gpu-health-after.txt
dmesg-before.txt
dmesg-pre-nccl.txt
dmesg-after.txt
kernel-health-findings.txt
stress-*.{log,status}
stress-failures.txt
```

Raw evidence must be retained even when a summarized report is generated.

---

## 32. Final Agent Report

Produce a result matrix:

| Validation | Evidence | Result |
|---|---|---|
| Correct experimental module loaded | hashes/srcversion/signature | PASS/FAIL |
| Experimental feature enabled | `/proc/driver/nvidia/params` | PASS/FAIL |
| BAR1/topology prerequisites | driver + PCI/IOMMU diagnostics | PASS/FAIL |
| Patched `libcuda` provenance | hashes + runtime mapping | PASS/FAIL |
| CUDA DMA-BUF export | perftest log | PASS/FAIL |
| DMA-BUF verbs registration | perftest log | PASS/FAIL |
| No legacy MR fallback | registration-path evidence | PASS/FAIL |
| RDMA transfer | perftest result | PASS/FAIL |
| MPI two-rank communicator | NCCL log | PASS/FAIL |
| Separate GPU assignment | NCCL rank/device log | PASS/FAIL |
| NCCL IB transport | `Using network IB` | PASS/FAIL |
| NCCL GDRDMA connector | `NET/IB/.../GDRDMA` | PASS/FAIL |
| Workload-scale RDMA activity | RDMA counter deltas | PASS/FAIL |
| Negligible external MAC traffic | `PPCNT grp=0` deltas | PASS/FAIL |
| Physical MAC-counter calibration | forced-wire control | PASS/FAIL |
| Internal forwarding inference | differential evidence | STRONGLY SUPPORTED / NOT SUPPORTED |
| Feature disabled safely | `NVreg_ExperimentalDmaBufP2P=0` control | PASS/FAIL |
| Concurrent registration stress | stress logs | PASS/FAIL |
| Clean teardown | mapping/memory diagnostics | PASS/FAIL |
| Kernel health | Xid/IOMMU/AER/mlx5/BAR1 audit | PASS/FAIL |
| Post-test CUDA health | CUDA smoke test | PASS/FAIL |

The report must distinguish **observed facts** from **inferred architecture**.

A successful final result should be phrased approximately as:

```text
Experimental DMA-BUF GPUDirect RDMA path: PASS
NCCL NET/IB/GDRDMA communication: PASS
Feature-gate default-off behavior: PASS
Concurrent registration/deregistration stability: PASS
Post-test driver/GPU health: PASS

Same-host ConnectX internal forwarding:
STRONGLY SUPPORTED by workload-scale RDMA activity combined with
negligible IEEE 802.3 MAC activity, calibrated against a physical-wire
control that produces workload-scale MAC counter increments.
```

The reproduction plan should be committed separately from the executed validation record as:

```text
validation/nccl-same-host-gdr-reproduction-plan.md
```

with a documentation-only commit such as:

```text
Add same-host DMA-BUF GDR reproduction plan
```
