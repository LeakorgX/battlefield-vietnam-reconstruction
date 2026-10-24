/* SPIR-V module builder. */
#include "spirv.h"
#include "vkp.h"

void spv_put(struct spv_buf *b, uint32_t w)
{
    if (b->n == b->cap) {
        uint32_t cap = b->cap ? b->cap * 2 : 256;
        uint32_t *w2 = vkp_realloc(b->w, cap * 4);
        if (!w2) return;
        b->w = w2; b->cap = cap;
    }
    b->w[b->n++] = w;
}

static void put_all(struct spv_buf *b, const uint32_t *w, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) spv_put(b, w[i]);
}

void spv_inst(struct spv_buf *b, uint32_t op, const uint32_t *w, uint32_t n)
{
    spv_put(b, op | (n + 1) << 16);
    put_all(b, w, n);
}

static uint32_t string_words(const char *str) { return (uint32_t)(vkp_strlen(str) / 4 + 1); }

static void put_string(struct spv_buf *b, const char *str)
{
    uint32_t n = (uint32_t)vkp_strlen(str);
    for (uint32_t i = 0; i <= n; i += 4) {
        uint32_t w = 0;
        for (uint32_t k = 0; k < 4 && i + k < n; k++) w |= (uint32_t)(uint8_t)str[i + k] << (8 * k);
        spv_put(b, w);
    }
}

void spv_string(struct spv_buf *b, const char *str) { put_string(b, str); }

void spv_init(struct spv *s)
{
    memset(s, 0, sizeof *s);
    s->bound = 1;
    s->glsl = spv_id(s);
    spv_put(&s->imports, SpvOpExtInstImport | (2 + string_words("GLSL.std.450")) << 16);
    spv_put(&s->imports, s->glsl);
    put_string(&s->imports, "GLSL.std.450");
    spv_inst(&s->memmodel, SpvOpMemoryModel, (const uint32_t[]){0 /* Logical */, 1 /* GLSL450 */}, 2);
}

static void buf_free(struct spv_buf *b) { vkp_free(b->w); b->w = 0; b->n = b->cap = 0; }

void spv_free(struct spv *s)
{
    struct spv_buf *all[] = {&s->caps, &s->exts, &s->imports, &s->memmodel, &s->entries, &s->modes, &s->debug,
                             &s->annots, &s->types, &s->funcs, &s->fvars, &s->fbody, &s->keys};
    for (unsigned i = 0; i < VKP_COUNT(all); i++) buf_free(all[i]);
    vkp_free(s->table); s->table = 0;
}

uint32_t *spv_finish(struct spv *s, uint32_t *count)
{
    struct spv_buf *all[] = {&s->caps, &s->exts, &s->imports, &s->memmodel, &s->entries, &s->modes, &s->debug,
                             &s->annots, &s->types, &s->funcs};
    uint32_t n = 5;
    for (unsigned i = 0; i < VKP_COUNT(all); i++) n += all[i]->n;
    uint32_t *out = vkp_alloc(n * 4);
    if (!out) return 0;
    out[0] = 0x07230203; out[1] = 0x00010300; /* SPIR-V 1.3 */
    out[2] = 0; out[3] = s->bound; out[4] = 0;
    uint32_t o = 5;
    for (unsigned i = 0; i < VKP_COUNT(all); i++) {
        if (all[i]->n) memcpy(out + o, all[i]->w, all[i]->n * 4);
        o += all[i]->n;
    }
    *count = n;
    return out;
}

uint32_t spv_id(struct spv *s) { return s->bound++; }

void spv_capability(struct spv *s, uint32_t cap)
{
    for (uint32_t i = 0; i < s->caps.n; i += 2) if (s->caps.w[i + 1] == cap) return;
    spv_inst(&s->caps, SpvOpCapability, &cap, 1);
}

void spv_extension(struct spv *s, const char *name)
{
    spv_put(&s->exts, SpvOpExtension | (1 + string_words(name)) << 16);
    put_string(&s->exts, name);
}

