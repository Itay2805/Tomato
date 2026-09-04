#!/usr/bin/env python3
import argparse
import os
import subprocess
import sys
from pathlib import Path

# How qemu is invoked, anything after `--` is appended to these
QEMU_FLAGS = [
    "-M",
    "q35",
    "-s",
    "--enable-kvm",
    "-cpu",
    "host",
    "-m",
    "512M",
    "-debugcon",
    "stdio",
    "-monitor",
    "telnet:127.0.0.1:55555,server,nowait",
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
    )

    parser.add_argument(
        "-D",
        "--debug",
        action="store_true",
        default=False,
        help="build as debug",
    )

    args = parser.parse_args(argv)

    image = Path("build/tomato.hdd")

    bob = [
        "bob",
        "build",
        "-c",
        f"DEBUG={'y' if args.debug else 'n'}",
    ]
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
