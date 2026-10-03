/* Complete ECX object-field getters. Field meanings and owning classes remain
 * unproven. Packed volatile loads retain unaligned word reads and native flags.
 * This file is editable and is not regenerated during builds. */
#include <stdint.h>
#include "target.h"
struct __attribute__((packed)) BfvWord { uint32_t value; };
#define TC __attribute__((thiscall))

#if BFV_BUILD_CLIENT
/* Native 00422d80; object word at byte offset 1384. */
uint32_t TC bfv_client_read_word_00422d80(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 1384u);
    return field->value;
}

/* Native 00422d90; object word at byte offset 2484. */
uint32_t TC bfv_client_read_word_00422d90(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 2484u);
    return field->value;
}

/* Native 004ca770; object word at byte offset 7516. */
uint32_t TC bfv_client_read_word_004ca770(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 7516u);
    return field->value;
}

/* Native 004ca780; object word at byte offset 7520. */
uint32_t TC bfv_client_read_word_004ca780(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 7520u);
    return field->value;
}

/* Native 004ca790; object word at byte offset 7524. */
uint32_t TC bfv_client_read_word_004ca790(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 7524u);
    return field->value;
}

/* Native 004ca830; object word at byte offset 7592. */
uint32_t TC bfv_client_read_word_004ca830(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 7592u);
    return field->value;
}

/* Native 004fee40; object word at byte offset 276. */
uint32_t TC bfv_client_read_word_004fee40(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 276u);
    return field->value;
}

/* Native 004fef60; object word at byte offset 416. */
uint32_t TC bfv_client_read_word_004fef60(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 416u);
    return field->value;
}

/* Native 005171c0; object word at byte offset 1144. */
uint32_t TC bfv_client_read_word_005171c0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 1144u);
    return field->value;
}

/* Native 005171d0; object word at byte offset 1288. */
uint32_t TC bfv_client_read_word_005171d0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 1288u);
    return field->value;
}

/* Native 005206a0; object word at byte offset 312. */
uint32_t TC bfv_client_read_word_005206a0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 312u);
    return field->value;
}

/* Native 00520910; object word at byte offset 688. */
uint32_t TC bfv_client_read_word_00520910(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 688u);
    return field->value;
}

/* Native 00520930; object word at byte offset 696. */
uint32_t TC bfv_client_read_word_00520930(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 696u);
    return field->value;
}

/* Native 00520950; object word at byte offset 700. */
uint32_t TC bfv_client_read_word_00520950(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 700u);
    return field->value;
}

/* Native 00520a50; object word at byte offset 1140. */
uint32_t TC bfv_client_read_word_00520a50(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 1140u);
    return field->value;
}

/* Native 00520a80; object word at byte offset 440. */
uint32_t TC bfv_client_read_word_00520a80(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 440u);
    return field->value;
}

/* Native 00520b60; object word at byte offset 244. */
uint32_t TC bfv_client_read_word_00520b60(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 244u);
    return field->value;
}

/* Native 005501f0; object word at byte offset 296. */
uint32_t TC bfv_client_read_word_005501f0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 296u);
    return field->value;
}

/* Native 00598eb0; object word at byte offset 252. */
uint32_t TC bfv_client_read_word_00598eb0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 252u);
    return field->value;
}

/* Native 0059b4f0; object word at byte offset 204. */
uint32_t TC bfv_client_read_word_0059b4f0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 204u);
    return field->value;
}

/* Native 005c3050; object word at byte offset 1128. */
uint32_t TC bfv_client_read_word_005c3050(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 1128u);
    return field->value;
}

/* Native 005f4be0; object word at byte offset 180. */
uint32_t TC bfv_client_read_word_005f4be0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 180u);
    return field->value;
}

/* Native 006656e0; object word at byte offset 660. */
uint32_t TC bfv_client_read_word_006656e0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 660u);
    return field->value;
}

/* Native 006656f0; object word at byte offset 724. */
uint32_t TC bfv_client_read_word_006656f0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 724u);
    return field->value;
}

/* Native 00665740; object word at byte offset 1136. */
uint32_t TC bfv_client_read_word_00665740(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 1136u);
    return field->value;
}

/* Native 00665750; object word at byte offset 1032. */
uint32_t TC bfv_client_read_word_00665750(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 1032u);
    return field->value;
}

/* Native 00665790; object word at byte offset 1104. */
uint32_t TC bfv_client_read_word_00665790(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 1104u);
    return field->value;
}

