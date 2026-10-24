// symscan: find every absolute reference into the image (the executables carry no
// relocation table), the first step toward relinking the image from source.
//
//   symscan <image.exe> <all-functions.tsv> <out-prefix>
//
// Writes <out-prefix>-refs.tsv and <out-prefix>-tables.tsv.
//
// Tables: switch tables MSVC places inside .text. `jmp dword ptr [r*4 + T]` makes T
// a table of code addresses; `movzx r, byte ptr [r + B]` feeding such a jump makes
// B a byte index table. A table ends at the next table, the next index function, or
// the first entry that is not one (code address inside .text / byte below the
// jump table's entry count).
//
// Code: a linear sweep of .text with the tables skipped (as static_recomp.py's
// emitter does once it reads the tables); every 4-byte immediate or displacement
// whose value lies inside the image is a candidate.
// Data: every aligned 4-byte word of the other sections whose value lies inside
// the image, and every jump table entry. PE structures (data directories) hold
// RVAs, not addresses, and are reported apart.
//
// Each candidate gets a decision and the rule behind it:
//   refs:   where kind value target_section target_class decision rule context
//   kind:   disp | imm | word | table | pe | name (overlaps an import DLL or
//           hint/name string; always a constant)
//   target_class: func (index function start) | insn (instruction boundary)
//                 | code_mid (inside an instruction) | table | data | header
//   decision: address | constant | review
#include <capstone/capstone.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32;

struct Section { std::string name; u32 va, vsize, raw, rawsize; };
struct Ref { u32 where; std::string kind; u32 value; std::string context; };
struct Table { u32 start, end; std::string kind; u32 jump; };

static std::vector<u8> img;
static u32 rd32(size_t o) { u32 v; memcpy(&v, &img[o], 4); return v; }
static u16 rd16(size_t o) { u16 v; memcpy(&v, &img[o], 2); return v; }
static bool printable(u32 b) { return (b >= 0x20 && b < 0x7f) || b == 9 || b == 10 || b == 13; }

