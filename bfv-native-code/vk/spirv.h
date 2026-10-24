/* Minimal SPIR-V module builder: sections, deduplicated types and constants,
 * functions with their variables hoisted into the first block. */
#ifndef BFV_SPIRV_H
#define BFV_SPIRV_H
#include <stdint.h>
#include <stddef.h>

struct spv_buf { uint32_t *w; uint32_t n, cap; };

struct spv {
    uint32_t bound;
    struct spv_buf caps, exts, imports, memmodel, entries, modes, debug, annots, types, funcs;
    struct spv_buf fvars, fbody;    /* function being built */
    struct spv_buf keys;            /* type/constant cache: [len, op, operands..., id] */
    uint32_t *table; uint32_t table_cap, table_n;
    uint32_t glsl;                  /* GLSL.std.450 import */
    int failed;
};

/* SPIR-V enums used by the generators */
enum {
    SpvOpNop = 0, SpvOpUndef = 1, SpvOpName = 5, SpvOpMemberName = 6, SpvOpExtension = 10,
    SpvOpExtInstImport = 11, SpvOpExtInst = 12, SpvOpMemoryModel = 14, SpvOpEntryPoint = 15,
    SpvOpExecutionMode = 16, SpvOpCapability = 17, SpvOpTypeVoid = 19, SpvOpTypeBool = 20,
    SpvOpTypeInt = 21, SpvOpTypeFloat = 22, SpvOpTypeVector = 23, SpvOpTypeMatrix = 24,
    SpvOpTypeImage = 25, SpvOpTypeSampler = 26, SpvOpTypeSampledImage = 27, SpvOpTypeArray = 28,
    SpvOpTypeRuntimeArray = 29, SpvOpTypeStruct = 30, SpvOpTypePointer = 32, SpvOpTypeFunction = 33,
    SpvOpConstantTrue = 41, SpvOpConstantFalse = 42, SpvOpConstant = 43, SpvOpConstantComposite = 44,
    SpvOpConstantNull = 46, SpvOpSpecConstant = 50,
    SpvOpFunction = 54, SpvOpFunctionParameter = 55, SpvOpFunctionEnd = 56, SpvOpFunctionCall = 57,
    SpvOpVariable = 59, SpvOpLoad = 61, SpvOpStore = 62, SpvOpAccessChain = 65, SpvOpDecorate = 71,
    SpvOpMemberDecorate = 72, SpvOpVectorExtractDynamic = 77, SpvOpVectorInsertDynamic = 78,
    SpvOpVectorShuffle = 79, SpvOpCompositeConstruct = 80, SpvOpCompositeExtract = 81,
    SpvOpCompositeInsert = 82, SpvOpSampledImage = 86, SpvOpImageSampleImplicitLod = 87,
    SpvOpImageSampleExplicitLod = 88, SpvOpImageSampleDrefImplicitLod = 89,
    SpvOpImageSampleDrefExplicitLod = 90, SpvOpImageSampleProjImplicitLod = 91,
    SpvOpImageFetch = 95, SpvOpImageQuerySizeLod = 103,
    SpvOpConvertFToU = 109, SpvOpConvertFToS = 110, SpvOpConvertSToF = 111, SpvOpConvertUToF = 112,
    SpvOpBitcast = 124, SpvOpSNegate = 126, SpvOpFNegate = 127, SpvOpIAdd = 128, SpvOpFAdd = 129,
    SpvOpISub = 130, SpvOpFSub = 131, SpvOpIMul = 132, SpvOpFMul = 133, SpvOpUDiv = 134,
    SpvOpSDiv = 135, SpvOpFDiv = 136, SpvOpFMod = 141, SpvOpVectorTimesScalar = 142, SpvOpDot = 148,
    SpvOpAny = 154, SpvOpAll = 155, SpvOpIsNan = 156, SpvOpIsInf = 157,
    SpvOpLogicalEqual = 164, SpvOpLogicalNotEqual = 165, SpvOpLogicalOr = 166, SpvOpLogicalAnd = 167,
    SpvOpLogicalNot = 168, SpvOpSelect = 169, SpvOpIEqual = 170, SpvOpINotEqual = 171,
    SpvOpUGreaterThan = 172, SpvOpSGreaterThan = 173, SpvOpUGreaterThanEqual = 174,
    SpvOpSGreaterThanEqual = 175, SpvOpULessThan = 176, SpvOpSLessThan = 177,
    SpvOpULessThanEqual = 178, SpvOpSLessThanEqual = 179, SpvOpFOrdEqual = 180,
    SpvOpFUnordEqual = 181, SpvOpFOrdNotEqual = 182, SpvOpFUnordNotEqual = 183,
    SpvOpFOrdLessThan = 184, SpvOpFUnordLessThan = 185, SpvOpFOrdGreaterThan = 186,
    SpvOpFUnordGreaterThan = 187, SpvOpFOrdLessThanEqual = 188, SpvOpFUnordLessThanEqual = 189,
    SpvOpFOrdGreaterThanEqual = 190, SpvOpFUnordGreaterThanEqual = 191,
    SpvOpShiftRightLogical = 194, SpvOpShiftLeftLogical = 196, SpvOpBitwiseOr = 197,
    SpvOpBitwiseAnd = 199, SpvOpBitFieldUExtract = 203,
    SpvOpDPdx = 207, SpvOpDPdy = 208, SpvOpFwidth = 209,
    SpvOpPhi = 245, SpvOpLoopMerge = 246, SpvOpSelectionMerge = 247, SpvOpLabel = 248,
    SpvOpBranch = 249, SpvOpBranchConditional = 250, SpvOpSwitch = 251, SpvOpKill = 252,
    SpvOpReturn = 253, SpvOpReturnValue = 254, SpvOpUnreachable = 255,
    SpvOpDemoteToHelperInvocation = 5380,
};
enum {
    SpvStorageUniformConstant = 0, SpvStorageInput = 1, SpvStorageUniform = 2, SpvStorageOutput = 3,
    SpvStoragePrivate = 6, SpvStorageFunction = 7, SpvStoragePushConstant = 9,
};
enum {
    SpvDecorationSpecId = 1, SpvDecorationBlock = 2, SpvDecorationArrayStride = 6,
    SpvDecorationBuiltIn = 11, SpvDecorationNoPerspective = 13, SpvDecorationFlat = 14,
    SpvDecorationCentroid = 16, SpvDecorationInvariant = 18, SpvDecorationLocation = 30,
    SpvDecorationBinding = 33, SpvDecorationDescriptorSet = 34, SpvDecorationOffset = 35,
    SpvDecorationNoContraction = 42,
};
enum {
    SpvBuiltInPosition = 0, SpvBuiltInPointSize = 1, SpvBuiltInClipDistance = 3,
    SpvBuiltInVertexIndex = 42, SpvBuiltInFragCoord = 15, SpvBuiltInPointCoord = 16,
    SpvBuiltInFrontFacing = 17, SpvBuiltInFragDepth = 22,
};
enum {
    GLSLstd450Round = 1, GLSLstd450RoundEven = 2, GLSLstd450Trunc = 3, GLSLstd450FAbs = 4,
    GLSLstd450SAbs = 5, GLSLstd450FSign = 6, GLSLstd450Floor = 8, GLSLstd450Ceil = 9,
    GLSLstd450Fract = 10, GLSLstd450Sin = 13, GLSLstd450Cos = 14, GLSLstd450Pow = 26,
    GLSLstd450Exp2 = 29, GLSLstd450Log2 = 30, GLSLstd450Sqrt = 31, GLSLstd450InverseSqrt = 32,
    GLSLstd450FMin = 37, GLSLstd450UMin = 38, GLSLstd450SMin = 39, GLSLstd450FMax = 40,
    GLSLstd450UMax = 41, GLSLstd450SMax = 42, GLSLstd450FClamp = 43, GLSLstd450SClamp = 45,
    GLSLstd450FMix = 46, GLSLstd450Fma = 50, GLSLstd450Length = 66, GLSLstd450Cross = 68,
    GLSLstd450Normalize = 69, GLSLstd450NMin = 79, GLSLstd450NMax = 80, GLSLstd450NClamp = 81,
};

