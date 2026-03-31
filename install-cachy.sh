
#!/bin/bash

export  IGNORE_CC_MISMATCH=1
suddo rmmod nvidia_drm nvidia_modeset nvidia_uvm nvidia
set -e
make modules -j$(nproc) CC=clang LD=ld.lld AR=llvm-ar CXX=clang++ OBJCOPY=llvm-objcopy NV_VERBOSE=1
sudo make modules_install -j$(nproc) CC=clang LD=ld.lld AR=llvm-ar CXX=clang++ OBJCOPY=llvm-objcopy NV_VERBOSE=1
sudo depmod
nvidia-smi
