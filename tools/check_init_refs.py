#!/usr/bin/env python3
import re
import sys

from elftools.elf.constants import SH_FLAGS
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
from elftools.elf.sections import Section, SymbolTableSection

# Symbols defined by the linker script, these only mark where the init
# sections are and are fine to reference from anywhere
ALLOWED_SYMBOLS = re.compile(r"__init_\w+_(start|page_count)")


def is_init(section: Section) -> bool:
    return section.name.startswith(".init")


def section_at(sections: list[Section], addr: int) -> Section | None:
    for section in sections:
        if section["sh_addr"] <= addr < section["sh_addr"] + section["sh_size"]:
            return section
    return None


def describe(symtab: SymbolTableSection, shndx: int, addr: int) -> str:
    for sym in symtab.iter_symbols():
        if (
            sym["st_shndx"] == shndx
            and sym["st_info"]["type"] in ("STT_FUNC", "STT_OBJECT")
            and sym["st_value"] <= addr < sym["st_value"] + max(sym["st_size"], 1)
        ):
            return f"{sym.name}+{addr - sym['st_value']:#x}"
    return f"{addr:#x}"


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <kernel.elf>", file=sys.stderr)
        return 2

    path = sys.argv[1]
    with open(path, "rb") as f:
        elf = ELFFile(f)

        symtab = elf.get_section_by_name(".symtab")
        if not isinstance(symtab, SymbolTableSection):
            print(f"{path}: no symbol table", file=sys.stderr)
            return 2

        reloc_sections = [
            s for s in elf.iter_sections() if isinstance(s, RelocationSection)
        ]
        if not reloc_sections:
            print(
                f"{path}: no relocations, was it linked with --emit-relocs?",
                file=sys.stderr,
            )
            return 2

        init_sections = [
            s
            for s in elf.iter_sections()
            if is_init(s) and s["sh_flags"] & SH_FLAGS.SHF_ALLOC
        ]
        if not any(s["sh_size"] for s in init_sections):
            print(
                f"{path}: no init sections, did the linker script not match the boot code?",
                file=sys.stderr,
            )
            return 1

        errors = 0
        for rel in reloc_sections:
            source = elf.get_section(rel["sh_info"])

            # Only care about things that actually get loaded, and only about
            # the ones that stick around after init
            if not source["sh_flags"] & SH_FLAGS.SHF_ALLOC or is_init(source):
                continue

            for reloc in rel.iter_relocations():
                sym = symtab.get_symbol(reloc["r_info_sym"])
                shndx = sym["st_shndx"]
                if not isinstance(shndx, int):
                    # Undefined or absolute
                    continue

                if sym["st_info"]["type"] == "STT_SECTION":
                    target = elf.get_section(shndx)
                    if not is_init(target):
                        continue
                    name = target.name
                else:
                    # Go by address and not by section index, the linker
                    # attaches script defined symbols (like __text_start) to
                    # whatever section happens to be before them
                    target = section_at(init_sections, sym["st_value"])
                    if target is None or ALLOWED_SYMBOLS.fullmatch(sym.name):
                        continue
                    name = sym.name

                where = describe(symtab, rel["sh_info"], reloc["r_offset"])
                print(
                    f"{path}: {source.name}:{where} references {name} in {target.name}",
                    file=sys.stderr,
                )
                errors += 1

    if errors:
        print(f"{path}: {errors} reference(s) into init sections", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
