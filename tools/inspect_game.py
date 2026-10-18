#!/usr/bin/env python3
"""Read-only GameMaker/PE inventory for the locally supplied Nova Drift build.

This extracts candidate YYC function-table entries, not source code or a mod.
Output is private analysis material. No target code is loaded or executed.
"""
import argparse
import collections
import hashlib
import json
import re
import struct
from pathlib import Path


class PE:
    def __init__(self, path):
        self.data = path.read_bytes()
        b = self.data
        if b[:2] != b"MZ":
            raise ValueError("Expected a PE executable")
        pe = struct.unpack_from("<I", b, 0x3C)[0]
        if b[pe:pe + 4] != b"PE\0\0":
            raise ValueError("Invalid PE signature")
        self.machine, count = struct.unpack_from("<HH", b, pe + 4)
        if self.machine != 0x8664:
            raise ValueError("This scanner expects a Windows x64 executable")
        opt = pe + 24
        if struct.unpack_from("<H", b, opt)[0] != 0x20B:
            raise ValueError("This scanner supports PE32+ only")
        self.image_base = struct.unpack_from("<Q", b, opt + 24)[0]
        section_base = opt + struct.unpack_from("<H", b, pe + 20)[0]
        self.sections = []
        for i in range(count):
            o = section_base + i * 40
            virtual_size, rva, size, raw = struct.unpack_from("<IIII", b, o + 8)
            if raw + size > len(b):
                raise ValueError("Section extends past end of executable")
            self.sections.append(dict(
                name=b[o:o + 8].rstrip(b"\0").decode("ascii"),
                virtual_size=virtual_size, rva=rva, raw=raw, size=size,
                flags=struct.unpack_from("<I", b, o + 36)[0],
            ))
        # Parse the PE exception directory, not alignment padding in .pdata.
        directory_rva, directory_size = struct.unpack_from("<II", b, opt + 112 + 3 * 8)
        self.function_ranges = {}
        directory = self.offset(self.image_base + directory_rva)
        if directory is not None:
            if directory + directory_size > len(b) or directory_size % 12:
                raise ValueError("Invalid x64 exception directory")
            for o in range(directory, directory + directory_size, 12):
                start, end, _ = struct.unpack_from("<III", b, o)
                self.function_ranges[start] = end

    def section(self, address):
        rva = address - self.image_base
        return next((s for s in self.sections
                     if s["rva"] <= rva < s["rva"] + s["size"]), None)

    def offset(self, address):
        s = self.section(address)
        return None if s is None else s["raw"] + address - self.image_base - s["rva"]

    def function_table(self):
        """Infer name/function/metadata triples; retain validation for each entry."""
        b = self.data
        result = []
        for s in self.sections:
            if s["flags"] & 0x20000000 or s["name"] not in (".data", ".rdata"):
                continue
            for o in range(s["raw"], s["raw"] + s["size"] - 23, 8):
                name_address, function, metadata = struct.unpack_from("<QQQ", b, o)
                target_section = self.section(function)
                name_offset = self.offset(name_address)
                if (target_section is None or not target_section["flags"] & 0x20000000
                        or name_offset is None or b[name_offset:name_offset + 4] != b"gml_"):
                    continue
                name_end = b.find(b"\0", name_offset, name_offset + 256)
                if name_end < 0:
                    continue
                name_bytes = b[name_offset:name_end]
                if re.fullmatch(rb"gml_[A-Za-z0-9_@]+", name_bytes) is None:
                    continue
                rva = function - self.image_base
                fn_offset = self.offset(function)
                function_end = self.function_ranges.get(rva)
                # Only a byte sample: this is not a relocation-masked signature.
                byte_count = min(32, function_end - rva) if function_end else 16
                result.append(dict(
                    name=name_bytes.decode("ascii"), rva=hex(rva),
                    preferred_va=hex(function), file_offset=fn_offset,
                    table_rva=hex(s["rva"] + o - s["raw"]),
                    metadata_preferred_va=hex(metadata),
                    unwind_entry_matches=function_end is not None,
                    unwind_end_rva=hex(function_end) if function_end else None,
                    entry_bytes=b[fn_offset:fn_offset + byte_count].hex(" "),
                    classification="static candidate; runtime unverified",
                ))
        return result


