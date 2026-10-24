// textcheck: proves that an edited .text is the original .text with the code
// moved, instruction by instruction, plus the edits and nothing else.
//
//   textcheck.exe A.bin va_a B.bin va_b items.tsv ranges.tsv image_base out_ranges.tsv
//
// A is the original .text at va_a, B the edited one at va_b. items.tsv lists
// the original's items in order, "offset size kind" (hex offset, decimal size):
//   code  an instruction (or a detour: a jmp and padding) written as text
//   hold  an instruction written as bytes (.byte, with .long symbol fields)
//   data  bytes that are not decoded (undecodable, merged runs, index tables)
//   jt    a switch jump table (.long labels)
//   edit  "offset oldsize edit newsize": the old bytes [offset, offset+oldsize)
//         became newsize new bytes; references to offset now reach the new bytes
// ranges.tsv lines "start end delta" (hex VAs): the other sections and how far
// each moved.
//
// Pass 1 pairs every old item with the new bytes at the running position
// (code by decoding B, so a short branch that grew to a near one is found).
// Pass 2 checks each pair:
//   code       same instruction: a relative branch or call has the same
//              mnemonic and its target moved to where that code went; other
//              instructions have the same length and bytes, except 32-bit
//              displacement and immediate fields that hold an address, which
//              must have moved with it;
//   data/hold  same bytes, except 32-bit fields that moved with their target
//              (held bytes are unreached by any L1 path; ones that decode as
//              a relative branch keep their rel field and are counted);
//   jt         every entry moved with its target.
// A 32-bit field that keeps a value that lies in moved code or data is counted
// ("unmoved", a constant by symscan's decision) and listed, not failed.
//
// out_ranges.tsv: the text pieces that moved by one delta, as VA and RVA lines
// for relocheck --ranges over the other sections. Exit status 1 when any item
// does not match.
#include <capstone/capstone.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

typedef uint32_t u32;

static std::vector<uint8_t> load(const char *path) {
    std::vector<uint8_t> data;
    FILE *f = std::fopen(path, "rb");
    if (!f) { std::fprintf(stderr, "cannot open %s\n", path); std::exit(2); }
    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    data.resize(size);
    if (size && std::fread(data.data(), 1, size, f) != (size_t)size) { std::fprintf(stderr, "short read %s\n", path); std::exit(2); }
    std::fclose(f);
    return data;
}

static u32 le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (u32)p[3] << 24; }

enum Kind { CODE, HOLD, DATA, JT, EDIT };
struct Item { u32 off, size; Kind kind; u32 newsize; u32 pos = 0, ref = 0; };
struct Range { u32 start, end, delta; };

static std::vector<uint8_t> A, B;
static u32 va_a, va_b;
static std::vector<Item> items;
static std::vector<u32> starts;
static std::vector<Range> others;
static csh cs;
static size_t unexplained_items = 0, unmoved = 0, fields_moved = 0, branches_moved = 0, held_branches = 0, code_as_bytes = 0;

static void fail(const Item &it, const char *why) {
    if (++unexplained_items <= 20)
        std::printf("MISMATCH %08x (%s): %s\n", va_a + it.off,
                    it.kind == CODE ? "code" : it.kind == HOLD ? "hold" : it.kind == DATA ? "data" : "jt", why);
}

// where an original address went; false when it cannot be said (inside an
// edit, or inside an item whose length changed)
static bool map_va(u32 x, u32 &out) {
    if (x >= va_a && x < va_a + A.size()) {
        u32 off = x - va_a;
        size_t i = std::upper_bound(starts.begin(), starts.end(), off) - starts.begin() - 1;
        const Item &it = items[i];
        if (off == it.off) { out = va_b + it.ref; return true; }
        if (it.kind == EDIT) return false;
        u32 newsize = i + 1 < items.size() ? items[i + 1].pos - it.pos : it.size;
        if (newsize != it.size) return false;
        out = va_b + it.pos + (off - it.off);
        return true;
    }
    if (x == va_a + A.size()) { out = va_b + items.back().pos + items.back().size; return true; }
    for (const Range &r : others)
        if (r.start <= x && x < r.end) { out = x + r.delta; return true; }
    out = x;
    return true;
}

