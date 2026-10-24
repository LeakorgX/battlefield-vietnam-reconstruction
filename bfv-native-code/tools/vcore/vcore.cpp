// Differential verifier core: the emulation half of tools/verify_lifted.py in C++.
//
// verify_lifted.py --engine cpp links the lifted code, works out each
// function's callee stubs, call sites, resume stubs and ABI masks, and writes
// them to a job file; this program runs the native and the lifted body on two
// unicorn machines from the same random states and writes one result line per
// function. States, stubs and comparisons are the ones verify_lifted.py
// documents (Python's Mersenne Twister is reproduced, so the states are the
// same); see that file for the rules.
//
// Usage: vcore <job file> <result file> [threads]
#include <unicorn/unicorn.h>
#include <capstone/capstone.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using u32 = uint32_t; using u64 = uint64_t; using u8 = uint8_t; using u16 = uint16_t;

// ---- constants (verify_lifted.py) ----
static const u32 ARENA = 0x02000000, STACK = 0x03000000, BLOB = 0x20000000;
static const u32 ARENA_SIZE = 0x10000; static const int LAZY_PAGES = 64;
static const double RUN_SECONDS = 20; static const size_t EVENT_LIMIT = 20000;
static const u64 PREFIX_BUDGET = 20000, BIG_BUDGET = 400000;
static const u32 FUNCS = 0x1ff00000, FUNC_COUNT = 32;
static const u32 ESP_BIT = 16, POISON = 0x5a5a5a5b;
static const u32 CODE = 0x1ff10000, STOP = CODE, LOADER = CODE + 0x100; static const int OPAQUE_CALLS = 64;
static const u32 ENTRY_SP = STACK + 0x8000;
static const u32 TEB = 0x1fd00000, GDT = TEB + 0x1000, FS_SELECTOR = 3 * 8 | 3;
static const double FLOATS[] = {0.0, 1.0, -1.0, 0.5, 2.0, -3.25, 100.0, 1e-3, 0.75, -0.125};
static const int GPRS[] = {UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_EBX,
                           UC_X86_REG_ESP, UC_X86_REG_EBP, UC_X86_REG_ESI, UC_X86_REG_EDI};

static u32 fbits(double d) { float f = (float)d; u32 v; memcpy(&v, &f, 4); return v; }

// ---- Python's random.Random ----
struct PyRandom {
    u32 mt[624]; int mti = 625;
    void init_genrand(u32 s) {
        mt[0] = s;
        for (mti = 1; mti < 624; mti++) mt[mti] = 1812433253u * (mt[mti - 1] ^ (mt[mti - 1] >> 30)) + mti;
    }
    void init_by_array(const std::vector<u32> &key) {
        init_genrand(19650218u);
        u32 i = 1, j = 0, n = 624, len = (u32)key.size();
        for (u32 k = n > len ? n : len; k; k--) {
            mt[i] = (mt[i] ^ ((mt[i - 1] ^ (mt[i - 1] >> 30)) * 1664525u)) + key[j] + j;
            i++; j++;
            if (i >= n) { mt[0] = mt[n - 1]; i = 1; }
            if (j >= len) j = 0;
        }
        for (u32 k = n - 1; k; k--) {
            mt[i] = (mt[i] ^ ((mt[i - 1] ^ (mt[i - 1] >> 30)) * 1566083941u)) - i;
            i++;
            if (i >= n) { mt[0] = mt[n - 1]; i = 1; }
        }
        mt[0] = 0x80000000u;
    }
    explicit PyRandom(unsigned __int128 seed) {  // seed >= 0
        std::vector<u32> key;
        while (seed) { key.push_back((u32)seed); seed >>= 32; }
        if (key.empty()) key.push_back(0);
        init_by_array(key);
    }
    u32 genrand() {
        static const u32 mag01[2] = {0, 0x9908b0dfu};
        u32 y;
        if (mti >= 624) {
            int kk;
            for (kk = 0; kk < 624 - 397; kk++) { y = (mt[kk] & 0x80000000u) | (mt[kk + 1] & 0x7fffffffu); mt[kk] = mt[kk + 397] ^ (y >> 1) ^ mag01[y & 1]; }
            for (; kk < 623; kk++) { y = (mt[kk] & 0x80000000u) | (mt[kk + 1] & 0x7fffffffu); mt[kk] = mt[kk + (397 - 624)] ^ (y >> 1) ^ mag01[y & 1]; }
            y = (mt[623] & 0x80000000u) | (mt[0] & 0x7fffffffu); mt[623] = mt[396] ^ (y >> 1) ^ mag01[y & 1];
            mti = 0;
        }
        y = mt[mti++];
        y ^= y >> 11; y ^= (y << 7) & 0x9d2c5680u; y ^= (y << 15) & 0xefc60000u; y ^= y >> 18;
        return y;
    }
    double random() { u32 a = genrand() >> 5, b = genrand() >> 6; return (a * 67108864.0 + b) * (1.0 / 9007199254740992.0); }
    u32 getrandbits(int k) { return k == 0 ? 0 : genrand() >> (32 - k); }  // k <= 32
    u32 below(u32 n) { int k = 0; while (k < 32 && (n >> k)) k++; u32 r = getrandbits(k); while (r >= n) r = getrandbits(k); return r; }
    u32 randrange(u32 n) { return below(n); }
    u32 randrange(u32 a, u32 b) { return a + below(b - a); }
    u32 randrange(u32 a, u32 b, u32 step) { return a + step * below((b - a + step - 1) / step); }
    template <class T> T choice(const std::vector<T> &v) { return v[below((u32)v.size())]; }
};