void spv_init(struct spv *s);
void spv_free(struct spv *s);
/* Finished module (words, count); the caller frees it with vkp_free. */
uint32_t *spv_finish(struct spv *s, uint32_t *count);

uint32_t spv_id(struct spv *s);
void spv_put(struct spv_buf *b, uint32_t w);
void spv_inst(struct spv_buf *b, uint32_t op, const uint32_t *w, uint32_t n);
void spv_string(struct spv_buf *b, const char *str);

void spv_capability(struct spv *s, uint32_t cap);
void spv_extension(struct spv *s, const char *name);
void spv_entry(struct spv *s, uint32_t model, uint32_t fn, const char *name, const uint32_t *ifaces, uint32_t n);
void spv_mode(struct spv *s, uint32_t fn, uint32_t mode);
void spv_decorate(struct spv *s, uint32_t id, uint32_t dec, int nargs, ...);
void spv_member_decorate(struct spv *s, uint32_t id, uint32_t member, uint32_t dec, int nargs, ...);
void spv_name(struct spv *s, uint32_t id, const char *name);

/* deduplicated types and constants */
uint32_t spv_cached(struct spv *s, uint32_t op, const uint32_t *operands, uint32_t n);
uint32_t spv_t_void(struct spv *s);
uint32_t spv_t_bool(struct spv *s);
uint32_t spv_t_int(struct spv *s, int is_signed);
uint32_t spv_t_float(struct spv *s);
uint32_t spv_t_vec(struct spv *s, uint32_t comp, uint32_t n);
uint32_t spv_t_array(struct spv *s, uint32_t elem, uint32_t len);
uint32_t spv_t_ptr(struct spv *s, uint32_t storage, uint32_t type);
uint32_t spv_t_fn(struct spv *s, uint32_t ret, const uint32_t *args, uint32_t n);
uint32_t spv_t_image(struct spv *s, uint32_t dim, uint32_t depth, uint32_t arrayed);
uint32_t spv_t_sampled(struct spv *s, uint32_t image);
/* A struct is never deduplicated (decorations differ per use). */
uint32_t spv_t_struct(struct spv *s, const uint32_t *members, uint32_t n);
uint32_t spv_c_f32(struct spv *s, float v);
uint32_t spv_c_u32(struct spv *s, uint32_t v);
uint32_t spv_c_i32(struct spv *s, int32_t v);
uint32_t spv_c_bool(struct spv *s, int v);
uint32_t spv_c_comp(struct spv *s, uint32_t type, const uint32_t *ids, uint32_t n);
uint32_t spv_c_vec4f(struct spv *s, float x, float y, float z, float w);
uint32_t spv_c_null(struct spv *s, uint32_t type);