static const u32 kUnmovedListed = 10;

// a 32-bit field: the same value, or an address that moved with its target
static bool field_ok(u32 a, u32 b, u32 where) {
    u32 m;
    bool known = map_va(a, m);
    if (known && b == m) { if (m != a) ++fields_moved; return true; }
    if (b == a) {
        if (++unmoved <= kUnmovedListed)
            std::printf("unmoved value 0x%08x at %08x (would be 0x%08x as an address)\n", a, where, known ? m : 0);
        return true;
    }
    return false;
}

// bytes equal except 32-bit fields that moved with their target (relocheck's scan)
static bool bytes_ok(const uint8_t *a, const uint8_t *b, u32 n, u32 where) {
    u32 i = 0;
    while (i < n) {
        if (a[i] == b[i]) { ++i; continue; }
        bool found = false;
        for (u32 back = 0; back <= 3 && back <= i && !found; ++back) {
            u32 j = i - back;
            if (j + 4 > n) continue;
            bool same = true;
            for (u32 k = j; k < i; ++k) same &= a[k] == b[k];
            u32 x = le32(a + j), y = le32(b + j), m;
            if (same && x != y && map_va(x, m) && m == y) { ++fields_moved; i = j + 4; found = true; }
        }
        if (!found) return false;
    }
    (void)where;
    return true;
}

static bool relative(cs_insn *i) {
    for (int g = 0; g < i->detail->groups_count; ++g)
        if (i->detail->groups[g] == CS_GRP_BRANCH_RELATIVE) return true;
    return false;
}

static bool decode(const std::vector<uint8_t> &d, u32 off, u32 limit, u32 va, cs_insn *insn) {
    const uint8_t *p = d.data() + off;
    size_t left = std::min<size_t>(limit, d.size() - off);
    uint64_t address = va;
    return cs_disasm_iter(cs, &p, &left, &address, insn);
}

static const char *check_insn(const Item &it, u32 newsize, cs_insn *x, cs_insn *y) {
    static char why[96];
    const uint8_t *a = A.data() + it.off, *b = B.data() + it.pos;
    if (!decode(A, it.off, it.size, va_a + it.off, x)) {
        // a held item that does not decode: compare as data
        if (newsize != it.size || !bytes_ok(a, b, it.size, va_a + it.off)) return "undecodable bytes differ";
        return nullptr;
    }
    if (!decode(B, it.pos, newsize, va_b + it.pos, y)) { return "new bytes do not decode"; }
    u32 rest_a = it.size - x->size, rest_b = newsize - y->size;
    if (rest_a != rest_b || std::memcmp(a + x->size, b + y->size, rest_a)) { return "bytes after the instruction differ"; }
    if (relative(x)) {
        if (x->id != y->id || !relative(y)) { return "relative branch became another instruction"; }
        cs_x86 &dx = x->detail->x86, &dy = y->detail->x86;
        if (dx.op_count != 1 || dy.op_count != 1 || dx.operands[0].type != X86_OP_IMM) {
            return "relative branch with unexpected operands";
        }
        u32 ta = (u32)dx.operands[0].imm, tb = (u32)dy.operands[0].imm, m;
        if (!map_va(ta, m) || m != tb) {
            std::snprintf(why, sizeof why, "%s target %08x is %08x, should be %08x", x->mnemonic, ta, tb, m);
            return why;
        }
        if (ta != tb || x->size != y->size || va_a + it.off != va_b + it.pos) ++branches_moved;
        if (x->size != y->size) {
            // only the branch's own form may change (short to near), prefixes stay
            if (dx.prefix[0] != dy.prefix[0] || dx.prefix[1] != dy.prefix[1]) return "branch prefixes differ";
        }
        return nullptr;
    }
    if (x->id != y->id || x->size != y->size) { return "different instruction"; }
    cs_x86 &dx = x->detail->x86;
    std::vector<char> field(x->size, 0);  // 0 compare bytes, 1 disp32, 2 imm32
    if (dx.encoding.disp_size == 4) std::memset(field.data() + dx.encoding.disp_offset, 1, 4);
    if (dx.encoding.imm_size == 4) std::memset(field.data() + dx.encoding.imm_offset, 2, 4);
    for (u32 k = 0; k < x->size; ++k) {
        if (field[k]) {
            if (!field_ok(le32(a + k), le32(b + k), va_a + it.off + k)) { return "address field did not move with its target"; }
            k += 3;
        } else if (a[k] != b[k]) {
            return "instruction bytes differ outside its address fields";
        }
    }
    return nullptr;
}