/* Native 00667190; object word at byte offset 428. */
uint32_t TC bfv_client_read_word_00667190(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 428u);
    return field->value;
}

/* Native 006671b0; object word at byte offset 904. */
uint32_t TC bfv_client_read_word_006671b0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 904u);
    return field->value;
}

/* Native 0067bb80; object word at byte offset 524. */
uint32_t TC bfv_client_read_word_0067bb80(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 524u);
    return field->value;
}

/* Native 0067bb90; object word at byte offset 528. */
uint32_t TC bfv_client_read_word_0067bb90(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 528u);
    return field->value;
}

/* Native 00684320; object word at byte offset 256. */
uint32_t TC bfv_client_read_word_00684320(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 256u);
    return field->value;
}

/* Native 0068ce50; object word at byte offset 372. */
uint32_t TC bfv_client_read_word_0068ce50(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 372u);
    return field->value;
}

/* Native 0068eda0; object word at byte offset 380. */
uint32_t TC bfv_client_read_word_0068eda0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 380u);
    return field->value;
}

/* Native 006964f0; object word at byte offset 500. */
uint32_t TC bfv_client_read_word_006964f0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 500u);
    return field->value;
}

/* Native 00699cf0; object word at byte offset 160. */
uint32_t TC bfv_client_read_word_00699cf0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 160u);
    return field->value;
}

/* Native 00699d20; object word at byte offset 164. */
uint32_t TC bfv_client_read_word_00699d20(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 164u);
    return field->value;
}

/* Native 00699d30; object word at byte offset 172. */
uint32_t TC bfv_client_read_word_00699d30(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 172u);
    return field->value;
}

/* Native 00699d90; object word at byte offset 176. */
uint32_t TC bfv_client_read_word_00699d90(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 176u);
    return field->value;
}

/* Native 00699db0; object word at byte offset 208. */
uint32_t TC bfv_client_read_word_00699db0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 208u);
    return field->value;
}

/* Native 00699dd0; object word at byte offset 240. */
uint32_t TC bfv_client_read_word_00699dd0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 240u);
    return field->value;
}

/* Native 00699e40; object word at byte offset 248. */
uint32_t TC bfv_client_read_word_00699e40(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 248u);
    return field->value;
}

/* Native 00699e60; object word at byte offset 316. */
uint32_t TC bfv_client_read_word_00699e60(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 316u);
    return field->value;
}

/* Native 00699e70; object word at byte offset 320. */
uint32_t TC bfv_client_read_word_00699e70(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 320u);
    return field->value;
}

/* Native 00699e90; object word at byte offset 356. */
uint32_t TC bfv_client_read_word_00699e90(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 356u);
    return field->value;
}

/* Native 0069a0b0; object word at byte offset 448. */
uint32_t TC bfv_client_read_word_0069a0b0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 448u);
    return field->value;
}

/* Native 0069a100; object word at byte offset 472. */
uint32_t TC bfv_client_read_word_0069a100(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 472u);
    return field->value;
}

/* Native 006a6ad0; object word at byte offset 280. */
uint32_t TC bfv_client_read_word_006a6ad0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 280u);
    return field->value;
}

/* Native 006a7f20; object word at byte offset 360. */
uint32_t TC bfv_client_read_word_006a7f20(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 360u);
    return field->value;
}

/* Native 006a81b0; object word at byte offset 544. */
uint32_t TC bfv_client_read_word_006a81b0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 544u);
    return field->value;
}

/* Native 006a81c0; object word at byte offset 548. */
uint32_t TC bfv_client_read_word_006a81c0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 548u);
    return field->value;
}

/* Native 006a81d0; object word at byte offset 552. */
uint32_t TC bfv_client_read_word_006a81d0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 552u);
    return field->value;
}

/* Native 006a9260; object word at byte offset 284. */
uint32_t TC bfv_client_read_word_006a9260(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 284u);
    return field->value;
}

/* Native 006b6a80; object word at byte offset 1700. */
uint32_t TC bfv_client_read_word_006b6a80(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 1700u);
    return field->value;
}

/* Native 006bd060; object word at byte offset 884. */
uint32_t TC bfv_client_read_word_006bd060(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 884u);
    return field->value;
}

/* Native 006bd0a0; object word at byte offset 1000. */
uint32_t TC bfv_client_read_word_006bd0a0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 1000u);
    return field->value;
}

/* Native 006bd430; object word at byte offset 888. */
uint32_t TC bfv_client_read_word_006bd430(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 888u);
    return field->value;
}