int main(int argc, char **argv) {
    if (argc != 4) { fprintf(stderr, "usage: symscan <image.exe> <all-functions.tsv> <out-prefix>\n"); return 2; }
    {
        std::ifstream in(argv[1], std::ios::binary);
        img.assign((std::istreambuf_iterator<char>(in)), {});
    }
    if (img.size() < 0x400) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
    u32 pe = rd32(0x3c);
    u16 nsec = rd16(pe + 6), opt = rd16(pe + 20);
    u32 oh = pe + 24, base = rd32(oh + 28), image_size = rd32(oh + 56);
    u32 lo = base, hi = base + image_size;
    std::vector<Section> secs;
    for (u16 i = 0; i < nsec; i++) {
        size_t s = oh + opt + i * 40;
        char name[9] = {}; memcpy(name, &img[s], 8);
        secs.push_back({name, base + rd32(s + 12), rd32(s + 8), rd32(s + 20), rd32(s + 16)});
    }
    auto section_of = [&](u32 va) -> const Section * {
        for (auto &s : secs) if (va >= s.va && va < s.va + std::max(s.vsize, s.rawsize)) return &s;
        return nullptr;
    };
    std::vector<std::pair<u32, u32>> dirs; std::vector<int> dir_ids;
    const char *dir_names[16] = {"export", "import", "resource", "exception", "security", "basereloc", "debug",
                                 "architecture", "globalptr", "tls", "load_config", "bound_import", "iat",
                                 "delay_import", "com", "reserved"};
    for (int d = 0; d < 16; d++) {
        u32 rva = rd32(oh + 96 + d * 8), size = rd32(oh + 96 + d * 8 + 4);
        if (rva && size && d != 4) { dirs.push_back({base + rva, base + rva + size}); dir_ids.push_back(d); }
    }
    auto in_dir = [&](u32 va) -> int {
        for (size_t i = 0; i < dirs.size(); i++) if (va >= dirs[i].first && va < dirs[i].second) return dir_ids[i];
        return -1;
    };
    // import DLL names and hint/name entries: text outside every data directory,
    // whose bytes can look like an address ("s\0`\0" after "ExitProcess")
    std::map<u32, u32> names;  // [start, end) VAs
    auto file_of = [&](u32 va) -> long {
        const Section *s = section_of(va);
        return s && va - s->va < s->rawsize ? (long)(s->raw + (va - s->va)) : -1;
    };
    auto add_name = [&](u32 va, u32 skip) {
        long o = file_of(va);
        if (o < 0) return;
        size_t e = o + skip;
        while (e < img.size() && img[e]) e++;
        names[va] = va + (u32)(e + 1 - o);
    };
    if (u32 rva = rd32(oh + 96 + 8))
        for (u32 d = base + rva; file_of(d) >= 0 && rd32(file_of(d) + 12); d += 20) {
            long o = file_of(d);
            add_name(base + rd32(o + 12), 0);
            u32 thunks = rd32(o) ? rd32(o) : rd32(o + 16);
            for (u32 t = base + thunks; file_of(t) >= 0; t += 4) {
                u32 v = rd32(file_of(t));
                if (!v) break;
                if (!(v & 0x80000000)) add_name(base + v, 2);
            }
        }
    auto in_names = [&](u32 va) {  // does [va, va + 4) touch a name?
        auto it = names.upper_bound(va + 3);
        return it != names.begin() && std::prev(it)->second > va;
    };
    std::set<u32> funcs;
    {
        std::ifstream f(argv[2]); std::string line; std::getline(f, line);
        while (std::getline(f, line)) if (!line.empty()) funcs.insert((u32)strtoul(line.c_str(), nullptr, 16));
    }
    const Section *text = nullptr;
    for (auto &s : secs) if (s.name == ".text") text = &s;
    if (!text) { fprintf(stderr, "no .text\n"); return 1; }
    const u32 tva = text->va, tend = text->va + text->rawsize;
    auto in_text = [&](u32 v) { return v >= tva && v < tend; };

    csh cs; cs_open(CS_ARCH_X86, CS_MODE_32, &cs); cs_option(cs, CS_OPT_DETAIL, CS_OPT_ON);
    cs_insn *insn = cs_malloc(cs);

    // sweep .text, skipping [start, end) regions; calls visit(insn) per instruction
    auto sweep = [&](const std::map<u32, u32> &skip, auto visit) {
        u32 off = 0, size = text->rawsize;
        auto next_skip = skip.begin();
        while (off < size) {
            u32 va = tva + off;
            while (next_skip != skip.end() && next_skip->second <= va) ++next_skip;
            if (next_skip != skip.end() && next_skip->first <= va) { off = next_skip->second - tva; continue; }
            u32 limit = next_skip != skip.end() ? std::min(size, next_skip->first - tva) : size;
            const u8 *p = &img[text->raw + off]; size_t left = limit - off; uint64_t addr = va;
            if (!cs_disasm_iter(cs, &p, &left, &addr, insn)) { visit(nullptr, va); off++; continue; }
            visit(insn, va);
            off += insn->size;
        }
    };

    // pass 1: switch tables, swept again with the tables found so far carved out
    // until no new one appears (a table decoded as code can hide the next jump)
    std::map<u32, u32> jump_tables;   // start -> jump instruction
    std::map<u32, u32> byte_tables;   // start -> jump table it indexes
    std::vector<Table> tables; std::map<u32, u32> carve;
    for (size_t known = (size_t)-1; known != jump_tables.size() + byte_tables.size();) {
        known = jump_tables.size() + byte_tables.size();
        u32 last_movzx_table = 0, last_movzx_at = 0; x86_reg last_movzx_dst = X86_REG_INVALID;
        sweep(carve, [&](cs_insn *i, u32 va) {
            if (!i) return;
            cs_x86 &x = i->detail->x86;
            if (i->id == X86_INS_MOVZX && x.op_count == 2 && x.operands[1].type == X86_OP_MEM &&
                x.operands[1].size == 1 && x.operands[1].mem.base != X86_REG_INVALID &&
                x.operands[1].mem.index == X86_REG_INVALID && in_text((u32)x.operands[1].mem.disp)) {
                last_movzx_table = (u32)x.operands[1].mem.disp; last_movzx_at = va; last_movzx_dst = x.operands[0].reg;
            }
            if (i->id == X86_INS_JMP && x.op_count == 1 && x.operands[0].type == X86_OP_MEM &&
                x.operands[0].mem.base == X86_REG_INVALID && x.operands[0].mem.scale == 4 &&
                x.operands[0].mem.index != X86_REG_INVALID && in_text((u32)x.operands[0].mem.disp)) {
                u32 t = (u32)x.operands[0].mem.disp;
                jump_tables.emplace(t, va);
                if (last_movzx_table && va - last_movzx_at <= 16 && x.operands[0].mem.index == last_movzx_dst)
                    byte_tables.emplace(last_movzx_table, t);
                last_movzx_table = 0;
            }
        });
    // extents: a table ends at the next table or index function start, or at the
    // first entry that is not one
    std::set<u32> fences(funcs.begin(), funcs.end());
    for (auto &t : jump_tables) fences.insert(t.first);
    for (auto &t : byte_tables) fences.insert(t.first);
    tables.clear(); carve.clear();
    std::map<u32, u32> entries_of;
    for (auto &t : jump_tables) {
        u32 start = t.first;
        // an index that starts at 1 (the CRT memcpy) puts the displacement one entry early
        if (!in_text(rd32(text->raw + (start - tva))) && in_text(rd32(text->raw + (start + 4 - tva)))) start += 4;
        auto f = fences.upper_bound(start);
        u32 fence = f == fences.end() ? tend : std::min(tend, *f);
        u32 a = start;
        while (a + 4 <= fence && in_text(rd32(text->raw + (a - tva)))) a += 4;
        if (a == start) continue;
        entries_of[t.first] = (a - start) / 4;
        tables.push_back({start, a, "jump", t.second});
    }
    for (auto &t : byte_tables) {
        if (!entries_of.count(t.second)) continue;
        auto f = fences.upper_bound(t.first);
        u32 fence = f == fences.end() ? tend : std::min(tend, *f);
        u32 a = t.first, n = entries_of[t.second];
        while (a < fence && img[text->raw + (a - tva)] < n) a++;
        if (a == t.first) continue;
        tables.push_back({t.first, a, "byte", t.second});
    }
    std::sort(tables.begin(), tables.end(), [](const Table &a, const Table &b) { return a.start < b.start; });
    for (size_t k = 0; k + 1 < tables.size(); k++)  // overlapping guesses: the earlier one stops
        if (tables[k].end > tables[k + 1].start) tables[k].end = tables[k + 1].start;
    for (auto &t : tables) if (t.end > t.start) carve[t.start] = t.end;
    }
    auto table_at = [&](u32 v) -> const Table * {
        auto it = carve.upper_bound(v);
        if (it == carve.begin()) return nullptr;
        --it;
        if (v >= it->second) return nullptr;
        for (auto &t : tables) if (t.start == it->first) return &t;
        return nullptr;
    };

    // pass 2: references from code, tables skipped
    std::vector<u8> starts(text->rawsize, 0);
    std::vector<Ref> refs;
    sweep(carve, [&](cs_insn *i, u32 va) {
        starts[va - tva] = 1;
        if (!i) return;
        cs_x86 &x = i->detail->x86;
        u32 off = text->raw + (va - tva);
        std::string ctx = std::string(i->mnemonic) + " " + i->op_str;
        if (x.encoding.disp_offset && x.encoding.disp_size == 4) {
            u32 v = rd32(off + x.encoding.disp_offset);
            if (v >= lo && v < hi) refs.push_back({va + x.encoding.disp_offset, "disp", v, ctx});
        }
        if (x.encoding.imm_offset && x.encoding.imm_size == 4) {
            bool relative = false;  // call/jmp/jcc rel32 are already symbolic
            for (int g = 0; g < i->detail->groups_count; g++)
                if (i->detail->groups[g] == CS_GRP_BRANCH_RELATIVE) relative = true;
            u32 v = rd32(off + x.encoding.imm_offset);
            if (!relative && v >= lo && v < hi) refs.push_back({va + x.encoding.imm_offset, "imm", v, ctx});
        }
    });
    for (auto &t : tables)
        if (t.kind == "jump")
            for (u32 a = t.start; a < t.end; a += 4) refs.push_back({a, "table", rd32(text->raw + (a - tva)), "jump table"});
    for (auto &s : secs) {
        if (&s == text) continue;
        for (u32 o = 0; o + 4 <= s.rawsize; o += 4) {
            u32 v = rd32(s.raw + o), where = s.va + o;
            int d = in_dir(where);
            if (d >= 0) {
                if ((v >= lo && v < hi) || (v && v < image_size)) refs.push_back({where, "pe", v, dir_names[d]});
                continue;
            }
            if (v >= lo && v < hi && in_names(where)) {
                refs.push_back({where, "name", v, s.name});
            } else if (v >= lo && v < hi) {
                // string text: three printable bytes and a NUL that continue a printable
                // run (an aligned pointer follows a pointer's zero top byte instead)
                u32 run = 0;
                while (run < 4 && o > run && printable(img[s.raw + o - 1 - run])) run++;
                bool text_shaped = printable(v & 0xff) && printable(v >> 8 & 0xff) && printable(v >> 16 & 0xff);
                refs.push_back({where, text_shaped && run >= 4 ? "text" : "word", v, s.name});
            }
        }
    }

    auto target_class = [&](u32 v) -> std::string {
        if (in_text(v)) {
            if (table_at(v)) return "table";
            if (funcs.count(v)) return "func";
            return starts[v - tva] ? "insn" : "code_mid";
        }
        if (v < tva) return "header";
        return "data";
    };
    std::set<u32> code_pointers;  // code addresses pushed, moved or stored in data
    for (auto &r : refs) {
        std::string cls = target_class(r.value), m = r.context.substr(0, r.context.find(' '));
        if ((cls == "func" || cls == "insn") &&
            ((r.kind == "imm" && (m == "push" || m == "mov")) || r.kind == "word"))
            code_pointers.insert(r.value);
    }
    auto decide = [&](const Ref &r, const std::string &cls, std::string &rule) -> const char * {
        std::string m = r.context.substr(0, r.context.find(' '));
        if (r.kind == "pe") { rule = "pe_structure"; return "review"; }
        if (r.kind == "text") { rule = "string_text"; return "constant"; }
        if (r.kind == "name") { rule = "import_name_text"; return "constant"; }
        if (cls == "header") { rule = "header_unmoved"; return "constant"; }  // headers stay at the image base
        if (r.kind == "table") {
            if (cls == "func" || cls == "insn") { rule = "jump_table_entry"; return "address"; }
            rule = "jump_table_mid"; return "review";
        }
        if (r.kind == "disp") {
            if (cls == "data") { rule = "memory_operand"; return "address"; }
            if (cls == "table") { rule = "table_operand"; return "address"; }
            for (int k = -16; k <= 16; k += 4)  // the index starts above or below zero
                if (k && table_at(r.value + k)) { rule = "table_operand_biased"; return "address"; }
            if (m == "lea") { rule = "lea_arithmetic"; return "constant"; }
            rule = "memory_operand_into_code"; return "review";
        }
        if (r.kind == "imm") {
            if (cls == "data") { rule = "immediate_data"; return "address"; }
            if (cls == "table") { rule = "immediate_table"; return "address"; }
            if (cls == "func" || cls == "insn") {
                if (m == "push" || m == "mov") { rule = cls == "func" ? "code_pointer_func" : "code_pointer_insn"; return "address"; }
                // `setcc; dec; and r, F` is the branchless `cond ? F : 0` (zlib's checkfn)
                if (m == "and" && cls == "func") { rule = "select_code_pointer"; return "address"; }
                // comparing a stored pointer with a value the code also stores as a pointer
                if (m == "cmp" && code_pointers.count(r.value)) { rule = "compared_code_pointer"; return "address"; }
                rule = "arithmetic_on_code_value"; return "constant";
            }
            rule = "inside_instruction"; return "constant";
        }
        // data words
        if (cls == "data") { rule = "data_pointer"; return "address"; }
        if (cls == "func") { rule = "data_code_pointer_func"; return "address"; }
        if (cls == "insn" || cls == "table") { rule = "data_code_pointer_insn"; return "address"; }
        rule = "data_inside_instruction"; return "constant";  // no instruction starts there
    };

    std::string prefix = argv[3];
    FILE *out = fopen((prefix + "-refs.tsv").c_str(), "wb");
    if (!out) { fprintf(stderr, "cannot write %s-refs.tsv\n", argv[3]); return 1; }
    fprintf(out, "where\tkind\tvalue\ttarget_section\ttarget_class\tdecision\trule\tcontext\n");
    std::map<std::string, size_t> counts;
    for (auto &r : refs) {
        const Section *t = section_of(r.value);
        std::string cls = target_class(r.value), rule;
        const char *d = decide(r, cls, rule);
        fprintf(out, "%08x\t%s\t%08x\t%s\t%s\t%s\t%s\t%s\n", r.where, r.kind.c_str(), r.value,
                t ? t->name.c_str() : "-", cls.c_str(), d, rule.c_str(), r.context.c_str());
        counts[std::string(d) + "\t" + rule]++;
    }
    fclose(out);
    FILE *tf = fopen((prefix + "-tables.tsv").c_str(), "wb");
    fprintf(tf, "start\tend\tkind\tjump\n");
    size_t jt = 0, bt = 0;
    for (auto &t : tables) if (t.end > t.start) {
        fprintf(tf, "%08x\t%08x\t%s\t%08x\n", t.start, t.end, t.kind.c_str(), t.jump);
        (t.kind == "jump" ? jt : bt)++;
    }
    fclose(tf);
    printf("tables: %zu jump, %zu byte\n", jt, bt);
    for (auto &c : counts) printf("%8zu  %s\n", c.second, c.first.c_str());
    cs_free(insn, 1); cs_close(&cs);
    return 0;
}