void spv_entry(struct spv *s, uint32_t model, uint32_t fn, const char *name, const uint32_t *ifaces, uint32_t n)
{
    spv_put(&s->entries, SpvOpEntryPoint | (3 + string_words(name) + n) << 16);
    spv_put(&s->entries, model);
    spv_put(&s->entries, fn);
    put_string(&s->entries, name);
    put_all(&s->entries, ifaces, n);
}

void spv_mode(struct spv *s, uint32_t fn, uint32_t mode)
{
    spv_inst(&s->modes, SpvOpExecutionMode, (const uint32_t[]){fn, mode}, 2);
}

void spv_decorate(struct spv *s, uint32_t id, uint32_t dec, int nargs, ...)
{
    va_list ap;
    va_start(ap, nargs);
    spv_put(&s->annots, SpvOpDecorate | (3 + nargs) << 16);
    spv_put(&s->annots, id);
    spv_put(&s->annots, dec);
    for (int i = 0; i < nargs; i++) spv_put(&s->annots, va_arg(ap, uint32_t));
    va_end(ap);
}

void spv_member_decorate(struct spv *s, uint32_t id, uint32_t member, uint32_t dec, int nargs, ...)
{
    va_list ap;
    va_start(ap, nargs);
    spv_put(&s->annots, SpvOpMemberDecorate | (4 + nargs) << 16);
    spv_put(&s->annots, id);
    spv_put(&s->annots, member);
    spv_put(&s->annots, dec);
    for (int i = 0; i < nargs; i++) spv_put(&s->annots, va_arg(ap, uint32_t));
    va_end(ap);
}

void spv_name(struct spv *s, uint32_t id, const char *name)
{
    spv_put(&s->debug, SpvOpName | (2 + string_words(name)) << 16);
    spv_put(&s->debug, id);
    put_string(&s->debug, name);
}

/* Cache: open-addressed table of offsets into `keys`. */
static uint32_t key_hash(uint32_t op, const uint32_t *w, uint32_t n)
{
    uint32_t h = 2166136261u ^ op;
    for (uint32_t i = 0; i < n; i++) h = (h ^ w[i]) * 16777619u;
    return h ? h : 1;
}

static int key_equal(struct spv *s, uint32_t off, uint32_t op, const uint32_t *w, uint32_t n)
{
    const uint32_t *k = s->keys.w + off;
    if (k[0] != n || k[1] != op) return 0;
    for (uint32_t i = 0; i < n; i++) if (k[2 + i] != w[i]) return 0;
    return 1;
}

static void table_grow(struct spv *s)
{
    uint32_t cap = s->table_cap ? s->table_cap * 2 : 1024;
    uint32_t *t = vkp_alloc(cap * 4);
    if (!t) { s->failed = 1; return; }
    for (uint32_t i = 0; i < s->table_cap; i++) {
        uint32_t off = s->table[i];
        if (!off) continue;
        const uint32_t *k = s->keys.w + off - 1;
        uint32_t h = key_hash(k[1], k + 2, k[0]);
        for (uint32_t j = h & (cap - 1);; j = (j + 1) & (cap - 1)) if (!t[j]) { t[j] = off; break; }
    }
    vkp_free(s->table);
    s->table = t; s->table_cap = cap;
}

uint32_t spv_cached(struct spv *s, uint32_t op, const uint32_t *w, uint32_t n)
{
    if ((s->table_n + 1) * 2 > s->table_cap) table_grow(s);
    if (s->failed) return 0;
    uint32_t h = key_hash(op, w, n), j;
    for (j = h & (s->table_cap - 1); s->table[j]; j = (j + 1) & (s->table_cap - 1)) {
        uint32_t off = s->table[j] - 1;
        if (key_equal(s, off, op, w, n)) return s->keys.w[off + 2 + n];
    }
    uint32_t id = spv_id(s);
    s->table[j] = s->keys.n + 1;
    s->table_n++;
    spv_put(&s->keys, n);
    spv_put(&s->keys, op);
    put_all(&s->keys, w, n);
    spv_put(&s->keys, id);
    /* emit: types have the result first, constants the type first */
    int typed = op == SpvOpConstant || op == SpvOpConstantComposite || op == SpvOpConstantTrue ||
                op == SpvOpConstantFalse || op == SpvOpConstantNull;
    spv_put(&s->types, op | (n + 2) << 16);
    if (typed) { spv_put(&s->types, w[0]); spv_put(&s->types, id); put_all(&s->types, w + 1, n - 1); }
    else { spv_put(&s->types, id); put_all(&s->types, w, n); }
    return id;
}