/* Native 006d7180; object word at byte offset 476. */
uint32_t TC bfv_client_read_word_006d7180(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 476u);
    return field->value;
}

/* Native 006dc0a0; object word at byte offset 492. */
uint32_t TC bfv_client_read_word_006dc0a0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 492u);
    return field->value;
}

/* Native 006ed430; object word at byte offset 468. */
uint32_t TC bfv_client_read_word_006ed430(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 468u);
    return field->value;
}

/* Native 006ed8c0; object word at byte offset 444. */
uint32_t TC bfv_client_read_word_006ed8c0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 444u);
    return field->value;
}

/* Native 006f69b0; object word at byte offset 376. */
uint32_t TC bfv_client_read_word_006f69b0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 376u);
    return field->value;
}

/* Native 0071e2e0; object word at byte offset 388. */
uint32_t TC bfv_client_read_word_0071e2e0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 388u);
    return field->value;
}

/* Native 0071e2f0; object word at byte offset 392. */
uint32_t TC bfv_client_read_word_0071e2f0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 392u);
    return field->value;
}

/* Native 0071e300; object word at byte offset 384. */
uint32_t TC bfv_client_read_word_0071e300(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 384u);
    return field->value;
}

/* Native 007215b0; object word at byte offset 260. */
uint32_t TC bfv_client_read_word_007215b0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 260u);
    return field->value;
}

/* Native 00729a00; object word at byte offset 1436. */
uint32_t TC bfv_client_read_word_00729a00(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 1436u);
    return field->value;
}

/* Native 0074d2f0; object word at byte offset 268. */
uint32_t TC bfv_client_read_word_0074d2f0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 268u);
    return field->value;
}

/* Native 0074dde0; object word at byte offset 540. */
uint32_t TC bfv_client_read_word_0074dde0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 540u);
    return field->value;
}

/* Native 00753230; object word at byte offset 184. */
uint32_t TC bfv_client_read_word_00753230(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 184u);
    return field->value;
}

/* Native 00770030; object word at byte offset 292. */
uint32_t TC bfv_client_read_word_00770030(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 292u);
    return field->value;
}

/* Native 00770750; object word at byte offset 332. */
uint32_t TC bfv_client_read_word_00770750(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 332u);
    return field->value;
}

/* Native 007ae2e0; object word at byte offset 636. */
uint32_t TC bfv_client_read_word_007ae2e0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 636u);
    return field->value;
}

/* Native 008a25a0; object word at byte offset 676. */
uint32_t TC bfv_client_read_word_008a25a0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 676u);
    return field->value;
}

/* Native 008a84c0; object word at byte offset 348. */
uint32_t TC bfv_client_read_word_008a84c0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 348u);
    return field->value;
}

/* Native 008a8520; object word at byte offset 484. */
uint32_t TC bfv_client_read_word_008a8520(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 484u);
    return field->value;
}

/* Native 00970720; object word at byte offset 128. */
uint32_t TC bfv_client_read_word_00970720(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 128u);
    return field->value;
}

/* Native 009707f0; object word at byte offset 168. */
uint32_t TC bfv_client_read_word_009707f0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 168u);
    return field->value;
}

/* Native 00970ab0; object word at byte offset 288. */
uint32_t TC bfv_client_read_word_00970ab0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 288u);
    return field->value;
}

/* Native 00970cc0; object word at byte offset 536. */
uint32_t TC bfv_client_read_word_00970cc0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 536u);
    return field->value;
}

/* Native 00970e60; object word at byte offset 592. */
uint32_t TC bfv_client_read_word_00970e60(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 592u);
    return field->value;
}

/* Native 0097bfd0; object word at byte offset 396. */
uint32_t TC bfv_client_read_word_0097bfd0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 396u);
    return field->value;
}

/* Native 009bef00; object word at byte offset 212. */
uint32_t TC bfv_client_read_word_009bef00(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 212u);
    return field->value;
}

/* Native 009bef10; object word at byte offset 216. */
uint32_t TC bfv_client_read_word_009bef10(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 216u);
    return field->value;
}

/* Native 009bef20; object word at byte offset 220. */
uint32_t TC bfv_client_read_word_009bef20(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 220u);
    return field->value;
}

/* Native 009bef30; object word at byte offset 140. */
uint32_t TC bfv_client_read_word_009bef30(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 140u);
    return field->value;
}

/* Native 009bef40; object word at byte offset 144. */
uint32_t TC bfv_client_read_word_009bef40(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 144u);
    return field->value;
}

