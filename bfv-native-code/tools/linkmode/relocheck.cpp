// relocheck: proves that two builds differ only in 32-bit address fields.
//
//   relocheck.exe A.bin B.bin delta_hex
//     A is linked with every a_XXXXXXXX at XXXXXXXX, B at XXXXXXXX + delta.
//     Each differing byte must sit in a little-endian 32-bit field whose value
//     in B is its value in A plus delta (an absolute field, or a pc-relative
//     one: both move by delta).
//
//   relocheck.exe --map A.bin B.bin va_hex map.tsv
//     A and B are the same section, loaded at va. map.tsv lines "old new"
//     (hex) move a symbol from old to new. Each differing byte must sit in a
//     field that holds old in A and new in B, absolutely or as a rel32 whose
//     target (field va + 4 + value) is old in A and new in B.
//
//   relocheck.exe --ranges A.bin B.bin va_a_hex va_b_hex ranges.tsv
//     A is a section at va_a, B the same section relinked at va_b after a
//     size change. ranges.tsv lines "start end delta" (hex): addresses in
//     [start, end) moved by delta. Each differing byte must sit in a field
//     whose target, absolute or rel32, lies in a range and moved by its delta.
//
// Prints the field count and the number of bytes no field explains; exit
// status 1 when any.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <vector>

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

static uint32_t le32(const std::vector<uint8_t> &d, size_t i) {
    return d[i] | d[i + 1] << 8 | d[i + 2] << 16 | (uint32_t)d[i + 3] << 24;
}

template <class Fits>
static int check(const std::vector<uint8_t> &a, const std::vector<uint8_t> &b, Fits fits) {
    if (a.size() != b.size()) { std::printf("sizes differ: %zu %zu\n", a.size(), b.size()); return 1; }
    size_t fields = 0, unexplained = 0, first = (size_t)-1;
    size_t i = 0, n = a.size();
    while (i < n) {
        if (a[i] == b[i]) { ++i; continue; }
        // the field starts at most 3 bytes back, after the last field consumed
        bool found = false;
        for (size_t back = 0; back <= 3 && !found && back <= i; ++back) {
            size_t j = i - back;
            if (j + 4 > n) continue;
            bool same_before = true;
            for (size_t k = j; k < i; ++k) same_before &= a[k] == b[k];
            if (same_before && fits(j, le32(a, j), le32(b, j))) {
                ++fields;
                i = j + 4;
                found = true;
            }
        }
        if (!found) {
            if (first == (size_t)-1) first = i;
            ++unexplained;
            ++i;
        }
    }
    std::printf("%zu moved field(s), %zu unexplained byte(s)", fields, unexplained);
    if (unexplained) std::printf(", first at +0x%zx", first);
    std::printf("\n");
    return unexplained ? 1 : 0;
}

int main(int argc, char **argv) {
    if (argc == 4) {
        std::vector<uint8_t> a = load(argv[1]), b = load(argv[2]);
        uint32_t delta = (uint32_t)std::strtoul(argv[3], nullptr, 16);
        return check(a, b, [&](size_t, uint32_t x, uint32_t y) { return y - x == delta; });
    }
    if (argc == 6 && !std::strcmp(argv[1], "--map")) {
        std::vector<uint8_t> a = load(argv[2]), b = load(argv[3]);
        uint32_t va = (uint32_t)std::strtoul(argv[4], nullptr, 16);
        std::unordered_map<uint32_t, uint32_t> moved;
        FILE *f = std::fopen(argv[5], "r");
        if (!f) { std::fprintf(stderr, "cannot open %s\n", argv[5]); return 2; }
        unsigned long from, to;
        while (std::fscanf(f, "%lx %lx", &from, &to) == 2) moved[(uint32_t)from] = (uint32_t)to;
        std::fclose(f);
        auto maps = [&](uint32_t x, uint32_t y) {
            auto it = moved.find(x);
            return it != moved.end() && it->second == y;
        };
        return check(a, b, [&](size_t j, uint32_t x, uint32_t y) {
            uint32_t next = va + (uint32_t)j + 4;
            return maps(x, y) || maps(x + next, y + next);
        });
    }
    if (argc == 7 && !std::strcmp(argv[1], "--ranges")) {
        std::vector<uint8_t> a = load(argv[2]), b = load(argv[3]);
        uint32_t va_a = (uint32_t)std::strtoul(argv[4], nullptr, 16);
        uint32_t va_b = (uint32_t)std::strtoul(argv[5], nullptr, 16);
        struct Range { uint32_t start, end, delta; };
        std::vector<Range> ranges;
        FILE *f = std::fopen(argv[6], "r");
        if (!f) { std::fprintf(stderr, "cannot open %s\n", argv[6]); return 2; }
        unsigned long start, end, delta;
        while (std::fscanf(f, "%lx %lx %lx", &start, &end, &delta) == 3)
            ranges.push_back({(uint32_t)start, (uint32_t)end, (uint32_t)delta});
        std::fclose(f);
        // VA and RVA ranges overlap (a small VA range holds large RVAs), so
        // any range that holds x and moved it by y - x explains the field
        auto maps = [&](uint32_t x, uint32_t y) {
            for (const Range &r : ranges)
                if (r.start <= x && x < r.end && y - x == r.delta) return true;
            return false;
        };
        return check(a, b, [&](size_t j, uint32_t x, uint32_t y) {
            return maps(x, y) || maps(x + va_a + (uint32_t)j + 4, y + va_b + (uint32_t)j + 4);
        });
    }
    std::fprintf(stderr, "usage: relocheck A.bin B.bin delta_hex | relocheck --map A.bin B.bin va_hex map.tsv"
                         " | relocheck --ranges A.bin B.bin va_a_hex va_b_hex ranges.tsv\n");
    return 2;
}