int main(int argc, char **argv) {
    if (argc != 9) {
        std::fprintf(stderr, "usage: textcheck A.bin va_a B.bin va_b items.tsv ranges.tsv image_base out_ranges.tsv\n");
        return 2;
    }
    A = load(argv[1]);
    va_a = (u32)std::strtoul(argv[2], nullptr, 16);
    B = load(argv[3]);
    va_b = (u32)std::strtoul(argv[4], nullptr, 16);
    u32 image_base = (u32)std::strtoul(argv[7], nullptr, 16);
    {
        FILE *f = std::fopen(argv[5], "r");
        if (!f) { std::fprintf(stderr, "cannot open %s\n", argv[5]); return 2; }
        char kind[16];
        unsigned long off, size, extra;
        while (std::fscanf(f, "%lx %lu %15s", &off, &size, kind) == 3) {
            Item it{(u32)off, (u32)size, CODE, (u32)size};
            if (!std::strcmp(kind, "hold")) it.kind = HOLD;
            else if (!std::strcmp(kind, "data")) it.kind = DATA;
            else if (!std::strcmp(kind, "jt")) it.kind = JT;
            else if (!std::strcmp(kind, "edit")) {
                it.kind = EDIT;
                if (std::fscanf(f, "%lu", &extra) != 1) { std::fprintf(stderr, "edit without a new size\n"); return 2; }
                it.newsize = (u32)extra;
            } else if (std::strcmp(kind, "code")) { std::fprintf(stderr, "unknown kind %s\n", kind); return 2; }
            items.push_back(it);
        }
        std::fclose(f);
        f = std::fopen(argv[6], "r");
        if (!f) { std::fprintf(stderr, "cannot open %s\n", argv[6]); return 2; }
        unsigned long start, end, delta;
        while (std::fscanf(f, "%lx %lx %lx", &start, &end, &delta) == 3)
            others.push_back({(u32)start, (u32)end, (u32)delta});
        std::fclose(f);
    }
    if (cs_open(CS_ARCH_X86, CS_MODE_32, &cs) != CS_ERR_OK) { std::fprintf(stderr, "capstone\n"); return 2; }
    cs_option(cs, CS_OPT_DETAIL, CS_OPT_ON);
    cs_insn *x = cs_malloc(cs), *y = cs_malloc(cs);

    // pass 1: pair each old item with its new position; an edit's old items are skipped
    u32 p = 0, covered = 0;
    std::vector<Item> kept;
    for (size_t i = 0; i < items.size(); ++i) {
        Item it = items[i];
        if (it.kind == EDIT) {
            it.pos = it.ref = p;
            p += it.newsize;
            covered = it.off + it.size;
            kept.push_back(it);
            continue;
        }
        if (it.off < covered) continue;  // replaced by the edit before it
        it.pos = p;
        it.ref = (!kept.empty() && kept.back().kind == EDIT && kept.back().off == it.off) ? kept.back().ref : p;
        u32 newsize = it.size;
        if (it.kind == CODE) {
            if (p >= B.size()) { std::printf("new .text ends at old %08x\n", va_a + it.off); return 1; }
            bool da = decode(A, it.off, it.size, va_a + it.off, x);
            if (da && relative(x) && decode(B, p, 16, va_b + p, y) && relative(y))
                newsize = it.size - x->size + y->size;  // a branch may change form
        }
        p += newsize;
        kept.push_back(it);
    }
    items.swap(kept);
    starts.clear();
    for (const Item &it : items) starts.push_back(it.off);
    if (p > B.size()) { std::printf("new .text is %zu bytes, items need %u\n", B.size(), p); return 1; }
    for (u32 k = p; k < B.size(); ++k)
        if (B[k]) { std::printf("non-zero byte after the last item at +0x%x\n", k); return 1; }

    // pass 2: compare
    size_t edits = 0;
    for (size_t i = 0; i < items.size(); ++i) {
        const Item &it = items[i];
        u32 newsize = (i + 1 < items.size() ? items[i + 1].pos : p) - it.pos;
        if (it.kind == EDIT) {
            ++edits;
            std::printf("edit at %08x: %u old byte(s) -> %u new at %08x:", va_a + it.off, it.size, it.newsize, va_b + it.pos);
            u32 q = it.pos;
            while (q < it.pos + it.newsize) {
                if (!decode(B, q, it.pos + it.newsize - q, va_b + q, y)) { std::printf(" [undecodable]"); fail(it, "edit bytes do not decode"); break; }
                std::printf(" %s %s;", y->mnemonic, y->op_str);
                q += y->size;
            }
            std::printf("\n");
            continue;
        }
        if (it.kind == CODE) {
            const char *why = check_insn(it, newsize, x, y);
            // dead bytes after a detour decode across a symbol field (.long inside the
            // decoded "instruction"): they pass when they are the same bytes with moved fields
            if (why && newsize == it.size && bytes_ok(A.data() + it.off, B.data() + it.pos, it.size, va_a + it.off))
                ++code_as_bytes;
            else if (why)
                fail(it, why);
            continue;
        }
        if (newsize != it.size) { fail(it, "size changed"); continue; }
        if (it.kind == HOLD && decode(A, it.off, it.size, va_a + it.off, x) && relative(x)) {
            // held bytes keep their rel field; no L1 path reaches them (data inside functions)
            u32 m, t = (u32)x->detail->x86.operands[0].imm;
            if (!map_va(t, m) || m != t + (va_b + it.pos) - (va_a + it.off)) ++held_branches;
        }
        if (it.kind == JT) {
            for (u32 k = 0; k + 4 <= it.size; k += 4) {
                u32 m;
                if (!map_va(le32(A.data() + it.off + k), m) || m != le32(B.data() + it.pos + k)) { fail(it, "jump table entry did not move with its code"); break; }
                ++fields_moved;
            }
            continue;
        }
        if (!bytes_ok(A.data() + it.off, B.data() + it.pos, it.size, va_a + it.off)) fail(it, "bytes differ");
    }

    // pieces of .text that moved by one delta
    FILE *out = std::fopen(argv[8], "w");
    if (!out) { std::fprintf(stderr, "cannot write %s\n", argv[8]); return 2; }
    size_t pieces = 0;
    for (size_t i = 0; i < items.size();) {
        if (items[i].kind == EDIT) { ++i; continue; }
        if (items[i].ref != items[i].pos) {  // a reference to an edited address reaches the edit
            u32 at = va_a + items[i].off, d = va_b + items[i].ref - at;
            std::fprintf(out, "%x %x %x\n%x %x %x\n", at, at + 1, d, at - image_base, at + 1 - image_base, d);
        }
        u32 delta = items[i].pos - items[i].off;
        size_t j = i + 1;
        while (j < items.size() && items[j].kind != EDIT && items[j].pos - items[j].off == delta) ++j;
        u32 start = items[i].off, end = j < items.size() ? items[j].off : (u32)A.size();
        u32 d = va_b - va_a + delta;
        std::fprintf(out, "%x %x %x\n", va_a + start, va_a + end, d);
        std::fprintf(out, "%x %x %x\n", va_a + start - image_base, va_a + end - image_base, d);
        ++pieces;
        i = j;
    }
    std::fclose(out);
    std::printf("%zu items, %zu edit(s), %zu moved branch(es), %zu moved field(s), %zu unmoved value(s), "
                "%zu piece(s), %zu held relative branch(es) whose target no longer lines up, %zu code item(s) matched as bytes, "
                "%zu mismatched item(s)\n",
                items.size(), edits, branches_moved, fields_moved, unmoved, pieces, held_branches, code_as_bytes, unexplained_items);
    cs_free(x, 1);
    cs_free(y, 1);
    cs_close(&cs);
    return unexplained_items ? 1 : 0;
}