/* Native 009bef50; object word at byte offset 148. */
uint32_t TC bfv_client_read_word_009bef50(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 148u);
    return field->value;
}

/* Native 009befc0; object word at byte offset 152. */
uint32_t TC bfv_client_read_word_009befc0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 152u);
    return field->value;
}

/* Native 009befd0; object word at byte offset 156. */
uint32_t TC bfv_client_read_word_009befd0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 156u);
    return field->value;
}

/* Native 009bf190; object word at byte offset 324. */
uint32_t TC bfv_client_read_word_009bf190(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 324u);
    return field->value;
}

/* Native 00a62060; object word at byte offset 264. */
uint32_t TC bfv_client_read_word_00a62060(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 264u);
    return field->value;
}

/* Native 00a6a000; object word at byte offset 3096. */
uint32_t TC bfv_client_read_word_00a6a000(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 3096u);
    return field->value;
}

/* Native 00a7edf0; object word at byte offset 132. */
uint32_t TC bfv_client_read_word_00a7edf0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 132u);
    return field->value;
}

/* Native 00a82460; object word at byte offset 224. */
uint32_t TC bfv_client_read_word_00a82460(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 224u);
    return field->value;
}

/* Native 00a82470; object word at byte offset 228. */
uint32_t TC bfv_client_read_word_00a82470(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 228u);
    return field->value;
}

#endif

#if BFV_BUILD_SERVER
/* Native 004224e0; object word at byte offset 2428. */
uint32_t TC bfv_server_read_word_004224e0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 2428u);
    return field->value;
}

/* Native 004225a0; object word at byte offset 1268. */
uint32_t TC bfv_server_read_word_004225a0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 1268u);
    return field->value;
}

/* Native 00422600; object word at byte offset 2392. */
uint32_t TC bfv_server_read_word_00422600(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 2392u);
    return field->value;
}

/* Native 0043e3f0; object word at byte offset 216. */
uint32_t TC bfv_server_read_word_0043e3f0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 216u);
    return field->value;
}

/* Native 0043e420; object word at byte offset 276. */
uint32_t TC bfv_server_read_word_0043e420(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 276u);
    return field->value;
}

/* Native 0043e550; object word at byte offset 416. */
uint32_t TC bfv_server_read_word_0043e550(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 416u);
    return field->value;
}

/* Native 0048b820; object word at byte offset 1144. */
uint32_t TC bfv_server_read_word_0048b820(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 1144u);
    return field->value;
}

/* Native 0048b830; object word at byte offset 1288. */
uint32_t TC bfv_server_read_word_0048b830(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 1288u);
    return field->value;
}

/* Native 00494cd0; object word at byte offset 312. */
uint32_t TC bfv_server_read_word_00494cd0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 312u);
    return field->value;
}

/* Native 00494ee0; object word at byte offset 676. */
uint32_t TC bfv_server_read_word_00494ee0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 676u);
    return field->value;
}

/* Native 00494f50; object word at byte offset 688. */
uint32_t TC bfv_server_read_word_00494f50(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 688u);
    return field->value;
}

/* Native 00494f70; object word at byte offset 696. */
uint32_t TC bfv_server_read_word_00494f70(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 696u);
    return field->value;
}

/* Native 00494f90; object word at byte offset 700. */
uint32_t TC bfv_server_read_word_00494f90(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 700u);
    return field->value;
}

/* Native 00495090; object word at byte offset 1140. */
uint32_t TC bfv_server_read_word_00495090(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 1140u);
    return field->value;
}

/* Native 004b05f0; object word at byte offset 212. */
uint32_t TC bfv_server_read_word_004b05f0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 212u);
    return field->value;
}

/* Native 004ba830; object word at byte offset 204. */
uint32_t TC bfv_server_read_word_004ba830(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 204u);
    return field->value;
}

/* Native 004d6c30; object word at byte offset 1128. */
uint32_t TC bfv_server_read_word_004d6c30(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 1128u);
    return field->value;
}

/* Native 004df0d0; object word at byte offset 152. */
uint32_t TC bfv_server_read_word_004df0d0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 152u);
    return field->value;
}

/* Native 004e18e0; object word at byte offset 408. */
uint32_t TC bfv_server_read_word_004e18e0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 408u);
    return field->value;
}

/* Native 00559a80; object word at byte offset 660. */
uint32_t TC bfv_server_read_word_00559a80(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 660u);
    return field->value;
}

