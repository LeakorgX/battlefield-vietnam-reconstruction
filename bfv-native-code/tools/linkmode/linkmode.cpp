// linkmode: writes link-mode copies of the lifted (L1) and readable (L2) C
// trees in which every original-image address is a linker symbol
// (a_XXXXXXXX) instead of a number, so the reconstructed C can be linked
// against a relinked image (static_recomp.py --symbolic).
//
//   linkmode.exe <refs.tsv> <lifted/T> <readable/T or -> <symbols_T.h or -> <out> <image_lo> <image_hi>
//   linkmode.exe --src <refs.tsv> <bodies.tsv> <src> <T> <target.h> <out> <image_lo> <image_hi>
//
// The second form rewrites the hand-written modules (see src_main below).
//
// Output: <out>/lifted, <out>/readable, <out>/runtime/symbols_T.h,
// <out>/report.tsv (image-range literals left as numbers, with the reason),
// <out>/addresses.txt (every a_ symbol used).
//
// The L1 C carries one comment per original instruction
// ("    /* 005acb90  call 0x500b70 ..."). A hexadecimal literal in that
// instruction's code becomes BFV_A(XXXXXXXX) when it is
//   - an absolute field of the instruction that symscan decided is an address
//     (refs.tsv rows of kind disp/imm whose field lies inside the instruction),
//   - the target of a call, jmp, jcc or loop, or
//   - the value assigned to c->eip / passed to EXIT_TO (an exit to native code).
// The L2 C has no per-instruction comments; its literals are decided with the
// union of the same function's L1 instruction sets (same body symbol
// bfv_T_l1_body_XXXXXXXX), unless a value is both an address and a constant
// in that function. L2 names in symbols_T.h become symbols too.
// The symbol keeps the literal's C type: BFV_A for a `u` literal (uint32_t),
// BFV_AI for a plain one (int; every image address fits in int).
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct Ref { uint32_t where, value; bool address; };
static std::vector<Ref> refs;  // sorted by where
static uint32_t image_lo, image_hi;
static std::map<std::string, long> counts;
static FILE *report;
static std::set<uint32_t> all_used;  // every a_ symbol, for the definitions file

struct Sets { std::set<uint32_t> addresses, constants; };
static std::map<uint32_t, Sets> functions;  // L1 body address -> union of its instruction sets

static bool is_hex(char c) { return isxdigit((unsigned char)c) != 0; }
static bool is_ident(char c) { return isalnum((unsigned char)c) || c == '_'; }

static void load_refs(const char *path) {
    std::ifstream in(path);
    if (!in) { fprintf(stderr, "cannot read %s\n", path); exit(1); }
    std::string line;
    std::getline(in, line);
    while (std::getline(in, line)) {
        std::vector<std::string> f;
        std::stringstream ss(line);
        std::string x;
        while (std::getline(ss, x, '\t')) f.push_back(x);
        if (f.size() < 6 || (f[1] != "disp" && f[1] != "imm")) continue;
        refs.push_back({(uint32_t)strtoul(f[0].c_str(), 0, 16), (uint32_t)strtoul(f[2].c_str(), 0, 16),
                        f[5] == "address"});
    }
    std::sort(refs.begin(), refs.end(), [](const Ref &a, const Ref &b) { return a.where < b.where; });
}

// "    /* 005acb90  call 0x500b70 ..." -> address and the instruction text.
// Instruction comments are indented; function header comments are not.
static bool insn_comment(const std::string &line, uint32_t &addr, std::string &text) {
    size_t i = line.find_first_not_of(' ');
    if (i == 0 || i == std::string::npos || line.compare(i, 3, "/* ") != 0) return false;
    i += 3;
    if (line.size() < i + 10) return false;
    for (int k = 0; k < 8; k++)
        if (!is_hex(line[i + k])) return false;
    if (line[i + 8] != ' ' || line[i + 9] != ' ') return false;
    addr = (uint32_t)strtoul(line.substr(i, 8).c_str(), 0, 16);
    size_t end = line.find(" */", i + 10);
    text = line.substr(i + 10, end == std::string::npos ? std::string::npos : end - i - 10);
    size_t semi = text.find("  ;");
    if (semi != std::string::npos) text.resize(semi);
    return true;
}

