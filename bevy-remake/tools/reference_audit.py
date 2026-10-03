"""Read-only BFV reference audit. Derived game data stays outside the code project.

Run with: uv run --with capstone --with pefile python tools/reference_audit.py
  --game-root .. --output ../bfv-reference-local --extract --map Ia_Drang
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import struct
import subprocess

import capstone
import pefile

PROJECT = Path(__file__).resolve().parents[1]
MAX_ENTRY_BYTES = 512 * 1024 * 1024


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def write_json(path: Path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False), encoding="utf-8")


def safe_name(name: str) -> str:
    normalized = name.replace("\\", "/")
    path = PurePosixPath(normalized)
    if not normalized or path.is_absolute() or any(part in ("..", ".") for part in path.parts) or ":" in normalized or "\x00" in normalized:
        raise ValueError(f"Unsafe archive member {name!r}")
    return str(path)


class Rfa:
    """Bounded directory reader; compression is delegated to the existing BGA tool."""
    def __init__(self, path: Path):
        self.path = path
        size = path.stat().st_size
        with path.open("rb") as stream:
            header = stream.read(8)
            if len(header) != 8:
                raise ValueError("Truncated RFA header")
            self.directory, self.compressed = struct.unpack("<II", header)
            if self.compressed not in (0, 1) or not 156 <= self.directory <= size - 4:
                raise ValueError(f"Invalid RFA header: {path}")
            stream.seek(self.directory)
            count = struct.unpack("<I", stream.read(4))[0]
            if count > 250_000:
                raise ValueError("RFA entry count exceeds limit")
            self.entries = []
            seen = set()
            for _ in range(count):
                raw_length = stream.read(4)
                if len(raw_length) != 4:
                    raise ValueError("Truncated RFA directory")
                length = struct.unpack("<I", raw_length)[0]
                if not 0 < length <= 4096:
                    raise ValueError("RFA filename length exceeds limit")
                raw_name, raw_fields = stream.read(length), stream.read(24)
                if len(raw_name) != length or len(raw_fields) != 24:
                    raise ValueError("Truncated RFA entry")
                name = safe_name(raw_name.rstrip(b"\x00").decode("cp1252"))
                compressed_size, original_size, offset, *unknown = struct.unpack("<6I", raw_fields)
                if original_size > MAX_ENTRY_BYTES or offset < 156 or offset + compressed_size > self.directory:
                    raise ValueError(f"Invalid RFA payload bounds: {path}: {name}")
                if name.casefold() in seen:
                    raise ValueError(f"Duplicate RFA member {name}")
                seen.add(name.casefold())
                self.entries.append(dict(name=name, compressed_size=compressed_size, size=original_size, offset=offset, unknown=unknown))

    def read(self, entry: dict) -> bytes:
        if self.compressed:
            raise ValueError("Compressed payload requires BGA extraction")
        with self.path.open("rb") as stream:
            stream.seek(entry["offset"])
            data = stream.read(entry["compressed_size"])
        if len(data) != entry["size"]:
            raise ValueError("Uncompressed RFA length mismatch")
        return data


def audit_pe(path: Path, destination: Path):
    pe = pefile.PE(str(path), fast_load=False)
    data = path.read_bytes()
    base = pe.OPTIONAL_HEADER.ImageBase
    sections = [dict(name=s.Name.rstrip(b"\0").decode("ascii", "replace"), va=base+s.VirtualAddress,
        virtual_size=s.Misc_VirtualSize, raw_offset=s.PointerToRawData, raw_size=s.SizeOfRawData,
        executable=bool(s.Characteristics & 0x20000000)) for s in pe.sections]

    def offset_va(offset):
        for section in sections:
            if section["raw_offset"] <= offset < section["raw_offset"] + section["raw_size"]:
                return section["va"] + offset - section["raw_offset"]
        return None

    def va_offset(va):
        for section in sections:
            if section["va"] <= va < section["va"] + section["raw_size"]:
                return section["raw_offset"] + va - section["va"]
        return None

    strings = []
    for match in re.finditer(rb"[\x20-\x7e]{4,}\x00", data):
        va = offset_va(match.start())
        if va is not None:
            strings.append(dict(va=va, text=match.group()[:-1].decode("ascii")))
    string_by_va = {item["va"]: item["text"] for item in strings}
    paths = [s for s in strings if re.search(r"\.(cpp|h|c)$", s["text"], re.I) and "\\" in s["text"]]
    types = [s for s in strings if s["text"].startswith((".?AV", ".?AU"))]
    imports = [dict(library=desc.dll.decode(), functions=[item.name.decode() if item.name else f"ordinal:{item.ordinal}" for item in desc.imports]) for desc in getattr(pe, "DIRECTORY_ENTRY_IMPORT", [])]
    result = dict(path=str(path), sha256=sha256(path), bytes=len(data), machine=hex(pe.FILE_HEADER.Machine),
        image_base=hex(base), entry_point=hex(base+pe.OPTIONAL_HEADER.AddressOfEntryPoint), sections=sections,
        imports=imports, source_paths=len(paths), rtti_types=len(types), ascii_strings=len(strings),
        provenance="Unknown: local executable has no externally verified vanilla reference hash")
    write_json(destination / "pe.json", result)
    write_json(destination / "strings.json", strings)
    write_json(destination / "source-paths.json", paths)
    write_json(destination / "rtti-types.json", types)
    mode = capstone.CS_MODE_32 if pe.FILE_HEADER.Machine == 0x14c else capstone.CS_MODE_64
    decoder = capstone.Cs(capstone.CS_ARCH_X86, mode)
    decoder.detail = True

    # A descriptor pattern previously observed in this engine. These are registration
    # candidates, not proof of the handler's semantics or dynamic execution.
    commands = []
    for match in re.finditer(rb"\xc7\x46\x0c(.{4})", data, re.S):
        pointer = struct.unpack("<I", match.group(1))[0]
        name = string_by_va.get(pointer)
        if name and re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]{1,80}", name):
            commands.append(dict(name=name, registration_va=hex(offset_va(match.start()) or 0), string_va=hex(pointer)))
    write_json(destination / "command-registration-candidates.json", commands)

    terms = ("GPM_", "Soldier", "Projectile", "PlayerControl", "ControlPoint", "Conquest", "Network", "setMaxSpeed", "setReloadTime", "setFireRate", "setJump", "TimeToReSpawn")
    anchors = []
    snippets = []
    for item in strings:
        text = item["text"]
        if len(text) > 160 or not any(term.casefold() in text.casefold() for term in terms):
            continue
        needle = struct.pack("<I", item["va"])
        references = []
        start = 0
        while True:
            offset = data.find(needle, start)
            if offset < 0:
                break
            start = offset + 1
            va = offset_va(offset)
            if va is None:
                continue
            references.append(hex(va))
            # Decode only an explicit immediate instruction boundary, not a guessed
            # function start or a linear sweep that can enter jump-table data.
            instruction_start = offset - 1
            if instruction_start < 0 or data[instruction_start] not in [0x68, *range(0xB8, 0xC0)]:
                continue
            instruction_va = offset_va(instruction_start)
            if instruction_va is None:
                continue
            lines = []
            for instruction in decoder.disasm(data[instruction_start:instruction_start+144], instruction_va):
                annotation = ""
                for operand in instruction.operands:
                    if operand.type == capstone.x86.X86_OP_IMM:
                        target = string_by_va.get(operand.imm & 0xffffffff)
                        if target:
                            annotation = " ; " + repr(target)
                lines.append(f"{instruction.address:08x}  {instruction.bytes.hex(' '):24s} {instruction.mnemonic:8s} {instruction.op_str}{annotation}")
                if instruction.mnemonic.startswith("ret") or len(lines) >= 18:
                    break
            if lines:
                snippets.append(f"STRING {text!r} @ {item['va']:08x}; candidate immediate xref @ {instruction_va:08x}\n" + "\n".join(lines))
        anchors.append(dict(text=text, string_va=hex(item["va"]), candidate_pointer_references=references))
    write_json(destination / "gameplay-string-anchors.json", anchors)
    (destination / "gameplay-xrefs.asm").write_text("; Static candidates. Instruction alignment and semantics require further verification.\n\n" + "\n\n".join(snippets), encoding="utf-8")

    # Known entry from the previous local notes: only a candidate until verified.
    if path.name.casefold() == "bfvietnam.exe":
        offset = va_offset(0x442310)
        if offset is not None:
            lines = [f"{i.address:08x}  {i.bytes.hex(' '):24s} {i.mnemonic:8s} {i.op_str}" for i in decoder.disasm(data[offset:offset+0x350], 0x442310)]
            (destination / "game-mode-parser-candidate.asm").write_text("\n".join(lines), encoding="utf-8")
    return result


def archive_order(path: Path):
    match = re.match(r"(.*)_(\d{3})$", path.stem)
    return (str(path.parent).casefold(), (match.group(1) if match else path.stem).casefold(), int(match.group(2)) if match else 0)


def extract_selected(archive: Rfa, selected: list[dict], destination: Path, extractor: Path):
    destination.mkdir(parents=True, exist_ok=True)
    if not archive.compressed:
        for entry in selected:
            output = destination / safe_name(entry["name"])
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_bytes(archive.read(entry))
    elif selected:
        listing = destination.parent / (archive.path.stem + "-extract-list.txt")
        listing.write_text("\n".join(e["name"] for e in selected) + "\n", encoding="cp1252")
        completed = subprocess.run([str(extractor), str(archive.path), str(destination), "-l" + str(listing)], capture_output=True, timeout=180)
        (destination.parent / (archive.path.stem + "-extract.log")).write_bytes(completed.stdout + completed.stderr)
        if completed.returncode:
            raise RuntimeError(f"BGA extraction failed: {archive.path}; see extraction log")
    results = []
    for entry in selected:
        output = destination / entry["name"]
        if not output.is_file() or output.stat().st_size != entry["size"]:
            raise RuntimeError(f"Extraction verification failed: {entry['name']}")
        results.append(dict(name=entry["name"], bytes=entry["size"], sha256=sha256(output)))
    return results


def collect_config(root: Path):
    templates = []
    values = []
    for path in sorted(root.rglob("*.con")):
        text = path.read_text(encoding="cp1252", errors="replace")
        current = None
        in_comment = False
        for number, line in enumerate(text.splitlines(), 1):
            line = line.strip()
            lower = line.casefold()
            if lower == "beginrem": in_comment = True; continue
            if lower == "endrem": in_comment = False; continue
            if in_comment or not line or lower.startswith("rem "):
                continue
            created = re.match(r"ObjectTemplate\.create\s+(\S+)\s+(\S+)", line, re.I)
            if created:
                current = dict(kind=created.group(1), name=created.group(2), source=str(path.relative_to(root)), line=number, properties=[])
                templates.append(current)
            elif re.match(r"ObjectTemplate\.active\s", line, re.I):
                # Do not falsely attribute an active-template edit to the last creation.
                current = None
            elif current and lower.startswith("objecttemplate."):
                current["properties"].append(dict(command=line, line=number))
            if re.search(r"(?:\.setFireRate|\.setReloadTime|\.setMaxSpeed|\.setAmmo|\.damage|\.velocity|\.gravity|\.setRespawn|\.timeToReSpawn|\.capture|\.ticket|\.terrainSize|\.scale)", line, re.I):
                values.append(dict(source=str(path.relative_to(root)), line=number, command=line))
    return templates, values


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--game-root", type=Path, default=PROJECT.parent)
    parser.add_argument("--output", type=Path, default=PROJECT.parent / "bfv-reference-local")
    parser.add_argument("--extract", action="store_true")
    parser.add_argument("--map", default="Ia_Drang")
    parser.add_argument("--skip-binary", action="store_true")
    args = parser.parse_args()
    game, output = args.game_root.resolve(), args.output.resolve()
    if output == PROJECT or PROJECT in output.parents or output == game or output in game.parents:
        parser.error("Derived reference output must be a separate directory outside the source project and cannot contain the game install")
    output.mkdir(parents=True, exist_ok=True)
    binaries = []
    if not args.skip_binary:
        for name in ("BfVietnam.exe", "bfvietnam_w32ded.exe"):
            binaries.append(audit_pe(game / name, output / "binary" / Path(name).stem))
    archive_root = game / "Mods/BfVietnam/Archives"
    archives = []
    selected_archives = []
    for path in sorted(archive_root.rglob("*.rfa"), key=archive_order):
        source = path
        backup = game / "BACKUP_original_RFA" / path.name
        if path.name.casefold() in ("game.rfa", "objects.rfa") and backup.exists():
            source = backup
        archive = Rfa(source)
        metadata = dict(installed_path=str(path.relative_to(game)), reference_path=str(source), sha256=sha256(source),
            installed_sha256=sha256(path) if source != path else None,
            provenance="Local backup labelled original; not verified against an official hash" if source != path else "Installed archive; vanilla provenance unverified",
            compressed=bool(archive.compressed), entries=archive.entries,
            extensions=dict(Counter(Path(e["name"]).suffix.casefold() for e in archive.entries)))
        archives.append(metadata)
        is_level = "levels" in [part.casefold() for part in path.parts]
        base_name = re.sub(r"_\d{3}$", "", path.stem).casefold()
        if not is_level and path.stem.casefold() in ("objects", "game", "effects", "ai") or is_level and base_name == args.map.casefold():
            selected_archives.append((archive, metadata, is_level))
    write_json(output / "archives.json", archives)
    extracted = []
    if args.extract:
        extractor = game / "universal-modder-main/_tools/rfaUnpack.exe"
        if not extractor.is_file():
            parser.error("Existing BGA rfaUnpack.exe is missing")
        for archive, metadata, is_level in selected_archives:
            selected = [e for e in archive.entries if Path(e["name"]).suffix.casefold() in ((".con", ".raw", ".dds", ".tga") if is_level else (".con",))]
            extracted.append(dict(archive=metadata["reference_path"], files=extract_selected(archive, selected, output / "extracted", extractor)))
        write_json(output / "extracted-manifest.json", extracted)
        templates, values = collect_config(output / "extracted")
        write_json(output / "object-templates.json", templates)
        write_json(output / "gameplay-config-values.json", values)
    summary = dict(reference_target="Unmodified Battlefield Vietnam; exact stock version/provenance still requires verification",
        map=args.map, binary_summaries=binaries, archives=len(archives), entries=sum(len(a["entries"]) for a in archives),
        extracted_files=sum(len(a["files"]) for a in extracted),
        unresolved=["Executable and most installed archives lack externally verified stock hashes", "A full engine reimplementation has not been completed", "Static disassembly candidates require control-flow and runtime verification", "The procedural Bevy prototype is not a parity reference"])
    write_json(output / "summary.json", summary)
    print(json.dumps({k: v for k, v in summary.items() if k != "binary_summaries"}, indent=2))


if __name__ == "__main__":
    main()