uint32_t spv_t_void(struct spv *s) { return spv_cached(s, SpvOpTypeVoid, 0, 0); }
uint32_t spv_t_bool(struct spv *s) { return spv_cached(s, SpvOpTypeBool, 0, 0); }
uint32_t spv_t_int(struct spv *s, int sg) { return spv_cached(s, SpvOpTypeInt, (const uint32_t[]){32, sg ? 1u : 0u}, 2); }
uint32_t spv_t_float(struct spv *s) { return spv_cached(s, SpvOpTypeFloat, (const uint32_t[]){32}, 1); }
uint32_t spv_t_vec(struct spv *s, uint32_t comp, uint32_t n)
{
    return n == 1 ? comp : spv_cached(s, SpvOpTypeVector, (const uint32_t[]){comp, n}, 2);
}
uint32_t spv_t_array(struct spv *s, uint32_t elem, uint32_t len)
{
    return spv_cached(s, SpvOpTypeArray, (const uint32_t[]){elem, spv_c_u32(s, len)}, 2);
}
uint32_t spv_t_ptr(struct spv *s, uint32_t storage, uint32_t type)
{
    return spv_cached(s, SpvOpTypePointer, (const uint32_t[]){storage, type}, 2);
}
uint32_t spv_t_fn(struct spv *s, uint32_t ret, const uint32_t *args, uint32_t n)
{
    uint32_t w[16];
    w[0] = ret;
    for (uint32_t i = 0; i < n && i < 15; i++) w[1 + i] = args[i];
    return spv_cached(s, SpvOpTypeFunction, w, n + 1);
}
uint32_t spv_t_image(struct spv *s, uint32_t dim, uint32_t depth, uint32_t arrayed)
{
    return spv_cached(s, SpvOpTypeImage, (const uint32_t[]){spv_t_float(s), dim, depth, arrayed, 0, 1, 0}, 7);
}
uint32_t spv_t_sampled(struct spv *s, uint32_t image) { return spv_cached(s, SpvOpTypeSampledImage, &image, 1); }
uint32_t spv_t_struct(struct spv *s, const uint32_t *members, uint32_t n)
{
    uint32_t id = spv_id(s);
    spv_put(&s->types, SpvOpTypeStruct | (n + 2) << 16);
    spv_put(&s->types, id);
    put_all(&s->types, members, n);
    return id;
}
uint32_t spv_c_f32(struct spv *s, float v)
{
    union { float f; uint32_t u; } x = { .f = v };
    return spv_cached(s, SpvOpConstant, (const uint32_t[]){spv_t_float(s), x.u}, 2);
}
uint32_t spv_c_u32(struct spv *s, uint32_t v) { return spv_cached(s, SpvOpConstant, (const uint32_t[]){spv_t_int(s, 0), v}, 2); }
uint32_t spv_c_i32(struct spv *s, int32_t v) { return spv_cached(s, SpvOpConstant, (const uint32_t[]){spv_t_int(s, 1), (uint32_t)v}, 2); }
uint32_t spv_c_bool(struct spv *s, int v)
{
    uint32_t t = spv_t_bool(s);
    return spv_cached(s, v ? SpvOpConstantTrue : SpvOpConstantFalse, &t, 1);
}
uint32_t spv_c_comp(struct spv *s, uint32_t type, const uint32_t *ids, uint32_t n)
{
    uint32_t w[17];
    w[0] = type;
    for (uint32_t i = 0; i < n && i < 16; i++) w[1 + i] = ids[i];
    return spv_cached(s, SpvOpConstantComposite, w, n + 1);
}
uint32_t spv_c_vec4f(struct spv *s, float x, float y, float z, float w)
{
    uint32_t c[4] = {spv_c_f32(s, x), spv_c_f32(s, y), spv_c_f32(s, z), spv_c_f32(s, w)};
    return spv_c_comp(s, spv_t_vec(s, spv_t_float(s), 4), c, 4);
}
uint32_t spv_c_null(struct spv *s, uint32_t type) { return spv_cached(s, SpvOpConstantNull, &type, 1); }