def chunk_inventory(path):
    # Read only headers and selected metadata, not hundreds of MiB of textures.
    size = path.stat().st_size
    chunks = []
    with path.open("rb") as f:
        magic, length = struct.unpack("<4sI", f.read(8))
        if magic != b"FORM" or length + 8 != size:
            raise ValueError("Invalid GameMaker FORM size")
        offset = 8
        while offset < size:
            f.seek(offset)
            tag, length = struct.unpack("<4sI", f.read(8))
            if offset + 8 + length > size:
                raise ValueError("Chunk extends past end of data.win")
            chunks.append(dict(tag=tag.decode("ascii"), offset=offset, size=length))
            offset += 8 + length
        if offset != size:
            raise ValueError("Invalid final chunk boundary")
    return chunks


def sha256(path):
    h = hashlib.sha256()
    with path.open("rb") as f:
        for block in iter(lambda: f.read(8 * 1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game-dir", type=Path, default=Path("game"))
    parser.add_argument("--out", type=Path, default=Path("local/inventory"))
    args = parser.parse_args()
    game = args.game_dir.resolve()
    out = args.out.resolve()
    if out == game or game in out.parents:
        parser.error("Output must be outside the copied game directory")
    exe = game / "NovaDrift.exe"
    data = game / "data.win"
    pe = PE(exe)
    functions = pe.function_table()
    chunks = chunk_inventory(data)
    versions = re.findall(r"^Version ([^\r\n]+)",
                          (game / "changelog.txt").read_text(encoding="utf-8-sig"), re.M)
    names = collections.Counter(f["name"] for f in functions)
    duplicates = {name: n for name, n in names.items() if n > 1}
    report = dict(
        title="Nova Drift", version_from_changelog=versions[0] if versions else None,
        exe_sha256=sha256(exe), data_sha256=sha256(data),
        exe_bytes=exe.stat().st_size, data_bytes=data.stat().st_size,
        pe_machine=hex(pe.machine), image_base=hex(pe.image_base),
        sections=pe.sections, chunks=chunks,
        has_CODE_chunk=any(c["tag"] == "CODE" for c in chunks),
        native_gml_candidates=len(functions), duplicate_names=duplicates,
        unwind_entry_matches=sum(f["unwind_entry_matches"] for f in functions),
        limitations=[
            "Candidate table layout inferred from static pointer patterns.",
            "Missing unwind entries can be legitimate leaf functions; not runtime tested.",
            "Preferred VAs must be rebased for ASLR: module base + RVA.",
            "Entry bytes are build-specific samples, not wildcard signatures.",
            "No game code is executed and no co-op behavior is established.",
        ],
    )
    out.mkdir(parents=True, exist_ok=True)
    (out / "summary.json").write_text(json.dumps(report, indent=2) + "\n")
    (out / "native-functions.json").write_text(json.dumps(functions, indent=2) + "\n")
    selected = [f for f in functions if re.search(
        r"^gml_Object_o(Player|ConstructAlly)_(Create|Step|Destroy)_"
        r"|^gml_Script_(inputControl|allyBehavior|allyWingman|allyInterceptor|saveRun|loadRun)$",
        f["name"])]
    (out / "coop-candidates.json").write_text(json.dumps(selected, indent=2) + "\n")
    print(json.dumps({k: report[k] for k in (
        "version_from_changelog", "exe_sha256", "data_sha256", "has_CODE_chunk",
        "native_gml_candidates", "unwind_entry_matches", "duplicate_names")}, indent=2))


if __name__ == "__main__":
    main()
