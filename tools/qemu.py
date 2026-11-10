#!/usr/bin/env python3
import argparse
import os
import platform
import struct
import subprocess
import sys
from pathlib import Path

# How qemu is invoked, anything after `--` is appended to these
QEMU_FLAGS = [
    "-s",
    "-smp",
    "4",
    "-m",
    "512M",
    "-monitor",
    "telnet:127.0.0.1:55555,server,nowait",
    "-display",
    "none",
    "-no-reboot",
    "-no-shutdown",
]

# The arch specific flags, the debug console always goes to stdio
QEMU_ARCH_FLAGS = {
    "x86_64": [
        "-M",
        "q35",
        # COM1, described to the kernel by our own DBG2 table
        "-serial",
        "stdio",
    ],
    "aarch64": [
        # The kernel only supports the GICv3 system register interface
        "-M",
        "virt,gic-version=3",
        # The PL011 UART
        "-serial",
        "stdio",
        # We don't want the default NIC (and its option ROM)
        "-nic",
        "none",
    ],
}

# Where distros put the aarch64 UEFI firmware, the first one that exists is used
AARCH64_FIRMWARE = [
    "/usr/share/edk2/aarch64/QEMU_EFI.silent.fd",
    "/usr/share/edk2/aarch64/QEMU_EFI.fd",
    "/usr/share/qemu-efi-aarch64/QEMU_EFI.fd",
    "/usr/share/AAVMF/AAVMF_CODE.fd",
    "/usr/share/qemu/edk2-aarch64-code.fd",
]


def dbg2_com1_table() -> bytes:
    """
    The kernel finds its debug console in the ACPI DBG2 table, which qemu only
    provides on aarch64, so build one that describes COM1 (the 16550 at port 0x3F8)
    """
    gas = struct.pack(
        "<BBBBQ",
        1,  # system IO
        8,  # register bit width
        0,  # register bit offset
        1,  # byte access
        0x3F8,
    )
    address_size = struct.pack("<I", 8)
    namespace = b".\0"  # no namespace

    device_header = struct.Struct("<BHBHHHHHHHHH")
    gas_offset = device_header.size
    address_size_offset = gas_offset + len(gas)
    namespace_offset = address_size_offset + len(address_size)
    device_length = namespace_offset + len(namespace)
    device = (
        device_header.pack(
            0,  # revision
            device_length,
            1,  # register count
            len(namespace),
            namespace_offset,
            0,  # oem data length
            0,  # oem data offset
            0x8000,  # serial port
            0x0000,  # fully 16550 compatible
            0,  # reserved
            gas_offset,
            address_size_offset,
        )
        + gas
        + address_size
        + namespace
    )

    header = struct.Struct("<4sIBB6s8sI4sI")
    body = struct.pack("<II", header.size + 8, 1) + device
    table = bytearray(
        header.pack(
            b"DBG2",
            header.size + len(body),
            0,  # revision
            0,  # checksum, filled below
            b"TOMATO",
            b"COM1    ",
            1,  # oem revision
            b"TMTO",
            1,  # creator revision
        )
        + body
    )
    table[9] = -sum(table) & 0xFF
    return bytes(table)


def host_arch() -> str:
    machine = platform.machine().lower()
    return {"amd64": "x86_64", "arm64": "aarch64"}.get(machine, machine)


def main() -> int:
    # Split off the extra qemu flags by hand, qemu uses single dash long
    # options (`-cpu`) which argparse would happily eat as short ones
    argv = sys.argv[1:]
    qemu_args: list[str] = []
    if "--" in argv:
        split = argv.index("--")
        argv, qemu_args = argv[:split], argv[split + 1 :]

    parser = argparse.ArgumentParser(
        description="Build the tomato image and boot it under qemu.",
    )

    parser.add_argument(
        "-D",
        "--debug",
        action="store_true",
        default=False,
        help="build as debug",
    )

    parser.add_argument(
        "-a",
        "--arch",
        choices=sorted(QEMU_ARCH_FLAGS),
        default="x86_64",
        help="the architecture to build and run",
    )

    parser.add_argument(
        "--firmware",
        type=Path,
        default=None,
        help="the UEFI firmware to use on aarch64, found automatically by default",
    )

    args = parser.parse_args(argv)

    image = Path("build/tomato.hdd")

    bob = [
        "bob",
        "build",
        "-c",
        f"DEBUG={'y' if args.debug else 'n'}",
        "-c",
        f"ARCH={args.arch}",
    ]
    bob.append(str(image))

    result = subprocess.run(bob)
    if result.returncode != 0:
        return result.returncode

    qemu = [
        f"qemu-system-{args.arch}",
        *QEMU_FLAGS,
        *QEMU_ARCH_FLAGS[args.arch],
    ]

    # Use kvm when we can, otherwise emulate a cpu with everything
    if host_arch() == args.arch:
        qemu += ["--enable-kvm", "-cpu", "host"]
    else:
        qemu += ["-cpu", "max"]

    match args.arch:
        case "x86_64":
            dbg2 = image.parent / "dbg2-com1.bin"
            dbg2.write_bytes(dbg2_com1_table())
            qemu += [
                "-acpitable",
                f"file={dbg2}",
                "-drive",
                f"file={image},format=raw,if=ide",
            ]

        case "aarch64":
            firmware = args.firmware
            if firmware is None:
                firmware = next(
                    (Path(p) for p in AARCH64_FIRMWARE if Path(p).exists()), None
                )
            if firmware is None:
                print(
                    "could not find the aarch64 UEFI firmware, pass it with --firmware",
                    file=sys.stderr,
                )
                return 1

            qemu += [
                "-bios",
                str(firmware),
                "-drive",
                f"if=none,id=hd0,format=raw,file={image}",
                "-device",
                "virtio-blk-device,drive=hd0",
            ]

    # Hand the terminal over to qemu completely
    qemu += qemu_args
    os.execvp(qemu[0], qemu)


if __name__ == "__main__":
    sys.exit(main())