/* Native 00559a90; object word at byte offset 724. */
uint32_t TC bfv_server_read_word_00559a90(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 724u);
    return field->value;
}

/* Native 00559b00; object word at byte offset 1024. */
uint32_t TC bfv_server_read_word_00559b00(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 1024u);
    return field->value;
}

/* Native 00559b30; object word at byte offset 1100. */
uint32_t TC bfv_server_read_word_00559b30(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 1100u);
    return field->value;
}

/* Native 0055b210; object word at byte offset 860. */
uint32_t TC bfv_server_read_word_0055b210(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 860u);
    return field->value;
}

/* Native 0056b350; object word at byte offset 444. */
uint32_t TC bfv_server_read_word_0056b350(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 444u);
    return field->value;
}

/* Native 0056b360; object word at byte offset 448. */
uint32_t TC bfv_server_read_word_0056b360(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 448u);
    return field->value;
}

/* Native 0056def0; object word at byte offset 164. */
uint32_t TC bfv_server_read_word_0056def0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 164u);
    return field->value;
}

/* Native 0056df00; object word at byte offset 172. */
uint32_t TC bfv_server_read_word_0056df00(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 172u);
    return field->value;
}

/* Native 0056df50; object word at byte offset 176. */
uint32_t TC bfv_server_read_word_0056df50(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 176u);
    return field->value;
}

/* Native 0056df70; object word at byte offset 208. */
uint32_t TC bfv_server_read_word_0056df70(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 208u);
    return field->value;
}

/* Native 0056dfe0; object word at byte offset 316. */
uint32_t TC bfv_server_read_word_0056dfe0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 316u);
    return field->value;
}

/* Native 0056dff0; object word at byte offset 320. */
uint32_t TC bfv_server_read_word_0056dff0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 320u);
    return field->value;
}

/* Native 0056e000; object word at byte offset 324. */
uint32_t TC bfv_server_read_word_0056e000(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 324u);
    return field->value;
}

/* Native 0056e010; object word at byte offset 356. */
uint32_t TC bfv_server_read_word_0056e010(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 356u);
    return field->value;
}

/* Native 0056e240; object word at byte offset 472. */
uint32_t TC bfv_server_read_word_0056e240(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 472u);
    return field->value;
}

/* Native 0056f320; object word at byte offset 372. */
uint32_t TC bfv_server_read_word_0056f320(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 372u);
    return field->value;
}

/* Native 00573b60; object word at byte offset 516. */
uint32_t TC bfv_server_read_word_00573b60(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 516u);
    return field->value;
}

/* Native 00573b70; object word at byte offset 520. */
uint32_t TC bfv_server_read_word_00573b70(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 520u);
    return field->value;
}

/* Native 0057dd90; object word at byte offset 220. */
uint32_t TC bfv_server_read_word_0057dd90(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 220u);
    return field->value;
}

/* Native 00588860; object word at byte offset 280. */
uint32_t TC bfv_server_read_word_00588860(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 280u);
    return field->value;
}

/* Native 00589820; object word at byte offset 352. */
uint32_t TC bfv_server_read_word_00589820(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 352u);
    return field->value;
}

/* Native 00589930; object word at byte offset 500. */
uint32_t TC bfv_server_read_word_00589930(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 500u);
    return field->value;
}

/* Native 00589940; object word at byte offset 504. */
uint32_t TC bfv_server_read_word_00589940(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 504u);
    return field->value;
}

/* Native 00589950; object word at byte offset 508. */
uint32_t TC bfv_server_read_word_00589950(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 508u);
    return field->value;
}

/* Native 00596ba0; object word at byte offset 428. */
uint32_t TC bfv_server_read_word_00596ba0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 428u);
    return field->value;
}

/* Native 005979a0; object word at byte offset 1700. */
uint32_t TC bfv_server_read_word_005979a0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 1700u);
    return field->value;
}

/* Native 0059de20; object word at byte offset 884. */
uint32_t TC bfv_server_read_word_0059de20(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 884u);
    return field->value;
}

/* Native 0059de60; object word at byte offset 956. */
uint32_t TC bfv_server_read_word_0059de60(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 956u);
    return field->value;
}

/* Native 0059e1d0; object word at byte offset 888. */
uint32_t TC bfv_server_read_word_0059e1d0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 888u);
    return field->value;
}

/* Native 0059e9a0; object word at byte offset 1436. */
uint32_t TC bfv_server_read_word_0059e9a0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 1436u);
    return field->value;
}