uint32_t spv_var(struct spv *s, uint32_t ptr_type, uint32_t storage)
{
    uint32_t id = spv_id(s);
    spv_inst(&s->types, SpvOpVariable, (const uint32_t[]){ptr_type, id, storage}, 3);
    return id;
}

uint32_t spv_fvar(struct spv *s, uint32_t ptr_type)
{
    uint32_t id = spv_id(s);
    spv_inst(&s->fvars, SpvOpVariable, (const uint32_t[]){ptr_type, id, SpvStorageFunction}, 3);
    return id;
}

void spv_func_begin(struct spv *s, uint32_t id, uint32_t ret, uint32_t fn_type)
{
    spv_inst(&s->funcs, SpvOpFunction, (const uint32_t[]){ret, id, 0, fn_type}, 4);
    s->fvars.n = s->fbody.n = 0;
}

void spv_func_end(struct spv *s)
{
    /* first OpLabel, then the hoisted variables, then the rest */
    if (s->fbody.n >= 2) {
        put_all(&s->funcs, s->fbody.w, 2);
        put_all(&s->funcs, s->fvars.w, s->fvars.n);
        put_all(&s->funcs, s->fbody.w + 2, s->fbody.n - 2);
    }
    spv_inst(&s->funcs, SpvOpFunctionEnd, 0, 0);
    s->fvars.n = s->fbody.n = 0;
}

uint32_t spv_label(struct spv *s, uint32_t id)
{
    if (!id) id = spv_id(s);
    spv_inst(&s->fbody, SpvOpLabel, &id, 1);
    return id;
}

uint32_t spv_op(struct spv *s, uint32_t op, uint32_t type, const uint32_t *args, uint32_t n)
{
    uint32_t id = spv_id(s);
    spv_put(&s->fbody, op | (n + 3) << 16);
    spv_put(&s->fbody, type);
    spv_put(&s->fbody, id);
    put_all(&s->fbody, args, n);
    return id;
}

void spv_opv(struct spv *s, uint32_t op, const uint32_t *args, uint32_t n) { spv_inst(&s->fbody, op, args, n); }

uint32_t spv_ext(struct spv *s, uint32_t type, uint32_t inst, const uint32_t *args, uint32_t n)
{
    uint32_t id = spv_id(s);
    spv_put(&s->fbody, SpvOpExtInst | (n + 5) << 16);
    spv_put(&s->fbody, type);
    spv_put(&s->fbody, id);
    spv_put(&s->fbody, s->glsl);
    spv_put(&s->fbody, inst);
    put_all(&s->fbody, args, n);
    return id;
}

uint32_t spv_load(struct spv *s, uint32_t type, uint32_t ptr) { return S_OP(s, SpvOpLoad, type, ptr); }
void spv_store(struct spv *s, uint32_t ptr, uint32_t value) { S_OPV(s, SpvOpStore, ptr, value); }
uint32_t spv_extract(struct spv *s, uint32_t type, uint32_t comp, uint32_t index)
{
    return S_OP(s, SpvOpCompositeExtract, type, comp, index);
}
uint32_t spv_shuffle(struct spv *s, uint32_t type, uint32_t a, uint32_t b, const uint32_t *sel, uint32_t n)
{
    uint32_t w[6] = {a, b};
    for (uint32_t i = 0; i < n && i < 4; i++) w[2 + i] = sel[i];
    return spv_op(s, SpvOpVectorShuffle, type, w, n + 2);
}
uint32_t spv_construct(struct spv *s, uint32_t type, const uint32_t *ids, uint32_t n)
{
    return spv_op(s, SpvOpCompositeConstruct, type, ids, n);
}
void spv_branch(struct spv *s, uint32_t label) { S_OPV(s, SpvOpBranch, label); }
