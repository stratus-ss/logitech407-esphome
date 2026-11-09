#!/usr/bin/env python3
"""Wrap a raw Cortex-M flash dump in a minimal ELF32 little-endian ARM container.

Why: REA's Ghidra lane pins raw-binary imports to the stock ARM language
(ARM:LE:32:v7, A32 mode) and explicitly rejects REA_GHIDRA_LANGUAGE_ID for
raw-binary. Cortex-M code is Thumb-2, so a bare dump decodes as garbage.
An ELF lets Ghidra's ARM loader see e_entry with the Thumb bit set, which
makes it select Thumb mode for the whole image.

Usage:
    mkelf.py <raw.bin> <out.elf> [--base 0x0] [--entry <addr>]

--base   load address of byte 0 in the dump (nRF52 flash = 0x00000000)
--entry  reset handler address; default reads the vector table at base+4
"""

import argparse
import struct
import sys

ELF_HDR = 52
PHDR = 32
SHDR = 40
SYM = 16

EM_ARM = 40
ET_EXEC = 2
PT_LOAD = 1
SHT_PROGBITS = 1
SHT_SYMTAB = 2
SHT_STRTAB = 3
SHF_ALLOC = 0x2
SHF_EXECINSTR = 0x4
EF_ARM_HASENTRY = 0x2
EF_ARM_EABI_VER5 = 0x05000000
STT_FUNC = 2
STB_GLOBAL = 1
VECTOR_COUNT = 16


def build(data: bytes, base: int, entry: int) -> bytes:
    # Thumb bit: an ARM ELF function symbol keeps bit 0 set to mark Thumb state.
    syms = []           # (st_name offset, st_value, st_size)
    names = b"\0"
    for i in range(1, VECTOR_COUNT):
        off = base + 4 * i
        if off + 4 > len(data):
            break
        (word,) = struct.unpack_from("<I", data, off)
        if word == 0:
            continue
        name = b"Reset_Handler" if i == 1 else b"irq_%d" % i
        syms.append((len(names), word, 0))
        names += name + b"\0"

    symtab = b""
    for st_name, value, size in syms:
        symtab += struct.pack("<IIIBBH", st_name, value, size,
                              (STB_GLOBAL << 4) | STT_FUNC, 0, 1)

    strtab = names
    shstrtab = b"\0.text\0.symtab\0.strtab\0.shstrtab\0"
    sh_text = shstrtab.index(b".text")
    sh_symtab = shstrtab.index(b".symtab")
    sh_strtab = shstrtab.index(b".strtab")
    sh_shstr = shstrtab.index(b".shstrtab")

    text_off = ELF_HDR + PHDR
    text_off += (-text_off) % 4
    sym_off = text_off + len(data)
    sym_off += (-sym_off) % 4
    str_off = sym_off + len(symtab)
    shstr_off = str_off + len(strtab)
    sh_off = shstr_off + len(shstrtab)
    sh_off += (-sh_off) % 4

    ehdr = struct.pack(
        "<16sHHIIIIIHHHHHH",
        b"\x7fELF\x01\x01\x01" + b"\0" * 9,   # e_ident
        ET_EXEC, EM_ARM, 1,
        entry | 1,                 # e_entry, Thumb bit set
        ELF_HDR, sh_off,
        EF_ARM_EABI_VER5 | EF_ARM_HASENTRY,
        ELF_HDR, PHDR, 1,
        SHDR, 5, 4,
    )
    phdr = struct.pack("<IIIIIIII", PT_LOAD, text_off, base, base,
                       len(data), len(data), 5, 4)  # p_flags = R|X

    shdrs = [
        struct.pack("<IIIIIIIIII", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        struct.pack("<IIIIIIIIII", sh_text, SHT_PROGBITS,
                    SHF_ALLOC | SHF_EXECINSTR, base, text_off, len(data),
                    0, 0, 4, 0),
        struct.pack("<IIIIIIIIII", sh_symtab, SHT_SYMTAB, 0, 0, sym_off,
                    len(symtab), 3, 1, 4, SYM),
        struct.pack("<IIIIIIIIII", sh_strtab, SHT_STRTAB, 0, 0, str_off,
                    len(strtab), 0, 0, 1, 0),
        struct.pack("<IIIIIIIIII", sh_shstr, SHT_STRTAB, 0, 0, shstr_off,
                    len(shstrtab), 0, 0, 1, 0),
    ]

    out = bytearray()
    out += ehdr
    out += phdr
    out += b"\0" * (text_off - len(out))
    out += data
    out += b"\0" * (sym_off - len(out))
    out += symtab
    out += strtab
    out += shstrtab
    out += b"\0" * (sh_off - len(out))
    for s in shdrs:
        out += s
    return bytes(out)


def default_entry(data: bytes, base: int) -> int:
    if len(data) < base + 8:
        raise ValueError("dump too short for a vector table")
    (reset,) = struct.unpack_from("<I", data, base + 4)
    if reset == 0:
        raise ValueError("vector table reset vector is zero")
    return reset


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("raw")
    ap.add_argument("out")
    ap.add_argument("--base", default="0x0")
    ap.add_argument("--entry", default=None)
    a = ap.parse_args()

    base = int(a.base, 0)
    data = open(a.raw, "rb").read()
    entry = int(a.entry, 0) if a.entry else default_entry(data, base)
    elf = build(data, base, entry)
    open(a.out, "wb").write(elf)
    print("wrote %s (%d bytes) base=0x%x entry=0x%x" % (a.out, len(elf), base, entry))
    return 0


def _selfcheck() -> None:
    # Synthetic Cortex-M-ish image: vector table + Thumb-2 function.
    data = bytearray(b"\0" * 0x100)
    struct.pack_into("<I", data, 0x00, 0x20002000)   # initial SP
    struct.pack_into("<I", data, 0x04, 0x08000101)   # reset handler (Thumb)
    for i in range(2, 16):
        struct.pack_into("<I", data, 4 * i, 0x08000101)
    # push {lr}; movs r0,#1; movs r1,#2; pop {pc}
    data[0x100:0x104] = bytes([0x00, 0xB5, 0x01, 0x20, 0x02, 0x21, 0x00, 0xBD])

    elf = build(bytes(data), 0, 0x08000100)
    assert elf[:4] == b"\x7fELF", "bad magic"
    (e_type, e_machine, e_version, e_entry, e_phoff, e_shoff, e_flags,
     e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx) = \
        struct.unpack_from("<HHIIIIIHHHHHH", elf, 16)
    assert e_type == ET_EXEC and e_machine == EM_ARM
    assert e_entry == 0x08000101, "entry must carry the Thumb bit"
    assert e_flags & EF_ARM_HASENTRY, "EF_ARM_HASENTRY must be set for Thumb"
    assert e_ehsize == ELF_HDR and e_phentsize == PHDR and e_shentsize == SHDR
    (p_type, p_off, p_vaddr, _, p_filesz, p_memsz, p_flags, _) = \
        struct.unpack_from("<IIIIIIII", elf, e_phoff)
    assert p_type == PT_LOAD and p_vaddr == 0 and p_filesz == len(data)
    assert p_flags == 5, "segment must be R+X"
    assert elf[p_off:p_off + 4] == bytes(data[0:4]), "vector table mismatch"
    assert elf[p_off + 0x100:p_off + 0x104] == bytes(data[0x100:0x104]), \
        "text mismatch"
    print("selfcheck ok")


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--selfcheck":
        _selfcheck()
    else:
        sys.exit(main())