/* Native 005b0c50; object word at byte offset 364. */
uint32_t TC bfv_server_read_word_005b0c50(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 364u);
    return field->value;
}

/* Native 005b7820; object word at byte offset 432. */
uint32_t TC bfv_server_read_word_005b7820(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 432u);
    return field->value;
}

/* Native 005c9d70; object word at byte offset 156. */
uint32_t TC bfv_server_read_word_005c9d70(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 156u);
    return field->value;
}

/* Native 005c9da0; object word at byte offset 160. */
uint32_t TC bfv_server_read_word_005c9da0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 160u);
    return field->value;
}

/* Native 005c9de0; object word at byte offset 148. */
uint32_t TC bfv_server_read_word_005c9de0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 148u);
    return field->value;
}

/* Native 005cb900; object word at byte offset 468. */
uint32_t TC bfv_server_read_word_005cb900(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 468u);
    return field->value;
}

/* Native 005cb920; object word at byte offset 284. */
uint32_t TC bfv_server_read_word_005cb920(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 284u);
    return field->value;
}

/* Native 005cbdf0; object word at byte offset 440. */
uint32_t TC bfv_server_read_word_005cbdf0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 440u);
    return field->value;
}

/* Native 005d0b70; object word at byte offset 296. */
uint32_t TC bfv_server_read_word_005d0b70(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 296u);
    return field->value;
}

/* Native 005d0d50; object word at byte offset 288. */
uint32_t TC bfv_server_read_word_005d0d50(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 288u);
    return field->value;
}

/* Native 005d1220; object word at byte offset 292. */
uint32_t TC bfv_server_read_word_005d1220(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 292u);
    return field->value;
}

/* Native 005d4c10; object word at byte offset 368. */
uint32_t TC bfv_server_read_word_005d4c10(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 368u);
    return field->value;
}

/* Native 0060fbd0; object word at byte offset 168. */
uint32_t TC bfv_server_read_word_0060fbd0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 168u);
    return field->value;
}

/* Native 00610080; object word at byte offset 248. */
uint32_t TC bfv_server_read_word_00610080(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 248u);
    return field->value;
}

/* Native 00610090; object word at byte offset 252. */
uint32_t TC bfv_server_read_word_00610090(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 252u);
    return field->value;
}

/* Native 006100a0; object word at byte offset 256. */
uint32_t TC bfv_server_read_word_006100a0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 256u);
    return field->value;
}

/* Native 006774c0; object word at byte offset 484. */
uint32_t TC bfv_server_read_word_006774c0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 484u);
    return field->value;
}

/* Native 006888e0; object word at byte offset 144. */
uint32_t TC bfv_server_read_word_006888e0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 144u);
    return field->value;
}

/* Native 007188f0; object word at byte offset 184. */
uint32_t TC bfv_server_read_word_007188f0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 184u);
    return field->value;
}

/* Native 00718ac0; object word at byte offset 224. */
uint32_t TC bfv_server_read_word_00718ac0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 224u);
    return field->value;
}

/* Native 0071f890; object word at byte offset 348. */
uint32_t TC bfv_server_read_word_0071f890(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 348u);
    return field->value;
}

/* Native 00720a30; object word at byte offset 240. */
uint32_t TC bfv_server_read_word_00720a30(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 240u);
    return field->value;
}

/* Native 00720aa0; object word at byte offset 244. */
uint32_t TC bfv_server_read_word_00720aa0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 244u);
    return field->value;
}

/* Native 007680b0; object word at byte offset 180. */
uint32_t TC bfv_server_read_word_007680b0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 180u);
    return field->value;
}

/* Native 00768520; object word at byte offset 536. */
uint32_t TC bfv_server_read_word_00768520(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 536u);
    return field->value;
}

/* Native 007686b0; object word at byte offset 592. */
uint32_t TC bfv_server_read_word_007686b0(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 592u);
    return field->value;
}

/* Native 00773910; object word at byte offset 396. */
uint32_t TC bfv_server_read_word_00773910(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 396u);
    return field->value;
}

/* Native 00788590; object word at byte offset 128. */
uint32_t TC bfv_server_read_word_00788590(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 128u);
    return field->value;
}

/* Native 007a8f10; object word at byte offset 140. */
uint32_t TC bfv_server_read_word_007a8f10(const void *object)
{
    const volatile struct BfvWord *field = (const volatile struct BfvWord *)((const unsigned char *)object + 140u);
    return field->value;
}

#endif
