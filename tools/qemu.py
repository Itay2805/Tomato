#!/usr/bin/env python3
import argparse
import os
import subprocess
import sys
from pathlib import Path

from bob.constants import get_configs_path

# How qemu is invoked, anything after `--` is appended to these
QEMU_FLAGS = [
    "-M",
    "q35",
    "-s",
    "--enable-kvm",
    "-cpu",
    "host",
    "-smp",
    "4",
    "-m",
    "512M",
    "-debugcon",
    "stdio",
    "-monitor",
    "tcp:127.0.0.1:55555,server,nowait",
    "-display",
    "none",
    "-no-reboot",
    "-no-shutdown",
]


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
        usage="%(prog)s [-h] [-B BUILDDIR] [-- QEMU_ARGS...]",
    )
    parser.add_argument(
        "-B",
        "--builddir",
        type=Path,
        default=Path("build"),
        help="the bob build directory (default: build)",
    )
    args = parser.parse_args(argv)

    image = args.builddir / "tomato.hdd"

    # Make sure the image is up to date, reusing the configs it was last
    # configured with so we don't silently drop something like DEBUG=y
    bob = ["bob", "build", "-B", str(args.builddir)]
    if get_configs_path(args.builddir).exists():
        bob.append("-u")
    bob.append(str(image))

    result = subprocess.run(bob)
    if result.returncode != 0:
        return result.returncode

    # Hand the terminal over to qemu completely
    qemu = [
        "qemu-system-x86_64",
        *QEMU_FLAGS,
        "-drive",
        f"file={image},format=raw,if=ide",
        *qemu_args,
    ]
    os.execvp(qemu[0], qemu)


if __name__ == "__main__":
    sys.exit(main())