// "void bfv_client_l1_body_005ad020(" -> 0x5ad020
static bool body_start(const std::string &line, uint32_t &addr) {
    if (line.rfind("void bfv_", 0) != 0) return false;
    size_t p = line.find("_l1_body_");
    if (p == std::string::npos || line.size() < p + 17) return false;
    for (int k = 0; k < 8; k++)
        if (!is_hex(line[p + 9 + k])) return false;
    addr = (uint32_t)strtoul(line.substr(p + 9, 8).c_str(), 0, 16);
    return true;
}

static void fill_block(Sets &b, uint32_t addr, uint32_t next, const std::string &text) {
    b = Sets();
    uint32_t end = next > addr && next - addr <= 15 ? next : addr + 15;
    auto it = std::lower_bound(refs.begin(), refs.end(), addr, [](const Ref &r, uint32_t a) { return r.where < a; });
    for (; it != refs.end() && it->where < end; ++it) (it->address ? b.addresses : b.constants).insert(it->value);
    std::string m = text.substr(0, text.find(' '));
    bool branch = m == "call" || m == "jmp" || (m.size() >= 2 && m[0] == 'j') || m.rfind("loop", 0) == 0;
    size_t sp = text.find(' ');
    if (branch && sp != std::string::npos && text.compare(sp + 1, 2, "0x") == 0)
        b.addresses.insert((uint32_t)strtoul(text.c_str() + sp + 1, 0, 16));
}

// Rewrites the literals of one code line. `sets` is the instruction's (L1) or
// the function's (L2) set, or null outside any.
static std::string process_line(const std::string &line, const Sets *sets, const char *scope, uint32_t where,
                                bool &in_comment, std::set<uint32_t> &used, const std::string &file) {
    std::string out;
    bool exit = line.find("c->eip = ") != std::string::npos || line.find("EXIT_TO(") != std::string::npos;
    size_t i = 0, n = line.size();
    while (i < n) {
        if (in_comment) {
            size_t e = line.find("*/", i);
            if (e == std::string::npos) { out += line.substr(i); break; }
            out += line.substr(i, e + 2 - i);
            i = e + 2;
            in_comment = false;
            continue;
        }
        if (line.compare(i, 2, "/*") == 0) { in_comment = true; out += "/*"; i += 2; continue; }
        if (line.compare(i, 2, "//") == 0) { out += line.substr(i); break; }
        if (line[i] == '"' || line[i] == '\'') {  // string or char literal
            char q = line[i];
            size_t j = i + 1;
            while (j < n && line[j] != q) j += line[j] == '\\' ? 2 : 1;
            j = std::min(j + 1, n);
            out += line.substr(i, j - i);
            i = j;
            continue;
        }
        if (line[i] == '0' && i + 2 < n && (line[i + 1] == 'x' || line[i + 1] == 'X') && is_hex(line[i + 2]) &&
            (i == 0 || !is_ident(line[i - 1]))) {
            size_t j = i + 2;
            while (j < n && is_hex(line[j])) j++;
            size_t digits = j;
            while (j < n && (line[j] == 'u' || line[j] == 'U' || line[j] == 'l' || line[j] == 'L')) j++;
            if (j < n && is_ident(line[j])) { out += line.substr(i, j - i); i = j; continue; }
            unsigned long long v = strtoull(line.substr(i + 2, digits - i - 2).c_str(), 0, 16);
            if (v >= image_lo && v < image_hi) {
                uint32_t value = (uint32_t)v;
                bool constant = sets && sets->constants.count(value);
                bool address = sets && sets->addresses.count(value);
                std::string suffix = line.substr(digits, j - digits);
                bool typed = suffix.empty() || suffix == "u" || suffix == "U";
                if (typed && ((exit && !constant) || (address && !constant))) {
                    char buf[32];
                    snprintf(buf, sizeof buf, suffix.empty() ? "BFV_AI(%08x)" : "BFV_A(%08x)", value);
                    out += buf;
                    used.insert(value);
                    counts[std::string(scope) + " symbol"]++;
                    i = j;
                    continue;
                }
                const char *why = !typed ? "long_literal" : !sets ? "outside_function"
                                  : constant ? "symscan_constant" : "no_reference";
                counts[std::string(scope) + " " + why]++;
                fprintf(report, "%s\t%s\t%08x\t%08x\t%s\n", scope, file.c_str(), where, value, why);
            }
            out += line.substr(i, j - i);
            i = j;
            continue;
        }
        out += line[i++];
    }
    return out;
}

