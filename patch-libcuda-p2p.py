#!/usr/bin/env python3
"""Persistently patch libcuda for mixed-generation P2P support."""

from pathlib import Path
import re
import sys


# Each rewrite is (byte offset in the signature, replacement bytes).
PATCHES = (
    (
        "can-access predicate",
        """
        48 8b 83 50 0c 00 00     # mov rax, qword ptr [rbx + 0xc50]
        49 8b 94 24 50 0c 00 00  # mov rdx, qword ptr [r12 + 0xc50]
        48 39 d0                 # cmp rax, rdx
        74 ??                    # je 0x44 -> jmp 0x44
        48 3d c0 00 00 00        # cmp rax, 0xc0
        75 ??                    # jne 0x9
        48 81 fa c8 00 00 00     # cmp rdx, 0xc8
        74 ??                    # je 0x33
        48 3d c8 00 00 00        # cmp rax, 0xc8
        75 ??                    # jne 0x9
        48 81 fa c0 00 00 00     # cmp rdx, 0xc0
        74 ??                    # je 0x22
        83 e0 f0                 # and eax, -0x10
        48 3d f0 00 00 00        # cmp rax, 0xf0
        74 ??                    # je 0xb
        31 c0                    # xor eax, eax
        """,
        (18, b"\xeb"),
    ),
    (
        "compatibility predicate",
        """
        48 8b 87 50 0c 00 00  # mov rax, qword ptr [rdi + 0xc50]
        48 8b 93 50 0c 00 00  # mov rdx, qword ptr [rbx + 0xc50]
        48 39 d0              # cmp rax, rdx
        74 ??                 # je 0x45 -> jmp 0x45
        48 3d c0 00 00 00     # cmp rax, 0xc0
        75 ??                 # jne 0x9
        48 81 fa c8 00 00 00  # cmp rdx, 0xc8
        74 ??                 # je 0x34
        48 3d c8 00 00 00     # cmp rax, 0xc8
        75 ??                 # jne 0x9
        48 81 fa c0 00 00 00  # cmp rdx, 0xc0
        74 ??                 # je 0x23
        83 e0 f0              # and eax, -0x10
        48 3d f0 00 00 00     # cmp rax, 0xf0
        75 ??                 # jne 0xc
        83 e2 f0              # and edx, -0x10
        """,
        (17, b"\xeb"),
    ),
    (
        "peer-device selection",
        """
        49 8b 87 50 0c 00 00  # mov rax, qword ptr [r15 + 0xc50]
        49 8b 96 50 0c 00 00  # mov rdx, qword ptr [r14 + 0xc50]
        48 39 d0              # cmp rax, rdx
        0f 84 ?? ?? ?? ??     # je 0x87 -> nop; jmp 0x87
        48 81 fa c8 00 00 00  # cmp rdx, 0xc8
        75 ??                 # jne 0x8
        48 3d c0 00 00 00     # cmp rax, 0xc0
        74 ??                 # je 0x76
        48 81 fa c0 00 00 00  # cmp rdx, 0xc0
        75 ??                 # jne 0x8
        48 3d c8 00 00 00     # cmp rax, 0xc8
        74 ??                 # je 0x65
        83 e0 f0              # and eax, -0x10
        48 3d f0 00 00 00     # cmp rax, 0xf0
        74 ??                 # je 0x4e
        """,
        (17, b"\x90\xe9"),
    ),
    (
        "peer setup",
        """
        49 8b 84 24 50 0c 00 00  # mov rax, qword ptr [r12 + 0xc50]
        49 8b 95 50 0c 00 00     # mov rdx, qword ptr [r13 + 0xc50]
        48 39 d0                 # cmp rax, rdx
        74 ??                    # je 0x6a -> jmp 0x6a
        48 3d c0 00 00 00        # cmp rax, 0xc0
        75 ??                    # jne 0x9
        48 81 fa c8 00 00 00     # cmp rdx, 0xc8
        74 ??                    # je 0x59
        48 81 fa c0 00 00 00     # cmp rdx, 0xc0
        75 ??                    # jne 0x8
        48 3d c8 00 00 00        # cmp rax, 0xc8
        74 ??                    # je 0x48
        83 e0 f0                 # and eax, -0x10
        48 3d f0 00 00 00        # cmp rax, 0xf0
        75 ??                    # jne 0x15
        83 e2 f0                 # and edx, -0x10
        """,
        (18, b"\xeb"),
    ),
)


if len(sys.argv) != 2:
    raise SystemExit(f"usage: {sys.argv[0]} LIBCUDA")

source = Path(sys.argv[1])
data = bytearray(source.read_bytes())
changes = []

for name, signature, (branch, replacement) in PATCHES:
    tokens = re.sub(r"#.*", "", signature).split()
    pattern = b"".join(
        b"." if token == "??" else re.escape(bytes.fromhex(token))
        for token in tokens
    )
    matches = list(re.finditer(pattern, data, re.DOTALL))
    if len(matches) != 1:
        raise SystemExit(f"error: {name} signature matched {len(matches)} times instead of once")

    offset = matches[0].start() + branch
    data[offset:offset + len(replacement)] = replacement
    changes.append((name, offset, replacement))

with source.open("r+b") as library:
    for _, offset, replacement in changes:
        library.seek(offset)
        library.write(replacement)

for name, offset, _ in changes:
    print(f"applied: {name} at 0x{offset:x}")