/* global variable (types section) */
uint32_t spv_var(struct spv *s, uint32_t ptr_type, uint32_t storage);
/* function-local variable (hoisted to the first block) */
uint32_t spv_fvar(struct spv *s, uint32_t ptr_type);

void spv_func_begin(struct spv *s, uint32_t id, uint32_t ret, uint32_t fn_type);
void spv_func_end(struct spv *s);
uint32_t spv_label(struct spv *s, uint32_t id);

/* body instructions */
uint32_t spv_op(struct spv *s, uint32_t op, uint32_t type, const uint32_t *args, uint32_t n);
void spv_opv(struct spv *s, uint32_t op, const uint32_t *args, uint32_t n);
#define SPV_ARGS(...) ((const uint32_t[]){__VA_ARGS__}), (sizeof((const uint32_t[]){__VA_ARGS__}) / 4)
#define S_OP(s, op, type, ...) spv_op((s), (op), (type), SPV_ARGS(__VA_ARGS__))
#define S_OPV(s, op, ...) spv_opv((s), (op), SPV_ARGS(__VA_ARGS__))
uint32_t spv_ext(struct spv *s, uint32_t type, uint32_t inst, const uint32_t *args, uint32_t n);
#define S_EXT(s, type, inst, ...) spv_ext((s), (type), (inst), SPV_ARGS(__VA_ARGS__))
uint32_t spv_load(struct spv *s, uint32_t type, uint32_t ptr);
void spv_store(struct spv *s, uint32_t ptr, uint32_t value);
uint32_t spv_extract(struct spv *s, uint32_t type, uint32_t comp, uint32_t index);
uint32_t spv_shuffle(struct spv *s, uint32_t type, uint32_t a, uint32_t b, const uint32_t *sel, uint32_t n);
uint32_t spv_construct(struct spv *s, uint32_t type, const uint32_t *ids, uint32_t n);
void spv_branch(struct spv *s, uint32_t label);

#endif