static std::vector<std::string> read_lines(const fs::path &p) {
    std::ifstream in(p, std::ios::binary);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

static void write_with_externs(const fs::path &dst, const std::vector<std::string> &out, const std::set<uint32_t> &used,
                               const std::string &rel) {
    fs::create_directories(dst.parent_path());
    std::ofstream o(dst, std::ios::binary);
    bool declared = used.empty();
    for (auto &l : out) {
        o << l << '\n';
        if (!declared && l.rfind("#include", 0) == 0) {
            for (uint32_t v : used) {
                char buf[48];
                snprintf(buf, sizeof buf, "extern char a_%08x[];\n", v);
                o << buf;
            }
            declared = true;
        }
    }
    if (!declared) { fprintf(stderr, "%s: no #include line for the declarations\n", rel.c_str()); exit(1); }
    all_used.insert(used.begin(), used.end());
}

static void lifted_file(const fs::path &src, const fs::path &dst, const std::string &rel) {
    std::vector<std::string> lines = read_lines(src);
    std::vector<std::pair<size_t, uint32_t>> marks;
    std::vector<std::string> texts;
    for (size_t k = 0; k < lines.size(); k++) {
        uint32_t a;
        std::string t;
        if (insn_comment(lines[k], a, t)) { marks.push_back({k, a}); texts.push_back(t); }
    }
    std::vector<std::string> out;
    std::set<uint32_t> used;
    Sets block;
    bool in_block = false, in_comment = false;
    uint32_t fn = 0, where = 0;
    Sets *fn_sets = nullptr;
    size_t m = 0;
    for (size_t k = 0; k < lines.size(); k++) {
        uint32_t a;
        if (body_start(lines[k], a)) { fn = a; fn_sets = &functions[fn]; }
        if (m < marks.size() && marks[m].first == k) {
            uint32_t next = m + 1 < marks.size() ? marks[m + 1].second : 0;
            fill_block(block, marks[m].second, next, texts[m]);
            where = marks[m].second;
            in_block = true;
            if (fn_sets) {
                fn_sets->addresses.insert(block.addresses.begin(), block.addresses.end());
                fn_sets->constants.insert(block.constants.begin(), block.constants.end());
            }
            m++;
        }
        if (lines[k] == "}") { in_block = false; fn_sets = nullptr; where = 0; }
        out.push_back(process_line(lines[k], in_block ? &block : nullptr, "lifted", where, in_comment, used, rel));
    }
    write_with_externs(dst, out, used, rel);
}

// L2: instruction blocks where an L1-fallback body still has them, else the function's sets.
static void readable_file(const fs::path &src, const fs::path &dst, const std::string &rel) {
    std::vector<std::string> lines = read_lines(src);
    std::vector<std::pair<size_t, uint32_t>> marks;
    std::vector<std::string> texts;
    for (size_t k = 0; k < lines.size(); k++) {
        uint32_t a;
        std::string t;
        if (insn_comment(lines[k], a, t)) { marks.push_back({k, a}); texts.push_back(t); }
    }
    std::vector<std::string> out;
    std::set<uint32_t> used;
    Sets block;
    const Sets *fn_sets = nullptr;
    bool in_block = false, in_comment = false;
    uint32_t where = 0;
    size_t m = 0;
    for (size_t k = 0; k < lines.size(); k++) {
        uint32_t a;
        if (body_start(lines[k], a)) {
            auto it = functions.find(a);
            fn_sets = it == functions.end() ? nullptr : &it->second;
            where = a;
            if (!fn_sets) { fprintf(stderr, "%s: %08x has no L1 body\n", rel.c_str(), a); exit(1); }
        }
        if (m < marks.size() && marks[m].first == k) {
            uint32_t next = m + 1 < marks.size() ? marks[m + 1].second : 0;
            fill_block(block, marks[m].second, next, texts[m]);
            in_block = true;
            m++;
        }
        if (lines[k] == "}") { in_block = false; fn_sets = nullptr; where = 0; }
        out.push_back(process_line(lines[k], in_block ? &block : fn_sets, "readable", where, in_comment, used, rel));
    }
    write_with_externs(dst, out, used, rel);
}

// "#define name 0xADDR" -> "#define name BFV_AI(00ADDR)" with its declaration
static void symbols_header(const fs::path &src, const fs::path &dst) {
    std::vector<std::string> lines = read_lines(src);
    fs::create_directories(dst.parent_path());
    std::ofstream o(dst, std::ios::binary);
    long named = 0;
    for (auto &l : lines) {
        size_t p = l.rfind(" 0x");
        if (l.rfind("#define ", 0) == 0 && p != std::string::npos && p > 8) {
            char *end;
            unsigned long long v = strtoull(l.c_str() + p + 3, &end, 16);
            if (*end != 0) { fprintf(stderr, "symbols header: unexpected line %s\n", l.c_str()); exit(1); }
            if (v >= image_lo && v < image_hi) {
                char buf[64];
                snprintf(buf, sizeof buf, "extern char a_%08llx[];\n", v);
                o << buf << l.substr(0, p);
                snprintf(buf, sizeof buf, " BFV_AI(%08llx)\n", v);
                o << buf;
                named++;
                all_used.insert((uint32_t)v);
                continue;
            }
            fprintf(report, "symbols\t%s\t00000000\t%08llx\toutside_image\n", src.filename().string().c_str(), v);
        }
        o << l << '\n';
    }
    counts["symbols named"] = named;
}

static long tree(const fs::path &root, const fs::path &outdir, void (*fn)(const fs::path &, const fs::path &, const std::string &)) {
    std::vector<fs::path> files;
    for (auto &e : fs::recursive_directory_iterator(root))
        if (e.is_regular_file() && e.path().extension() == ".c") files.push_back(e.path());
    std::sort(files.begin(), files.end());
    for (auto &f : files) {
        fs::path rel = fs::relative(f, root);
        fn(f, outdir / rel, rel.generic_string());
    }
    return (long)files.size();
}

// --src: the hand-written modules (src/*.c). Each function bfv_T_..._XXXXXXXX
// replaces the native function its catalog entry names; bodies.tsv gives that
// entry's native bytes (native_body, else guarded_prefix). A literal in the
// function becomes a symbol when it is a rel32 call/jmp/jcc target in those
// bytes or an immediate/displacement there that symscan decided is an address,
// and stays a number when symscan decided it is a constant there. A literal
// outside any function (a #define in a `#if BFV_BUILD_T` block) uses the union
// of the block's functions. Blocks of the other target are copied unchanged.
static std::map<std::string, Sets> src_functions;  // symbol -> sets

static void load_bodies(const char *path) {
    std::ifstream in(path);
    if (!in) { fprintf(stderr, "cannot read %s\n", path); exit(1); }
    std::string line;
    while (std::getline(in, line)) {
        std::stringstream ss(line);
        std::string symbol, a, hex;
        std::getline(ss, symbol, '\t');
        std::getline(ss, a, '\t');
        std::getline(ss, hex, '\t');
        uint32_t addr = (uint32_t)strtoul(a.c_str(), 0, 16);
        std::vector<uint8_t> b;
        for (size_t k = 0; k + 1 < hex.size(); k += 2) b.push_back((uint8_t)strtoul(hex.substr(k, 2).c_str(), 0, 16));
        Sets &s = src_functions[symbol];
        auto rel = [&](size_t at, size_t len) {
            if (at + len + 4 > b.size()) return;
            int32_t d;
            memcpy(&d, &b[at + len], 4);
            s.addresses.insert(addr + (uint32_t)(at + len + 4) + (uint32_t)d);
        };
        for (size_t k = 0; k < b.size(); k++) {
            if (b[k] == 0xe8 || b[k] == 0xe9) rel(k, 1);
            if (b[k] == 0x0f && k + 1 < b.size() && (b[k + 1] & 0xf0) == 0x80) rel(k, 2);
        }
        auto it = std::lower_bound(refs.begin(), refs.end(), addr, [](const Ref &r, uint32_t x) { return r.where < x; });
        for (; it != refs.end() && it->where < addr + b.size(); ++it) (it->address ? s.addresses : s.constants).insert(it->value);
    }
}

// "uint32_t SC bfv_client_return_word_0052adb0(void)" (no ';') -> symbol
static bool src_function(const std::string &line, const std::string &target, std::string &symbol) {
    if (line.empty() || !is_ident(line[0]) || line.back() == ';' || line.rfind("#", 0) == 0) return false;
    size_t p = line.find("bfv_" + target + "_");
    if (p == std::string::npos || (p > 0 && is_ident(line[p - 1]))) return false;
    size_t e = p;
    while (e < line.size() && is_ident(line[e])) e++;
    if (e >= line.size() || line[e] != '(') return false;
    symbol = line.substr(p, e - p);
    return true;
}

// target.h names that are image addresses -> value (read before the files)
static std::map<std::string, uint32_t> target_names;

// An "i"(...) asm operand takes the bare symbol (a char array, so +N is a
// byte offset): GCC does not accept every integer cast of an address as an
// immediate operand, while symbol+offset prints the same with %c.
static std::string asm_operands(std::string line) {
    for (size_t p = line.find("\"i\"("); p != std::string::npos; p = line.find("\"i\"(", p + 4)) {
        int depth = 0;
        size_t k = p + 3;
        for (; k < line.size(); k++) {
            if (line[k] == '(') depth++;
            else if (line[k] == ')' && --depth == 0) break;
            else if (is_ident(line[k]) && !is_ident(line[k - 1])) {
                size_t e = k;
                while (e < line.size() && is_ident(line[e])) e++;
                auto it = target_names.find(line.substr(k, e - k));
                if (it != target_names.end()) {
                    char buf[16];
                    snprintf(buf, sizeof buf, "a_%08x", it->second);
                    line.replace(k, e - k, buf);
                    e = k + strlen(buf);
                }
                k = e - 1;
            }
        }
    }
    return line;
}

static void src_file(const fs::path &src, const fs::path &dst, const std::string &rel, const std::string &target) {
    std::vector<std::string> lines = read_lines(src);
    std::string mine = "#if BFV_BUILD_" + std::string(target == "client" ? "CLIENT" : "SERVER");
    std::string other = target == "client" ? "#if BFV_BUILD_SERVER" : "#if BFV_BUILD_CLIENT";
    // pass 1: the union per block (block 0 = outside any target block)
    std::vector<int> block_of(lines.size(), 0);
    std::map<int, Sets> unions;
    int block = 0, blocks = 0;
    bool skip = false;
    for (size_t k = 0; k < lines.size(); k++) {
        const std::string &l = lines[k];
        if (l.rfind(mine, 0) == 0) block = ++blocks;
        else if (l.rfind(other, 0) == 0) { block = -1; skip = true; }
        else if (l.rfind("#endif", 0) == 0) { block = 0; skip = false; }
        block_of[k] = skip ? -1 : block;
        std::string symbol;
        if (!skip && src_function(l, target, symbol)) {
            auto it = src_functions.find(symbol);
            if (it == src_functions.end()) { fprintf(stderr, "%s: %s has no catalog entry\n", rel.c_str(), symbol.c_str()); exit(1); }
            Sets &u = unions[block];
            u.addresses.insert(it->second.addresses.begin(), it->second.addresses.end());
            u.constants.insert(it->second.constants.begin(), it->second.constants.end());
        }
    }
    std::vector<std::string> out;
    std::set<uint32_t> used;
    bool in_comment = false;
    const Sets *fn = nullptr;
    uint32_t where = 0;
    for (size_t k = 0; k < lines.size(); k++) {
        const std::string &l = lines[k];
        if (block_of[k] < 0) { out.push_back(l); continue; }
        std::string symbol;
        if (src_function(l, target, symbol)) {
            fn = &src_functions[symbol];
            where = (uint32_t)strtoul(symbol.substr(symbol.size() - 8).c_str(), 0, 16);
        }
        auto u = unions.find(block_of[k]);
        const Sets *sets = fn ? fn : u == unions.end() ? nullptr : &u->second;
        out.push_back(asm_operands(process_line(l, sets, "src", where, in_comment, used, rel)));
        if (l == "}") { fn = nullptr; where = 0; }
    }
    write_with_externs(dst, out, used, rel);
}

// target.h: every BFV_ name build.py gives an image address becomes a symbol
static void target_header(const fs::path &src, const fs::path &dst) {
    std::vector<std::string> lines = read_lines(src);
    fs::create_directories(dst.parent_path());
    std::ofstream o(dst, std::ios::binary);
    long named = 0;
    for (auto &l : lines) {
        size_t p = l.rfind(" 0x");
        if (l.rfind("#define BFV_", 0) == 0 && p != std::string::npos && !l.empty() && l.back() == 'u') {
            unsigned long long v = strtoull(l.c_str() + p + 3, 0, 16);
            if (v >= image_lo && v < image_hi) {
                char buf[64];
                snprintf(buf, sizeof buf, "extern char a_%08llx[];\n", v);
                o << buf << l.substr(0, p);
                snprintf(buf, sizeof buf, " BFV_A(%08llx)\n", v);
                o << buf;
                named++;
                target_names[l.substr(8, p - 8)] = (uint32_t)v;
                all_used.insert((uint32_t)v);
                continue;
            }
        }
        o << l << '\n';
    }
    counts["target.h named"] = named;
}

static int src_main(int argc, char **argv) {
    // linkmode.exe --src refs.tsv bodies.tsv src T target.h out image_lo image_hi
    if (argc != 10) {
        fprintf(stderr, "usage: linkmode --src refs.tsv bodies.tsv src target target.h out image_lo image_hi\n");
        return 2;
    }
    load_refs(argv[2]);
    load_bodies(argv[3]);
    std::string target = argv[5];
    fs::path out = argv[7];
    image_lo = (uint32_t)strtoul(argv[8], 0, 16);
    image_hi = (uint32_t)strtoul(argv[9], 0, 16);
    fs::create_directories(out);
    report = fopen((out / "report.tsv").string().c_str(), "w");
    fprintf(report, "tree\tfile\tfunction\tvalue\treason\n");
    std::vector<fs::path> files;
    for (auto &e : fs::directory_iterator(argv[4]))
        if (e.is_regular_file() && (e.path().extension() == ".c" || e.path().extension() == ".h")) files.push_back(e.path());
    std::sort(files.begin(), files.end());
    target_header(argv[6], out / "target.h");
    for (auto &f : files) {
        std::string rel = f.filename().string();
        if (f.extension() == ".c") src_file(f, out / rel, rel, target);
        else fs::copy_file(f, out / rel, fs::copy_options::overwrite_existing);
    }
    fclose(report);
    FILE *list = fopen((out / "addresses.txt").string().c_str(), "w");
    for (uint32_t v : all_used) fprintf(list, "%08x\n", v);
    fclose(list);
    printf("%zu files, %zu catalog functions", files.size(), src_functions.size());
    for (auto &c : counts) printf(", %s %ld", c.first.c_str(), c.second);
    printf("\n");
    return 0;
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--src") == 0) return src_main(argc, argv);
    if (argc != 8) {
        fprintf(stderr, "usage: linkmode refs.tsv lifted/T readable/T|- symbols_T.h|- out image_lo image_hi\n");
        return 2;
    }
    load_refs(argv[1]);
    fs::path out = argv[5];
    image_lo = (uint32_t)strtoul(argv[6], 0, 16);
    image_hi = (uint32_t)strtoul(argv[7], 0, 16);
    fs::create_directories(out);
    report = fopen((out / "report.tsv").string().c_str(), "w");
    fprintf(report, "tree\tfile\tinstruction_or_function\tvalue\treason\n");
    long files = tree(argv[2], out / "lifted", lifted_file);
    if (strcmp(argv[3], "-") != 0) files += tree(argv[3], out / "readable", readable_file);
    if (strcmp(argv[4], "-") != 0) symbols_header(argv[4], out / "runtime" / fs::path(argv[4]).filename());
    fclose(report);
    // addresses.txt: one a_ symbol value per line (static_recomp.py defines them)
    FILE *list = fopen((out / "addresses.txt").string().c_str(), "w");
    for (uint32_t v : all_used) fprintf(list, "%08x\n", v);
    fclose(list);
    printf("%ld files, %zu functions", files, functions.size());
    for (auto &c : counts) printf(", %s %ld", c.first.c_str(), c.second);
    printf("\n");
    return 0;
}