// ---- SHA-256 (state seeds) ----
static void sha256(const u8 *data, size_t len, u8 out[32]) {
    static const u32 K[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
        0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
        0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
        0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    u32 h[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    std::vector<u8> m(data, data + len); m.push_back(0x80);
    while (m.size() % 64 != 56) m.push_back(0);
    u64 bits = (u64)len * 8; for (int i = 7; i >= 0; i--) m.push_back((u8)(bits >> (8 * i)));
    auto rotr = [](u32 x, int n) { return (x >> n) | (x << (32 - n)); };
    for (size_t o = 0; o < m.size(); o += 64) {
        u32 w[64];
        for (int i = 0; i < 16; i++) w[i] = (u32)m[o + 4 * i] << 24 | (u32)m[o + 4 * i + 1] << 16 | (u32)m[o + 4 * i + 2] << 8 | m[o + 4 * i + 3];
        for (int i = 16; i < 64; i++) {
            u32 s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3), s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        u32 a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; i++) {
            u32 t1 = hh + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
            u32 t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
            hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    for (int i = 0; i < 8; i++) for (int j = 0; j < 4; j++) out[4 * i + j] = (u8)(h[i] >> (24 - 8 * j));
}

// ---- states ----
struct State { std::vector<u8> arena, stack; u32 regs[8]; std::vector<double> x87; u32 flags; };

static void pack32(std::vector<u8> &out, u32 v) { for (int i = 0; i < 4; i++) out.push_back((u8)(v >> (8 * i))); }

static State random_state(PyRandom &rng, int k, bool small) {
    double pointers = k % 3 == 2 ? 0.85 : 0.45;
    std::vector<double> floats(FLOATS, FLOATS + 10);
    auto fresh = [&]() -> u32 {
        double x = rng.random();
        if (x < 0.1) return FUNCS + 8 * rng.randrange(FUNC_COUNT);
        if (x < pointers) return ARENA + 0x100 + rng.randrange(0, ARENA_SIZE - 0x400, 4);
        if (small) {
            if (rng.random() < 0.9) { u32 r = rng.randrange(0, 64); return rng.choice(std::vector<u32>{0, 1, 2, 3, 4, 5, 8, 16, r}); }
            return fbits(rng.choice(floats));
        }
        if (x < pointers + 0.35 * (1 - pointers) / 0.55) {
            u32 r = rng.randrange(0, 64);
            return rng.choice(std::vector<u32>{0, 1, 2, 3, 4, 5, 8, 16, 31, 0x7f, 0x80, 0xff, 0x7fffffff, 0x80000000u, 0xffffffffu, r});
        }
        return fbits(rng.choice(floats));
    };
    std::vector<u32> pool;
    int npool = (int)rng.choice(std::vector<u32>{2, 3, 5});
    for (int i = 0; i < npool; i++) pool.push_back(fresh());
    auto value = [&]() -> u32 {
        if (k % 2) { if (rng.random() < 0.85) return rng.choice(pool); return fresh(); }
        return fresh();
    };
    std::vector<u32> words(ARENA_SIZE / 4);
    for (auto &w : words) w = value();
    if (small) for (size_t n = 0; n < words.size(); n += 4) if (rng.random() < 0.1) words[n] = ARENA + 4 * (u32)n;
    for (size_t n = words.size() - 64; n < words.size(); n++) words[n] = 0;
    State s;
    for (u32 w : words) pack32(s.arena, w);
    pack32(s.stack, STOP);
    for (int i = 0; i < 16; i++) pack32(s.stack, value());
    for (int i = 0; i < 8; i++) s.regs[i] = value();
    int nx = (int)rng.choice(std::vector<u32>{0, 0, 1, 2});
    for (int i = 0; i < nx; i++) s.x87.push_back(rng.choice(floats));
    s.flags = rng.choice(std::vector<u32>{0x202, 0x246, 0x287, 0xa93});
    return s;
}

static State frame_state(const State &st) {
    State s = st; s.regs[5] = ENTRY_SP + 0x40; s.stack.clear();
    for (int i = 0; i < 32; i++) pack32(s.stack, STOP);
    return s;
}

static std::vector<u8> lazy_page(u64 seed, u32 page) {
    PyRandom rng((unsigned __int128)seed ^ ((unsigned __int128)page * 0x9e3779b1u));
    std::vector<double> floats(FLOATS, FLOATS + 10);
    std::vector<u8> out; out.reserve(4096);
    for (int i = 0; i < 1024; i++) {
        double k = rng.random(); u32 w;
        if (k < 0.12) w = FUNCS + 8 * rng.randrange(FUNC_COUNT);
        else if (k < 0.5) w = ARENA + 0x100 + rng.randrange(0, ARENA_SIZE - 0x400, 4);
        else if (k < 0.8) { u32 r = rng.randrange(0, 64); w = rng.choice(std::vector<u32>{0, 1, 2, 3, 4, 8, 16, 0xff, 0xffffffffu, r}); }
        else w = fbits(rng.choice(floats));
        pack32(out, w);
    }
    return out;
}

static u32 opaque_result(u32 target) { return ARENA + 0x100 + (u32)(((u64)target * 2654435761u) & 0x7ffc); }
static u32 opaque_result(u32 target, u32 n) {
    u32 h = (u32)((u64)target * 2654435761u + (u64)n * 0x85ebca6bu);
    return (h >> 16) % 4 == 0 ? 0 : ARENA + 0x100 + (h & 0x7ffc);
}

// ---- events and results ----
struct Event {
    bool call = false; u32 addr = 0; int size = 0; bool has = false; u64 val = 0;
    u32 regs[8] = {}; u8 known = 0; u8 stack[20] = {};
    bool operator==(const Event &o) const {
        if (call != o.call || addr != o.addr) return false;
        if (!call) return size == o.size && has == o.has && (!has || val == o.val);
        if (known != o.known || memcmp(stack, o.stack, 20)) return false;
        for (int i = 0; i < 8; i++) if ((known >> i & 1) && regs[i] != o.regs[i]) return false;
        return true;
    }
    bool operator!=(const Event &o) const { return !(*this == o); }
};
struct F80 { u64 m; u16 e; bool operator==(const F80 &o) const { return m == o.m && e == o.e; } };
struct Result {
    bool done = false; std::string error; std::vector<Event> events;
    u32 regs[8] = {}; u8 known = 0;
    u64 fpu_ctl[3] = {}; F80 fp[8] = {}; u64 mxcsr = 0; u64 xmm[8][2] = {};
    bool operator==(const Result &o) const {
        if (done != o.done) return false;
        if (!done) return error == o.error && events == o.events;
        if (known != o.known) return false;
        for (int i = 0; i < 8; i++) if ((known >> i & 1) && regs[i] != o.regs[i]) return false;
        for (int i = 0; i < 3; i++) if (fpu_ctl[i] != o.fpu_ctl[i]) return false;
        for (int i = 0; i < 8; i++) if (!(fp[i] == o.fp[i])) return false;
        if (mxcsr != o.mxcsr) return false;
        for (int i = 0; i < 8; i++) if (xmm[i][0] != o.xmm[i][0] || xmm[i][1] != o.xmm[i][1]) return false;
        return events == o.events;
    }
};

struct Site { u32 args; bool halt; };
struct Job {
    std::string name; u32 entry, home; u32 bytes;
    std::vector<std::pair<u32, std::vector<u8>>> callees;
    std::unordered_map<u32, Site> sites, lsites;
    std::unordered_map<u32, u32> returns, abi, labi;
    std::optional<u32> ret_mask;
};

// ---- machine ----
static uint64_t now_us() { return (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

struct Shared { std::vector<u8> image; u32 base = 0x400000, size_of_image = 0; std::vector<std::pair<u32, u32>> code; std::vector<u8> blob; u32 quiet0 = 0, quiet1 = 0, quiet2 = 0, shadow_lo = 0, shadow_hi = 0; };

// TEB page: no SEH frame, the stack's base and limit, the TEB itself
static std::vector<u8> teb_page() {
    std::vector<u8> p(0x1000, 0); u32 v[3] = {0xffffffffu, STACK + 0x10000, STACK}; memcpy(p.data(), v, 12);
    u32 self = TEB; memcpy(p.data() + 0x18, &self, 4); return p;
}

struct Machine {
    uc_engine *uc = nullptr; csh cs = 0, cs_count = 0; bool lifted; const Shared &sh;
    u32 base, image_end, blob_end; u32 quiet0, quiet1, quiet2; bool quiet; u32 shadow_lo = 0, shadow_hi = 0;
    std::vector<u8> funcs_pristine;
    std::vector<std::pair<u32, u32>> dirty;
    std::vector<u32> lazy; u64 state_seed = 0;
    bool active = false; std::vector<Event> events; u32 pushad_eip = 0; size_t pushad_at = 0, pushad_n = 0;
    std::unordered_map<u32, Site> *sites = nullptr; std::unordered_map<u32, Site> empty_sites;
    std::unordered_map<u32, u32> canon, abi_calls; std::optional<u32> ret_mask;
    std::set<u32> ret_calls; u32 ncalls = 0;
    std::optional<std::pair<u32, u32>> prev, last_from;
    std::unordered_map<u32, std::string> rets;
    std::unordered_map<u64, u32> ninsn; u64 budget = 0, executed = 0;  // instruction budget, counted per block
    std::unordered_set<u32> callees; u32 home = 0; std::vector<std::pair<u32, u32>> code;
    bool left = false, halted = false, flooded = false; u32 frame_top = 0; u64 deadline = 0; u32 ticks = 0;
    std::unordered_map<u32, std::pair<std::optional<u32>, u32>> stubs;
    uc_context *context = nullptr;

    u32 reg(int r) { u32 v = 0; uc_reg_read(uc, r, &v); return v; }
    void setreg(int r, u32 v) { uc_reg_write(uc, r, &v); }
    bool in_code(u32 a) const { for (auto &c : code) if (c.first <= a && a < c.second) return true; return false; }
    u32 rd32(u32 a) { u32 v = 0; uc_mem_read(uc, a, &v, 4); return v; }
    void wr32(u32 a, u32 v) { uc_mem_write(uc, a, &v, 4); }
    void remove_cache(u64 a, u64 b) { uc_ctl_remove_cache(uc, a, b); }

    // last instruction of a block: mnemonic, and whether it is a call with an immediate target
    std::string last_insn(u32 address, u32 size, bool *imm_call = nullptr) {
        std::vector<u8> b(size);
        if (uc_mem_read(uc, address, b.data(), size) != UC_ERR_OK) return "";
        cs_insn *insn; size_t n = cs_disasm(cs, b.data(), size, address, 0, &insn);
        std::string out;
        if (n) {
            cs_insn &l = insn[n - 1]; out = l.mnemonic;
            if (imm_call) *imm_call = l.detail && l.detail->x86.op_count && l.detail->x86.operands[0].type == X86_OP_IMM;
            cs_free(insn, n);
        }
        return out;
    }

    Machine(const Shared &s, bool is_lifted) : lifted(is_lifted), sh(s) {
        uc_open(UC_ARCH_X86, UC_MODE_32, &uc);
        uc_ctl_set_cpu_model(uc, UC_CPU_X86_PHENOM);  // 3DNow! for the d3dx and 0x0081/0x0085 vector code
        setreg(UC_X86_REG_CR4, 0x200);  // CR4.OSFXSR: SSE on the Phenom model
        cs_open(CS_ARCH_X86, CS_MODE_32, &cs); cs_option(cs, CS_OPT_DETAIL, CS_OPT_ON); cs_open(CS_ARCH_X86, CS_MODE_32, &cs_count);
        base = s.base; u32 mapped = (s.size_of_image + 4095) & ~4095u;
        uc_mem_map(uc, base, mapped, UC_PROT_ALL);
        uc_mem_write(uc, base, s.image.data(), s.image.size());
        // only code is executable: a call through a garbage pointer into data is opaque
        uc_mem_protect(uc, base, mapped, UC_PROT_READ | UC_PROT_WRITE);
        for (auto &c : s.code) { u32 lo = c.first & ~0xfffu, hi = (c.second + 0xfff) & ~0xfffu; uc_mem_protect(uc, lo, hi - lo, UC_PROT_ALL); }
        uc_mem_map(uc, ARENA, ARENA_SIZE, UC_PROT_READ | UC_PROT_WRITE); uc_mem_map(uc, STACK, 0x10000, UC_PROT_READ | UC_PROT_WRITE);
        uc_mem_map(uc, CODE, 0x1000, UC_PROT_ALL);
        uc_mem_map(uc, TEB, 0x2000, UC_PROT_READ | UC_PROT_WRITE);
        { auto t = teb_page(); uc_mem_write(uc, TEB, t.data(), t.size());
          u8 d[8] = {0xff, 0x0f, (u8)TEB, (u8)(TEB >> 8), (u8)(TEB >> 16), 0xf3, 0x40, (u8)(TEB >> 24)};
          uc_mem_write(uc, GDT + 8 * (FS_SELECTOR >> 3), d, 8);
          u8 ss[8] = {0xff, 0xff, 0, 0, 0, 0x93, 0xcf, 0}; uc_mem_write(uc, GDT + 4 * 8, ss, 8);  // flat 32-bit SS
          uc_x86_mmr gdtr = {0, GDT, 0x1000, 0}; uc_reg_write(uc, UC_X86_REG_GDTR, &gdtr); setreg(UC_X86_REG_SS, 4 * 8); setreg(UC_X86_REG_FS, FS_SELECTOR); }
        shadow_lo = s.shadow_lo; shadow_hi = s.shadow_hi;
        if (shadow_hi) uc_mem_map(uc, shadow_lo, shadow_hi - shadow_lo, UC_PROT_READ | UC_PROT_WRITE);
        uc_mem_map(uc, BLOB, ((u32)s.blob.size() + 0xffff) & ~0xffffu, UC_PROT_ALL); uc_mem_write(uc, BLOB, s.blob.data(), s.blob.size());
        blob_end = BLOB + (u32)s.blob.size();
        u8 nop = 0x90; uc_mem_write(uc, STOP, &nop, 1);
        uc_mem_map(uc, FUNCS, 0x1000, UC_PROT_ALL);
        std::vector<u8> f;
        for (u32 n = 0; n < FUNC_COUNT; n++) { f.push_back(0xb8); pack32(f, ARENA + 0x100 + n * 0x1f4); f.push_back(0xc3); f.push_back(0xcc); f.push_back(0xcc); }
        uc_mem_write(uc, FUNCS, f.data(), f.size());
        funcs_pristine.assign(0x1000, 0); uc_mem_read(uc, FUNCS, funcs_pristine.data(), 0x1000);
        image_end = base + s.size_of_image; code = s.code;
        if (lifted) { quiet0 = s.quiet0; quiet1 = s.quiet1; quiet2 = s.quiet2; } else quiet0 = quiet1 = quiet2 = 0;
        quiet = quiet1 != 0;
        uc_hook h;
        uc_hook_add(uc, &h, UC_HOOK_CODE, (void *)hk_indirect, this, FUNCS, FUNCS + 8 * FUNC_COUNT);
        uc_hook_add(uc, &h, UC_HOOK_MEM_READ, (void *)hk_access, this, 1, 0);
        uc_hook_add(uc, &h, UC_HOOK_MEM_WRITE, (void *)hk_access, this, 1, 0);
        if (quiet1 && !shadow_hi) {  // the readable runtime keeps *cpu on the shadow stack: no guest frame to leave out
            uc_hook_add(uc, &h, UC_HOOK_CODE, (void *)hk_enter, this, quiet0, quiet0);
            uc_hook_add(uc, &h, UC_HOOK_CODE, (void *)hk_enter_deep, this, quiet2, quiet2);
        }
        uc_hook_add(uc, &h, UC_HOOK_MEM_READ_UNMAPPED | UC_HOOK_MEM_WRITE_UNMAPPED, (void *)hk_unmapped, this, 1, 0);
        uc_hook_add(uc, &h, UC_HOOK_BLOCK, (void *)hk_block, this, 1, 0);
        uc_context_alloc(uc, &context); uc_context_save(uc, context);
    }

    static void hk_indirect(uc_engine *, u64 address, u32, void *d) {
        Machine *m = (Machine *)d;
        if (m->active && (address - FUNCS) % 8 == 0) {
            u32 sp = m->reg(UC_X86_REG_ESP);
            m->call_event((u32)address, sp);
            m->returning(ARENA + 0x100 + ((u32)address - FUNCS) / 8 * 0x1f4, 0, true);
        }
    }
    static void hk_access(uc_engine *uc, uc_mem_type kind, u64 address64, int size, int64_t value, void *d) {
        Machine *m = (Machine *)d; u32 address = (u32)address64;
        if (kind == UC_MEM_WRITE && ((m->base <= address && address < m->image_end) || (FUNCS <= address && address < FUNCS + 0x1000) || (BLOB <= address && address < m->blob_end)))
            m->dirty.push_back({address, (u32)size});
        if (!m->active) return;
        bool shadow = m->shadow_lo <= address && address < m->shadow_hi;
        if (!m->quiet1 && kind == UC_MEM_WRITE && ((BLOB <= address && address < m->blob_end) || shadow || (STACK <= address && (int64_t)address < (int64_t)m->reg(UC_X86_REG_ESP) - 0x20))) {
            m->left = true; uc_emu_stop(uc); return;
        }
        if ((BLOB <= address && address < m->blob_end) || shadow) return;
        u32 eip = m->reg(UC_X86_REG_EIP), sp = m->reg(UC_X86_REG_ESP);
        u64 top = STACK <= sp && sp < STACK + 0x10000 ? (u64)sp + 16 : 0;
        if ((m->quiet0 <= eip && eip < m->quiet1) ||
            (BLOB <= eip && eip < m->blob_end && STACK <= address && (u64)address < std::max<u64>(m->frame_top, top))) return;
        Event e; e.addr = address; e.size = size;
        if (kind == UC_MEM_WRITE) { e.has = true; e.val = size >= 8 ? (u64)value : (u64)value & ((1ull << (8 * size)) - 1); }
        // unicorn's pushad stores EDI first, at the lowest address; the CPU's
        // order (and the lifted body's) is EAX first, at the highest
        u8 op = 0;
        if (kind == UC_MEM_WRITE && m->base <= eip && eip < m->image_end && uc_mem_read(uc, eip, &op, 1) == UC_ERR_OK && op == 0x60) {
            bool more = m->pushad_eip == eip && m->pushad_n < 8 && m->pushad_n == m->events.size() - m->pushad_at;
            if (!more) { m->pushad_eip = eip; m->pushad_at = m->events.size(); m->pushad_n = 0; }
            m->events.insert(m->events.begin() + m->pushad_at, e); m->pushad_n++; return;
        }
        m->events.push_back(e);
    }
    static void hk_enter(uc_engine *, u64, u32, void *d) { Machine *m = (Machine *)d; m->frame_top = m->reg(UC_X86_REG_ESP) + 8; }
    static void hk_enter_deep(uc_engine *, u64, u32, void *d) { Machine *m = (Machine *)d; m->frame_top = m->reg(UC_X86_REG_ESP) + 12; }
    static bool hk_unmapped(uc_engine *uc, uc_mem_type, u64 address, int size, int64_t, void *d) {
        Machine *m = (Machine *)d;
        std::set<u32> pages = {(u32)address & ~0xfffu, (u32)(address + size - 1) & ~0xfffu};
        if (m->lazy.size() + pages.size() > (size_t)LAZY_PAGES) return false;
        for (u32 page : pages) {
            if (uc_mem_map(uc, page, 0x1000, UC_PROT_READ | UC_PROT_WRITE) != UC_ERR_OK) continue;
            m->lazy.push_back(page); auto c = lazy_page(m->state_seed, page); uc_mem_write(uc, page, c.data(), c.size());
        }
        return true;
    }
    static bool after_call(uc_engine *uc, u32 address) {
        u8 b[7];
        if (uc_mem_read(uc, address - 7, b, 7) != UC_ERR_OK) return false;
        if (b[2] == 0xe8) return true;
        for (int k = 0; k < 6; k++) {
            if (b[k] != 0xff || ((b[k + 1] >> 3) & 7) != 2) continue;
            int modrm = b[k + 1], mod = modrm >> 6, rm = modrm & 7, n = 2;
            if (mod != 3 && rm == 4) {
                if (k + 2 > 6) continue;
                n += 1; int base = b[k + 2] & 7;
                if (mod == 0 && base == 5) n += 4;
            } else if (mod == 0 && rm == 5) n += 4;
            n += mod == 1 ? 1 : mod == 2 ? 4 : 0;
            if (k + n == 7) return true;
        }
        return false;
    }
    u32 block_insns(u32 address, u32 size) {
        u64 key = (u64)address << 32 | size;
        auto it = ninsn.find(key);
        if (it != ninsn.end()) return it->second;
        std::vector<u8> b(size); u32 n = 1;
        if (uc_mem_read(uc, address, b.data(), size) == UC_ERR_OK) {
            cs_insn *insn; size_t k = cs_disasm(cs_count, b.data(), size, address, 0, &insn);
            if (k) { n = (u32)k; cs_free(insn, k); }
        }
        return ninsn.emplace(key, n).first->second;
    }
    static void hk_block(uc_engine *uc, u64 address64, u32 size, void *d) {
        Machine *m = (Machine *)d; u32 address = (u32)address64;
        if (m->active && (++m->ticks & 1023) == 0 && now_us() >= m->deadline) { uc_emu_stop(uc); return; }  // wall-clock limit without unicorn's per-start timer thread
        // the instruction budget: emu_start's count would add a code hook on every
        // instruction, and with one unicorn loses the lazy flags of an instruction
        // before a rep string instruction whose first access faults into a lazy page
        if (m->active && m->budget && (m->executed += m->block_insns(address, size)) > m->budget) { uc_emu_stop(uc); return; }
        auto prev = m->prev; m->prev = std::make_pair(address, size); m->last_from = prev;
        if (!prev || !m->active) return;
        std::string kind;
        if (BLOB <= prev->first && prev->first < m->blob_end) kind = "blob";
        else {
            auto it = m->rets.find(prev->first);
            if (it == m->rets.end()) {
                bool imm = false; std::string mn = m->last_insn(prev->first, prev->second, &imm);
                it = m->rets.emplace(prev->first, mn == "call" && imm ? "direct call" : mn).first;
            }
            kind = it->second;
        }
        if ((kind == "blob" || kind == "call" || (kind == "direct call" && address == m->home)) && m->in_code(address) &&
            (!m->callees.count(address) || address == m->home)) {
            u32 sp = m->reg(UC_X86_REG_ESP), ret = m->rd32(sp);
            if (m->sites->count(ret)) {
                m->call_event(address, sp);
                m->returning(opaque_result(address), 0, true); m->prev.reset(); return;
            }
        }
        if (kind == "ret" && !((BLOB <= address && address < m->blob_end) || address == STOP || after_call(uc, address))) { m->left = true; uc_emu_stop(uc); }
    }
    static void hk_called(uc_engine *, u64 address, u32, void *d) {
        Machine *m = (Machine *)d;
        if (m->active) {
            u32 sp = m->reg(UC_X86_REG_ESP);
            m->call_event((u32)address, sp);
            auto &s = m->stubs[(u32)address];
            m->returning(s.first ? *s.first : 0, s.second, (bool)s.first);
        }
    }

    void call_event(u32 address, u32 sp) {
        Event e; e.call = true; e.addr = address; e.known = 0xff;
        for (int i = 0; i < 8; i++) e.regs[i] = reg(GPRS[i]);
        auto it = abi_calls.find(rd32(sp));
        if (it == abi_calls.end() && ret_mask && !quiet) {
            auto src = prev && prev->first == address ? last_from : prev;
            if (src && last_insn(src->first, src->second) == "ret") ret_calls.insert(ncalls);
        }
        ncalls++;
        if (it != abi_calls.end()) e.known = (u8)(it->second | ESP_BIT);
        uc_mem_read(uc, sp + 4, e.stack, 20);
        events.push_back(e);
    }

    void returning(u32 result, u32 pop, bool has_result) {
        if (active && events.size() > EVENT_LIMIT) { flooded = true; uc_emu_stop(uc); return; }
        u32 sp = reg(UC_X86_REG_ESP), ret = rd32(sp);
        auto si = sites->find(ret);
        if (si == sites->end()) { halted = true; uc_emu_stop(uc); return; }
        Site site = si->second;
        wr32(sp - 4, reg(UC_X86_REG_EIP));
        auto c = canon.find(ret); if (c != canon.end()) wr32(sp, c->second);
        if (has_result) setreg(UC_X86_REG_EAX, result);
        auto a = abi_calls.find(ret);
        if (a != abi_calls.end()) {
            if (!(a->second & 2)) setreg(UC_X86_REG_ECX, POISON);
            if (!(a->second & 4)) setreg(UC_X86_REG_EDX, POISON);
        }
        setreg(UC_X86_REG_ESP, sp + 4 + pop + site.args);
        if (site.halt) { halted = true; uc_emu_stop(uc); }
        else setreg(UC_X86_REG_EIP, ret);
    }

    void reset() {
        for (auto [address, size] : dirty) {
            const u8 *src = nullptr; size_t avail = 0;
            u32 start = 0;
            if (address >= BLOB) { start = BLOB; src = sh.blob.data(); avail = sh.blob.size(); }
            else if (address >= FUNCS) { start = FUNCS; src = funcs_pristine.data(); avail = funcs_pristine.size(); }
            else if (address >= base) { start = base; src = sh.image.data(); avail = sh.image.size(); }
            else continue;
            size_t off = address - start;
            if (off + size <= avail) { uc_mem_write(uc, address, src + off, size); remove_cache(address, (u64)address + size); }
        }
        dirty.clear();
        for (u32 page : lazy) uc_mem_unmap(uc, page, 0x1000);
        lazy.clear();
        uc_context_restore(uc, context);
    }

    Result run(const State &st, u32 entry, u64 count, const std::vector<std::pair<u32, std::vector<u8>>> &cal, std::unordered_map<u32, Site> *s) {
        reset(); sites = s ? s : &empty_sites; prev.reset(); rets.clear(); ninsn.clear(); callees.clear(); stubs.clear();
        for (auto &c : cal) callees.insert(c.first);
        std::vector<std::pair<u32, std::vector<u8>>> saved;
        for (auto &[t, code] : cal) { std::vector<u8> old(code.size()); uc_mem_read(uc, t, old.data(), old.size()); saved.push_back({t, old}); }
        for (auto &[t, code] : cal) { uc_mem_write(uc, t, code.data(), code.size()); remove_cache(t, (u64)t + code.size()); }
        for (auto &[t, code] : cal) {
            std::optional<u32> k; u32 pop = 0;
            if (code[0] == 0xb8) { u32 v; memcpy(&v, &code[1], 4); k = v; }
            if (code.size() >= 3 && code[code.size() - 3] == 0xc2) pop = code[code.size() - 2] | code[code.size() - 1] << 8;
            stubs[t] = {k, pop};
        }
        std::vector<uc_hook> hooks;
        for (auto &c : cal) { uc_hook h; uc_hook_add(uc, &h, UC_HOOK_CODE, (void *)hk_called, this, c.first, c.first); hooks.push_back(h); }
        Result r = run_(st, entry, count);
        for (auto h : hooks) uc_hook_del(uc, h);
        for (auto &[t, old] : saved) { uc_mem_write(uc, t, old.data(), old.size()); remove_cache(t, (u64)t + old.size()); }
        return r;
    }

    Result run_(const State &st, u32 entry, u64 count) {
        uc_mem_write(uc, ARENA, st.arena.data(), st.arena.size());
        std::vector<u8> zero(0x10000, 0); uc_mem_write(uc, STACK, zero.data(), zero.size());
        uc_mem_write(uc, ENTRY_SP, st.stack.data(), st.stack.size());
        { auto t = teb_page(); uc_mem_write(uc, TEB, t.data(), t.size()); }
        if (shadow_hi) { std::vector<u8> z(shadow_hi - shadow_lo, 0); uc_mem_write(uc, shadow_lo, z.data(), z.size()); }
        u8 digest[32]; sha256(st.arena.data(), 256, digest); memcpy(&state_seed, digest, 8);
        std::vector<u8> code(0x1000, 0); code[0] = 0x90; uc_mem_write(uc, CODE, code.data(), code.size()); remove_cache(CODE, CODE + 0x1000);
        u64 v; v = 0x37f; uc_reg_write(uc, UC_X86_REG_FPCW, &v); v = 0; uc_reg_write(uc, UC_X86_REG_FPSW, &v); v = 0xffff; uc_reg_write(uc, UC_X86_REG_FPTAG, &v);
        v = 0x1f80; uc_reg_write(uc, UC_X86_REG_MXCSR, &v);
        u64 z[2] = {0, 0}; for (int n = 0; n < 8; n++) uc_reg_write(uc, UC_X86_REG_XMM0 + n, z);
        for (int i = 0; i < 8; i++) setreg(GPRS[i], st.regs[i]);
        setreg(UC_X86_REG_ESP, ENTRY_SP); setreg(UC_X86_REG_EFLAGS, st.flags);
        if (!st.x87.empty()) {
            std::vector<u8> loader, fl;
            for (size_t k = 0; k < st.x87.size(); k++) { loader.push_back(0xd9); loader.push_back(0x05); pack32(loader, ARENA + ARENA_SIZE - 0x100 + 4 * (u32)k); pack32(fl, fbits(st.x87[k])); }
            uc_mem_write(uc, ARENA + ARENA_SIZE - 0x100, fl.data(), fl.size());
            uc_mem_write(uc, LOADER, loader.data(), loader.size()); uc_emu_start(uc, LOADER, LOADER + loader.size(), 0, 0);
        }
        events.clear(); pushad_eip = 0; active = true; halted = false; ret_calls.clear(); ncalls = 0; left = false; frame_top = 0; flooded = false;
        u64 began = now_us(), limit = began + (u64)(RUN_SECONDS * 1e6); deadline = limit; ticks = 0;
        bool done = false; std::string error; bool errored = false;
        u32 start = entry;
        for (int opaque = 0; opaque <= OPAQUE_CALLS; opaque++) {

            executed = 0; budget = count;  // per start, as emu_start's count
            uc_err e = uc_emu_start(uc, start, STOP, 0, 0);
            if (e == UC_ERR_OK) break;
            if (e != UC_ERR_FETCH_UNMAPPED && e != UC_ERR_FETCH_PROT) { errored = true; error = uc_strerror(e); break; }
            if (opaque == OPAQUE_CALLS) break;  // out of opaque calls: out of budget
            u32 target = reg(UC_X86_REG_EIP), sp = reg(UC_X86_REG_ESP);
            call_event(target, sp);
            returning(opaque < 16 ? opaque_result(target) : opaque_result(target, opaque), 0, true);
            if (halted || flooded || now_us() >= limit) break;
            start = reg(UC_X86_REG_EIP);
        }
        if (!errored) { done = !left && (halted || reg(UC_X86_REG_EIP) == STOP); if (left) error = "left the function"; }
        active = false;
        Result r; r.events.swap(events);
        u32 final_sp = reg(UC_X86_REG_ESP);
        if (done && !(STACK <= final_sp && final_sp <= STACK + 0x10000 && (halted || final_sp > ENTRY_SP))) { done = false; error = "stack lost"; }
        if (!done) {
            if (error.empty()) error = now_us() - began >= (u64)(RUN_SECONDS * 1e6) && !flooded ? "timeout" : "budget";
            r.done = false; r.error = error; return r;
        }
        r.done = true; r.known = 0xff;
        for (int i = 0; i < 8; i++) r.regs[i] = reg(GPRS[i]);
        if (ret_mask && !halted) r.known = (u8)(*ret_mask | ESP_BIT);
        uc_reg_read(uc, UC_X86_REG_FPCW, &r.fpu_ctl[0]); uc_reg_read(uc, UC_X86_REG_FPSW, &r.fpu_ctl[1]); uc_reg_read(uc, UC_X86_REG_FPTAG, &r.fpu_ctl[2]);
        for (int n = 0; n < 8; n++) { u8 b[16] = {}; uc_reg_read(uc, UC_X86_REG_FP0 + n, b); memcpy(&r.fp[n].m, b, 8); memcpy(&r.fp[n].e, b + 8, 2); }
        uc_reg_read(uc, UC_X86_REG_MXCSR, &r.mxcsr);
        for (int n = 0; n < 8; n++) uc_reg_read(uc, UC_X86_REG_XMM0 + n, r.xmm[n]);
        std::vector<Event> kept;
        for (auto &e : r.events) if (e.call || !(STACK <= e.addr && e.addr < final_sp)) kept.push_back(e);
        r.events.swap(kept);
        return r;
    }
};

// ---- post-processing (ret_view, as_native) ----
static void mask_regs(u32 *regs, u8 &known, u32 m) { known &= (u8)(m | ESP_BIT); (void)regs; }

static Result ret_view(const Result &r, const Machine &native) {
    if (!native.ret_mask || native.ret_calls.empty()) return r;
    Result out = r; u32 n = 0;
    for (auto &e : out.events) if (e.call) { if (native.ret_calls.count(n)) mask_regs(e.regs, e.known, *native.ret_mask); n++; }
    if (r.done && native.halted && native.ret_calls.count(native.ncalls - 1)) mask_regs(out.regs, out.known, *native.ret_mask);
    return out;
}

static Result as_native(Result r, const std::unordered_map<u32, u32> &returns) {
    if (returns.empty()) return r;
    auto fix = [&](u32 v) { auto it = returns.find(v); return it == returns.end() ? v : it->second; };
    for (auto &e : r.events) {
        if (e.call) {
            for (int i = 0; i < 8; i++) if (e.known >> i & 1) e.regs[i] = fix(e.regs[i]);
            for (int o = 0; o + 3 < 20; o += 4) { u32 w; memcpy(&w, e.stack + o, 4); w = fix(w); memcpy(e.stack + o, &w, 4); }
        } else if (e.size == 4 && e.has) e.val = fix((u32)e.val);
    }
    if (r.done) for (int i = 0; i < 8; i++) r.regs[i] = fix(r.regs[i]);
    return r;
}

// ---- failure summaries (JSON) ----
static std::string hx(u32 v) { char b[16]; snprintf(b, sizeof b, "\"%08x\"", v); return b; }
static std::string esc(const std::string &s) { std::string o; for (char c : s) { if (c == '"' || c == '\\') o += '\\'; o += c; } return o; }
static std::string events_json(const std::vector<Event> &ev, size_t from = 0, size_t count = 40) {
    std::string o = "[";
    for (size_t i = from; i < ev.size() && i < from + count; i++) {
        const Event &e = ev[i]; if (i > from) o += ", ";
        if (e.call) {
            o += "[\"call\", " + hx(e.addr) + ", [";
            for (int k = 0; k < 8; k++) { if (k) o += ", "; o += e.known >> k & 1 ? hx(e.regs[k]) : "\"-\""; }
            o += "], \""; char b[4]; for (u8 c : e.stack) { snprintf(b, sizeof b, "%02x", c); o += b; } o += "\"]";
        } else {
            char b[64]; snprintf(b, sizeof b, "[%s, %d, ", hx(e.addr).c_str(), e.size); o += b;
            if (e.has) { snprintf(b, sizeof b, "\"%llx\"]", (unsigned long long)e.val); o += b; } else o += "null]";
        }
    }
    return o + "]";
}
static std::string summary(const Result &r) {
    if (!r.done) return "{\"error\": \"" + esc(r.error) + "\", \"events\": " + events_json(r.events) + "}";
    std::string o = "{\"regs\": [";
    for (int k = 0; k < 8; k++) { if (k) o += ", "; o += r.known >> k & 1 ? hx(r.regs[k]) : "\"-\""; }
    o += "], \"fpu\": [";
    char b[96];
    for (int k = 0; k < 3; k++) { snprintf(b, sizeof b, "%s\"%llu\"", k ? ", " : "", (unsigned long long)r.fpu_ctl[k]); o += b; }
    for (int k = 0; k < 8; k++) { snprintf(b, sizeof b, ", \"(%llu, %u)\"", (unsigned long long)r.fp[k].m, r.fp[k].e); o += b; }
    snprintf(b, sizeof b, "], \"sse\": [\"%llx\"", (unsigned long long)r.mxcsr); o += b;
    for (int k = 0; k < 8; k++) {
        if (r.xmm[k][1]) snprintf(b, sizeof b, ", \"%llx%016llx\"", (unsigned long long)r.xmm[k][1], (unsigned long long)r.xmm[k][0]);
        else snprintf(b, sizeof b, ", \"%llx\"", (unsigned long long)r.xmm[k][0]);
        o += b;
    }
    return o + "], \"events\": " + events_json(r.events) + "}";
}

// ---- one function ----
struct Outcome { std::string line; };

static std::string verify_one(Machine &native, Machine &lifted, Job &j, int states, u64 seed) {
    int completed = 0, skipped = 0, prefixed = 0; std::string failure;
    native.home = lifted.home = j.home;
    native.abi_calls = j.abi; lifted.abi_calls = j.labi; lifted.canon = j.returns; native.canon.clear();
    native.ret_mask = lifted.ret_mask = j.ret_mask;
    int out_of_budget = 0;
    std::map<std::string, int> why;  // native run errors, printed with VCORE_WHY set
    auto rng_for = [&]() { return PyRandom((unsigned __int128)(seed * 1000003ull + j.home)); };
    PyRandom rng = rng_for();
    const u64 budgets[2][2] = {{20000, 2000000}, {BIG_BUDGET, 20 * BIG_BUDGET}};
    for (int round = 0; round < 2; round++) {
        u64 budget = budgets[round][0], lbudget = budgets[round][1];
        if (round) { if (completed || !failure.empty() || !out_of_budget) break; rng = rng_for(); skipped = 0; }
        for (int k = 0; k < 3 * states; k++) {
            if (completed >= states) break;
            State st = random_state(rng, k, k >= states);
            Result a = native.run(st, j.home, budget, j.callees, &j.sites);
            if (!a.done && (a.error == "stack lost" || a.error == "left the function" || a.error == "budget")) { st = frame_state(st); a = native.run(st, j.home, budget, j.callees, &j.sites); }
            if (!a.done) why[a.error]++;
            if (!a.done && (a.error == "budget" || a.error == "left the function" || a.error == "timeout")) { skipped++; out_of_budget += a.error == "budget"; continue; }
            Result b = as_native(lifted.run(st, j.entry, lbudget, j.callees, &j.lsites), j.returns);
            a = ret_view(a, native); b = ret_view(b, native);
            if (!b.done && b.error == "timeout") { skipped++; why["lifted timeout"]++; continue; }
            if (!a.done) {
                skipped++;
                if (b.done) failure = "{\"state\": " + std::to_string(k) + ", \"why\": \"native faulted, lifted completed\", \"native\": \"" + esc(a.error) + "\"}";
            } else if (!(a == b)) failure = "{\"state\": " + std::to_string(k) + ", \"why\": \"mismatch\", \"native\": " + summary(a) + ", \"lifted\": " + summary(b) + "}";
            else completed++;
            if (!failure.empty()) break;
        }
    }
    if (!completed && failure.empty() && out_of_budget) {
        PyRandom rng2 = rng_for();
        for (int k = 0; k < 3 * states; k++) {
            if (prefixed >= states) break;
            State st = random_state(rng2, k, k >= states);
            Result a = native.run(st, j.home, PREFIX_BUDGET, j.callees, &j.sites);
            if (!a.done && (a.error == "stack lost" || a.error == "left the function" || a.error == "budget")) { st = frame_state(st); a = native.run(st, j.home, PREFIX_BUDGET, j.callees, &j.sites); }
            if (a.done || a.error != "budget" || a.events.empty()) continue;
            Result b = as_native(lifted.run(st, j.entry, 100 * PREFIX_BUDGET, j.callees, &j.lsites), j.returns);
            a = ret_view(a, native); b = ret_view(b, native);
            size_t n = a.events.size();
            if (b.done || b.error == "timeout") continue;
            if (b.events.size() < n) {
                if (b.error == "budget" || !std::equal(b.events.begin(), b.events.end(), a.events.begin())) continue;
                failure = "{\"state\": " + std::to_string(k) + ", \"why\": \"lifted faulted inside the native trace prefix\", \"lifted\": \"" + esc(b.error) + "\", \"event\": " + std::to_string(b.events.size()) + "}";
                break;
            }
            if (!std::equal(a.events.begin(), a.events.end(), b.events.begin())) {
                size_t i = 0; while (a.events[i] == b.events[i]) i++;
                failure = "{\"state\": " + std::to_string(k) + ", \"why\": \"trace prefix mismatch\", \"native\": {\"events\": " + events_json(a.events, i, 5) +
                          "}, \"lifted\": {\"events\": " + events_json(b.events, i, 5) + "}, \"event\": " + std::to_string(i) + "}";
                break;
            }
            prefixed++;
        }
    }
    std::string status = !failure.empty() ? "failed" : completed || prefixed ? "passed" : "unverified";
    if (const char *path = getenv("VCORE_WHY"); path && status != "passed") {  // diagnostic file
        std::string w; for (auto &[k, n] : why) w += " " + k + "=" + std::to_string(n);
        static std::mutex lock; std::lock_guard<std::mutex> guard(lock);
        if (FILE *f = fopen(path, "a")) { fprintf(f, "%s %s%s\n", j.name.c_str(), status.c_str(), w.c_str()); fclose(f); }
    }
    char b[160]; snprintf(b, sizeof b, "%s %s %d %d %d ", j.name.c_str(), status.c_str(), completed, skipped, prefixed);
    return std::string(b) + (failure.empty() ? "-" : failure);
}

// ---- job file ----
static std::vector<u8> read_file(const std::string &p) { std::ifstream f(p, std::ios::binary); return std::vector<u8>((std::istreambuf_iterator<char>(f)), {}); }
static std::vector<u8> unhex(const std::string &s) { std::vector<u8> o; for (size_t i = 0; i + 1 < s.size(); i += 2) o.push_back((u8)std::stoul(s.substr(i, 2), nullptr, 16)); return o; }

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: vcore <job> <results> [threads]\n"); return 2; }
    int threads = argc > 3 ? atoi(argv[3]) : (int)std::thread::hardware_concurrency();
    std::ifstream in(argv[1]); std::string line;
    Shared sh; int states = 8; u64 seed = 1; std::vector<Job> jobs;
    while (std::getline(in, line)) {
        std::istringstream s(line); std::string tag; s >> tag;
        auto h = [&]() { std::string x; s >> x; return (u32)std::stoul(x, nullptr, 16); };
        auto path = [&]() { std::string p; std::getline(s >> std::ws, p); return p; };  // the rest of the line (paths have spaces)
        if (tag == "image") { sh.base = h(); sh.size_of_image = h(); sh.image = read_file(path()); }
        else if (tag == "code") { u32 lo = h(); sh.code.push_back({lo, h()}); }
        else if (tag == "blob") { sh.blob = read_file(path()); }
        else if (tag == "quiet") { sh.quiet0 = h(); sh.quiet1 = h(); sh.quiet2 = h(); }
        else if (tag == "states") { s >> states >> seed; }
        else if (tag == "shadow") { sh.shadow_lo = h(); sh.shadow_hi = h(); }
        else if (tag == "E") { Job j; s >> j.name; j.entry = h(); j.home = h(); jobs.push_back(std::move(j)); }
        else if (tag == "C") { u32 t = h(); std::string c; s >> c; jobs.back().callees.push_back({t, unhex(c)}); }
        else if (tag == "S" || tag == "L") { u32 r = h(); u32 pop; int halt; s >> pop >> halt; (tag == "S" ? jobs.back().sites : jobs.back().lsites)[r] = {pop, halt != 0}; }
        else if (tag == "R") { u32 a = h(); jobs.back().returns[a] = h(); }
        else if (tag == "A") { u32 a = h(); u32 m; s >> m; jobs.back().abi[a] = m; }
        else if (tag == "B") { u32 a = h(); u32 m; s >> m; jobs.back().labi[a] = m; }
        else if (tag == "M") { u32 m; s >> m; jobs.back().ret_mask = m; }
    }
    if (sh.image.empty() || sh.blob.empty()) { fprintf(stderr, "vcore: missing image or blob\n"); return 2; }
    std::vector<std::string> out(jobs.size());
    std::atomic<size_t> next{0}, finished{0}; std::mutex io;
    auto started = now_us();
    auto worker = [&]() {
        Machine native(sh, false), lifted(sh, true);
        for (size_t i; (i = next++) < jobs.size();) {
            out[i] = verify_one(native, lifted, jobs[i], states, seed);
            size_t f = ++finished;
            if (f % 1000 == 0) { std::lock_guard<std::mutex> g(io); fprintf(stderr, "  %zu/%zu checked, %.0fs\n", f, jobs.size(), (now_us() - started) / 1e6); }
        }
    };
    std::vector<std::thread> pool;
    for (int t = 0; t < threads; t++) pool.emplace_back(worker);
    for (auto &t : pool) t.join();
    std::ofstream o(argv[2]);
    for (auto &l : out) o << l << "\n";
    return 0;
}
