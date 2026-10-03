/* Individually editable constant-return functions. Generated once from audited
 * complete entries; the build does not regenerate this file. Static volatile
 * values force a MOV load instead of a zeroing XOR, preserving native flags.
 * uint32_t represents EAX bits; semantic pointer/integer types are not guessed. */
#include <stdint.h>
#include "target.h"
#define SC __attribute__((stdcall))

#if BFV_BUILD_CLIENT
/* Native 004236d0; EAX word 00000003; RET 16. */
uint32_t SC bfv_client_return_word_004236d0(uint32_t unused0 __attribute__((unused)), uint32_t unused1 __attribute__((unused)), uint32_t unused2 __attribute__((unused)), uint32_t unused3 __attribute__((unused)))
{
    static const volatile uint32_t result = 0x00000003u;
    return result;
}

/* Native 00516fd0; EAX word 00000024; RET 0. */
uint32_t SC bfv_client_return_word_00516fd0(void)
{
    static const volatile uint32_t result = 0x00000024u;
    return result;
}

/* Native 005170a0; EAX word 00000038; RET 0. */
uint32_t SC bfv_client_return_word_005170a0(void)
{
    static const volatile uint32_t result = 0x00000038u;
    return result;
}

/* Native 005172a0; EAX word 0000002a; RET 0. */
uint32_t SC bfv_client_return_word_005172a0(void)
{
    static const volatile uint32_t result = 0x0000002au;
    return result;
}

/* Native 005172d0; EAX word 0000003a; RET 0. */
uint32_t SC bfv_client_return_word_005172d0(void)
{
    static const volatile uint32_t result = 0x0000003au;
    return result;
}

/* Native 005230c0; EAX word 0000003e; RET 0. */
uint32_t SC bfv_client_return_word_005230c0(void)
{
    static const volatile uint32_t result = 0x0000003eu;
    return result;
}

/* Native 0052adb0; EAX word 00cb4724; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052adb0(void)
{
    static const volatile uint32_t result = 0x00cb4724u;
    return result;
}

/* Native 0052ae20; EAX word 00cad118; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052ae20(void)
{
    static const volatile uint32_t result = 0x00cad118u;
    return result;
}

/* Native 0052ae90; EAX word 00cb21d0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052ae90(void)
{
    static const volatile uint32_t result = 0x00cb21d0u;
    return result;
}

/* Native 0052afe0; EAX word 00cb4844; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052afe0(void)
{
    static const volatile uint32_t result = 0x00cb4844u;
    return result;
}

/* Native 0052b090; EAX word 00cb4890; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052b090(void)
{
    static const volatile uint32_t result = 0x00cb4890u;
    return result;
}

/* Native 0052b100; EAX word 00cb48f8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052b100(void)
{
    static const volatile uint32_t result = 0x00cb48f8u;
    return result;
}

/* Native 0052b170; EAX word 00cb495c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052b170(void)
{
    static const volatile uint32_t result = 0x00cb495cu;
    return result;
}

/* Native 0052b1e0; EAX word 00cb49c0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052b1e0(void)
{
    static const volatile uint32_t result = 0x00cb49c0u;
    return result;
}

/* Native 0052b250; EAX word 00cb4a0c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052b250(void)
{
    static const volatile uint32_t result = 0x00cb4a0cu;
    return result;
}

/* Native 0052b2c0; EAX word 00cb4a54; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052b2c0(void)
{
    static const volatile uint32_t result = 0x00cb4a54u;
    return result;
}

/* Native 0052b570; EAX word 00cb4d2c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052b570(void)
{
    static const volatile uint32_t result = 0x00cb4d2cu;
    return result;
}

/* Native 0052b5e0; EAX word 00cb4d98; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052b5e0(void)
{
    static const volatile uint32_t result = 0x00cb4d98u;
    return result;
}

/* Native 0052b650; EAX word 00cb4dfc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052b650(void)
{
    static const volatile uint32_t result = 0x00cb4dfcu;
    return result;
}

/* Native 0052b6c0; EAX word 00cb4e60; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052b6c0(void)
{
    static const volatile uint32_t result = 0x00cb4e60u;
    return result;
}

/* Native 0052b730; EAX word 00cb4ec8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052b730(void)
{
    static const volatile uint32_t result = 0x00cb4ec8u;
    return result;
}

/* Native 0052b7a0; EAX word 00cb4f50; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052b7a0(void)
{
    static const volatile uint32_t result = 0x00cb4f50u;
    return result;
}

/* Native 0052b810; EAX word 00cb4fd0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052b810(void)
{
    static const volatile uint32_t result = 0x00cb4fd0u;
    return result;
}

/* Native 0052b880; EAX word 00cb5034; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052b880(void)
{
    static const volatile uint32_t result = 0x00cb5034u;
    return result;
}

/* Native 0052b8f0; EAX word 00cb50a4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052b8f0(void)
{
    static const volatile uint32_t result = 0x00cb50a4u;
    return result;
}

/* Native 0052b960; EAX word 00cb5114; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052b960(void)
{
    static const volatile uint32_t result = 0x00cb5114u;
    return result;
}

/* Native 0052b9d0; EAX word 00cb517c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052b9d0(void)
{
    static const volatile uint32_t result = 0x00cb517cu;
    return result;
}

/* Native 0052ba40; EAX word 00cb5228; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052ba40(void)
{
    static const volatile uint32_t result = 0x00cb5228u;
    return result;
}

/* Native 0052bab0; EAX word 00cb52d4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052bab0(void)
{
    static const volatile uint32_t result = 0x00cb52d4u;
    return result;
}

/* Native 0052bb20; EAX word 00cb5340; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052bb20(void)
{
    static const volatile uint32_t result = 0x00cb5340u;
    return result;
}

/* Native 0052bb90; EAX word 00cb53f0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052bb90(void)
{
    static const volatile uint32_t result = 0x00cb53f0u;
    return result;
}

/* Native 0052bc00; EAX word 00cb54d8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052bc00(void)
{
    static const volatile uint32_t result = 0x00cb54d8u;
    return result;
}

/* Native 0052bc70; EAX word 00cb55c8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052bc70(void)
{
    static const volatile uint32_t result = 0x00cb55c8u;
    return result;
}

/* Native 0052bce0; EAX word 00cb5670; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052bce0(void)
{
    static const volatile uint32_t result = 0x00cb5670u;
    return result;
}

/* Native 0052bd50; EAX word 00cb56d8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052bd50(void)
{
    static const volatile uint32_t result = 0x00cb56d8u;
    return result;
}

/* Native 0052bdc0; EAX word 00cb573c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052bdc0(void)
{
    static const volatile uint32_t result = 0x00cb573cu;
    return result;
}

/* Native 0052be90; EAX word 00cb5804; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052be90(void)
{
    static const volatile uint32_t result = 0x00cb5804u;
    return result;
}

/* Native 0052bf00; EAX word 00cb5860; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052bf00(void)
{
    static const volatile uint32_t result = 0x00cb5860u;
    return result;
}

/* Native 0052bf80; EAX word 00b42840; RET 0. Original data identifier: void. */
uint32_t SC bfv_client_return_word_0052bf80(void)
{
    static const volatile uint32_t result = 0x00b42840u;
    return result;
}

/* Native 0052bf90; EAX word 00cb3a2c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052bf90(void)
{
    static const volatile uint32_t result = 0x00cb3a2cu;
    return result;
}

/* Native 0052bfa0; EAX word 00cb58a0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052bfa0(void)
{
    static const volatile uint32_t result = 0x00cb58a0u;
    return result;
}

/* Native 0052c9f0; EAX word 00cb5ca0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052c9f0(void)
{
    static const volatile uint32_t result = 0x00cb5ca0u;
    return result;
}

/* Native 0052ca00; EAX word 00cb4ab0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052ca00(void)
{
    static const volatile uint32_t result = 0x00cb4ab0u;
    return result;
}

/* Native 0052ca10; EAX word 00cb4b14; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052ca10(void)
{
    static const volatile uint32_t result = 0x00cb4b14u;
    return result;
}

/* Native 0052ca20; EAX word 00cb58f8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052ca20(void)
{
    static const volatile uint32_t result = 0x00cb58f8u;
    return result;
}

/* Native 0052ca30; EAX word 00cb4b7c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052ca30(void)
{
    static const volatile uint32_t result = 0x00cb4b7cu;
    return result;
}

/* Native 0052ca40; EAX word 00cb5920; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052ca40(void)
{
    static const volatile uint32_t result = 0x00cb5920u;
    return result;
}

/* Native 0052ca50; EAX word 00cb62b8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052ca50(void)
{
    static const volatile uint32_t result = 0x00cb62b8u;
    return result;
}

/* Native 0052ca60; EAX word 00cb4bf0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052ca60(void)
{
    static const volatile uint32_t result = 0x00cb4bf0u;
    return result;
}

/* Native 0052ca70; EAX word 00cb594c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052ca70(void)
{
    static const volatile uint32_t result = 0x00cb594cu;
    return result;
}

/* Native 0052ca80; EAX word 00cb4c64; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052ca80(void)
{
    static const volatile uint32_t result = 0x00cb4c64u;
    return result;
}

/* Native 0052ca90; EAX word 00cb645c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052ca90(void)
{
    static const volatile uint32_t result = 0x00cb645cu;
    return result;
}

/* Native 0052caa0; EAX word 00cb4cc8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052caa0(void)
{
    static const volatile uint32_t result = 0x00cb4cc8u;
    return result;
}

/* Native 0052cab0; EAX word 00ca7434; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cab0(void)
{
    static const volatile uint32_t result = 0x00ca7434u;
    return result;
}

/* Native 0052cac0; EAX word 00cb8c28; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cac0(void)
{
    static const volatile uint32_t result = 0x00cb8c28u;
    return result;
}

/* Native 0052cad0; EAX word 00cb597c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cad0(void)
{
    static const volatile uint32_t result = 0x00cb597cu;
    return result;
}

/* Native 0052cae0; EAX word 00cad164; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cae0(void)
{
    static const volatile uint32_t result = 0x00cad164u;
    return result;
}

/* Native 0052caf0; EAX word 00cb59a8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052caf0(void)
{
    static const volatile uint32_t result = 0x00cb59a8u;
    return result;
}

/* Native 0052cb00; EAX word 00cb59d0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cb00(void)
{
    static const volatile uint32_t result = 0x00cb59d0u;
    return result;
}

/* Native 0052cb10; EAX word 00cb57a0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cb10(void)
{
    static const volatile uint32_t result = 0x00cb57a0u;
    return result;
}

/* Native 0052cb20; EAX word 00cb687c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cb20(void)
{
    static const volatile uint32_t result = 0x00cb687cu;
    return result;
}

/* Native 0052cb30; EAX word 00cad188; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cb30(void)
{
    static const volatile uint32_t result = 0x00cad188u;
    return result;
}

/* Native 0052cb40; EAX word 00cb59fc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cb40(void)
{
    static const volatile uint32_t result = 0x00cb59fcu;
    return result;
}

/* Native 0052cb50; EAX word 00cb5a24; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cb50(void)
{
    static const volatile uint32_t result = 0x00cb5a24u;
    return result;
}

/* Native 0052cb60; EAX word 00cb8f10; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cb60(void)
{
    static const volatile uint32_t result = 0x00cb8f10u;
    return result;
}

/* Native 0052cb70; EAX word 00cb759c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cb70(void)
{
    static const volatile uint32_t result = 0x00cb759cu;
    return result;
}

/* Native 0052cb80; EAX word 00cb8f88; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cb80(void)
{
    static const volatile uint32_t result = 0x00cb8f88u;
    return result;
}

/* Native 0052cb90; EAX word 00cb767c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cb90(void)
{
    static const volatile uint32_t result = 0x00cb767cu;
    return result;
}

/* Native 0052cba0; EAX word 00cb9008; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cba0(void)
{
    static const volatile uint32_t result = 0x00cb9008u;
    return result;
}

/* Native 0052cbb0; EAX word 00cb7764; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cbb0(void)
{
    static const volatile uint32_t result = 0x00cb7764u;
    return result;
}

/* Native 0052cbc0; EAX word 00cb907c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cbc0(void)
{
    static const volatile uint32_t result = 0x00cb907cu;
    return result;
}

/* Native 0052cbd0; EAX word 00cb7840; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cbd0(void)
{
    static const volatile uint32_t result = 0x00cb7840u;
    return result;
}

/* Native 0052cbe0; EAX word 00cb90ec; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cbe0(void)
{
    static const volatile uint32_t result = 0x00cb90ecu;
    return result;
}

/* Native 0052cbf0; EAX word 00cb7918; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cbf0(void)
{
    static const volatile uint32_t result = 0x00cb7918u;
    return result;
}

/* Native 0052cc00; EAX word 00cb9168; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cc00(void)
{
    static const volatile uint32_t result = 0x00cb9168u;
    return result;
}

/* Native 0052cc10; EAX word 00cb7a04; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cc10(void)
{
    static const volatile uint32_t result = 0x00cb7a04u;
    return result;
}

/* Native 0052cc20; EAX word 00cb91e8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cc20(void)
{
    static const volatile uint32_t result = 0x00cb91e8u;
    return result;
}

/* Native 0052cc30; EAX word 00cb7aec; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cc30(void)
{
    static const volatile uint32_t result = 0x00cb7aecu;
    return result;
}

/* Native 0052cc40; EAX word 00cb9264; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cc40(void)
{
    static const volatile uint32_t result = 0x00cb9264u;
    return result;
}

/* Native 0052cc50; EAX word 00cb7bc8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cc50(void)
{
    static const volatile uint32_t result = 0x00cb7bc8u;
    return result;
}

/* Native 0052cc60; EAX word 00cb92dc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cc60(void)
{
    static const volatile uint32_t result = 0x00cb92dcu;
    return result;
}

/* Native 0052cc70; EAX word 00cb7ca8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cc70(void)
{
    static const volatile uint32_t result = 0x00cb7ca8u;
    return result;
}

/* Native 0052cc80; EAX word 00cb9354; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cc80(void)
{
    static const volatile uint32_t result = 0x00cb9354u;
    return result;
}

/* Native 0052cc90; EAX word 00cb7d80; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cc90(void)
{
    static const volatile uint32_t result = 0x00cb7d80u;
    return result;
}

/* Native 0052cca0; EAX word 00cb93c8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cca0(void)
{
    static const volatile uint32_t result = 0x00cb93c8u;
    return result;
}

/* Native 0052ccb0; EAX word 00cb7e54; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052ccb0(void)
{
    static const volatile uint32_t result = 0x00cb7e54u;
    return result;
}

/* Native 0052ccc0; EAX word 00cb9440; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052ccc0(void)
{
    static const volatile uint32_t result = 0x00cb9440u;
    return result;
}

/* Native 0052ccd0; EAX word 00cb7f34; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052ccd0(void)
{
    static const volatile uint32_t result = 0x00cb7f34u;
    return result;
}

/* Native 0052cce0; EAX word 00cb94c0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cce0(void)
{
    static const volatile uint32_t result = 0x00cb94c0u;
    return result;
}

/* Native 0052ccf0; EAX word 00cb801c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052ccf0(void)
{
    static const volatile uint32_t result = 0x00cb801cu;
    return result;
}

/* Native 0052cd00; EAX word 00cb9538; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cd00(void)
{
    static const volatile uint32_t result = 0x00cb9538u;
    return result;
}

/* Native 0052cd10; EAX word 00cb80fc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cd10(void)
{
    static const volatile uint32_t result = 0x00cb80fcu;
    return result;
}

/* Native 0052cd20; EAX word 00cb95b0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cd20(void)
{
    static const volatile uint32_t result = 0x00cb95b0u;
    return result;
}

/* Native 0052cd30; EAX word 00cb81dc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cd30(void)
{
    static const volatile uint32_t result = 0x00cb81dcu;
    return result;
}

/* Native 0052cd40; EAX word 00cb9638; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cd40(void)
{
    static const volatile uint32_t result = 0x00cb9638u;
    return result;
}

/* Native 0052cd50; EAX word 00cb82d4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cd50(void)
{
    static const volatile uint32_t result = 0x00cb82d4u;
    return result;
}

/* Native 0052cd60; EAX word 00cb96c8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cd60(void)
{
    static const volatile uint32_t result = 0x00cb96c8u;
    return result;
}

/* Native 0052cd70; EAX word 00cb83d4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cd70(void)
{
    static const volatile uint32_t result = 0x00cb83d4u;
    return result;
}

/* Native 0052cd80; EAX word 00cb974c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cd80(void)
{
    static const volatile uint32_t result = 0x00cb974cu;
    return result;
}

/* Native 0052cd90; EAX word 00cb84b8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cd90(void)
{
    static const volatile uint32_t result = 0x00cb84b8u;
    return result;
}

/* Native 0052cda0; EAX word 00cb97c4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cda0(void)
{
    static const volatile uint32_t result = 0x00cb97c4u;
    return result;
}

/* Native 0052cdb0; EAX word 00cb8598; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cdb0(void)
{
    static const volatile uint32_t result = 0x00cb8598u;
    return result;
}

/* Native 0052cdc0; EAX word 00cb9840; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cdc0(void)
{
    static const volatile uint32_t result = 0x00cb9840u;
    return result;
}

/* Native 0052cdd0; EAX word 00cb8684; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cdd0(void)
{
    static const volatile uint32_t result = 0x00cb8684u;
    return result;
}

/* Native 0052cde0; EAX word 00cb98c0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cde0(void)
{
    static const volatile uint32_t result = 0x00cb98c0u;
    return result;
}

/* Native 0052cdf0; EAX word 00cb876c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052cdf0(void)
{
    static const volatile uint32_t result = 0x00cb876cu;
    return result;
}

/* Native 0052ce00; EAX word 00cb9938; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052ce00(void)
{
    static const volatile uint32_t result = 0x00cb9938u;
    return result;
}

/* Native 0052ce10; EAX word 00cb8844; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052ce10(void)
{
    static const volatile uint32_t result = 0x00cb8844u;
    return result;
}

/* Native 0052d0a0; EAX word 00cb422c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052d0a0(void)
{
    static const volatile uint32_t result = 0x00cb422cu;
    return result;
}

/* Native 0052d0e0; EAX word 00cad0f0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052d0e0(void)
{
    static const volatile uint32_t result = 0x00cad0f0u;
    return result;
}

/* Native 0052d120; EAX word 00ca7414; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052d120(void)
{
    static const volatile uint32_t result = 0x00ca7414u;
    return result;
}

/* Native 0052d180; EAX word 00cb5d74; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052d180(void)
{
    static const volatile uint32_t result = 0x00cb5d74u;
    return result;
}

/* Native 0052d1c0; EAX word 00cb5e48; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052d1c0(void)
{
    static const volatile uint32_t result = 0x00cb5e48u;
    return result;
}

/* Native 0052d200; EAX word 00cb5f20; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052d200(void)
{
    static const volatile uint32_t result = 0x00cb5f20u;
    return result;
}

/* Native 0052d270; EAX word 00cb8944; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052d270(void)
{
    static const volatile uint32_t result = 0x00cb8944u;
    return result;
}

/* Native 0052d2b0; EAX word 00cad140; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052d2b0(void)
{
    static const volatile uint32_t result = 0x00cad140u;
    return result;
}

/* Native 0052d4d0; EAX word 00cb6788; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052d4d0(void)
{
    static const volatile uint32_t result = 0x00cb6788u;
    return result;
}

/* Native 0052d5a0; EAX word 00cb6b98; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052d5a0(void)
{
    static const volatile uint32_t result = 0x00cb6b98u;
    return result;
}

/* Native 0052d5e0; EAX word 00cb6c78; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052d5e0(void)
{
    static const volatile uint32_t result = 0x00cb6c78u;
    return result;
}

/* Native 0052d620; EAX word 00cb6dd8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052d620(void)
{
    static const volatile uint32_t result = 0x00cb6dd8u;
    return result;
}

/* Native 0052d660; EAX word 00cb6efc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052d660(void)
{
    static const volatile uint32_t result = 0x00cb6efcu;
    return result;
}

/* Native 0052d6a0; EAX word 00cb6fe4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052d6a0(void)
{
    static const volatile uint32_t result = 0x00cb6fe4u;
    return result;
}

/* Native 0052d6e0; EAX word 00cb7140; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052d6e0(void)
{
    static const volatile uint32_t result = 0x00cb7140u;
    return result;
}

/* Native 0052d720; EAX word 00cb72d8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052d720(void)
{
    static const volatile uint32_t result = 0x00cb72d8u;
    return result;
}

/* Native 0052d760; EAX word 00cb7480; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052d760(void)
{
    static const volatile uint32_t result = 0x00cb7480u;
    return result;
}

/* Native 0052d800; EAX word 00cb5bc4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052d800(void)
{
    static const volatile uint32_t result = 0x00cb5bc4u;
    return result;
}

/* Native 0052d870; EAX word 00cb6abc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052d870(void)
{
    static const volatile uint32_t result = 0x00cb6abcu;
    return result;
}

/* Native 0052d8b0; EAX word 00cb5af8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052d8b0(void)
{
    static const volatile uint32_t result = 0x00cb5af8u;
    return result;
}

/* Native 0052dc80; EAX word 00cb47e4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052dc80(void)
{
    static const volatile uint32_t result = 0x00cb47e4u;
    return result;
}

/* Native 0052dc90; EAX word 00cbb0d4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052dc90(void)
{
    static const volatile uint32_t result = 0x00cbb0d4u;
    return result;
}

/* Native 0052dd00; EAX word 00cb4814; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052dd00(void)
{
    static const volatile uint32_t result = 0x00cb4814u;
    return result;
}

/* Native 0052dd10; EAX word 00cbb12c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052dd10(void)
{
    static const volatile uint32_t result = 0x00cbb12cu;
    return result;
}

/* Native 0052dde0; EAX word 00cbb180; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052dde0(void)
{
    static const volatile uint32_t result = 0x00cbb180u;
    return result;
}

/* Native 0052de80; EAX word 00cbb214; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052de80(void)
{
    static const volatile uint32_t result = 0x00cbb214u;
    return result;
}

/* Native 0052f180; EAX word 00cbb048; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052f180(void)
{
    static const volatile uint32_t result = 0x00cbb048u;
    return result;
}

/* Native 0052f190; EAX word 00cbb420; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052f190(void)
{
    static const volatile uint32_t result = 0x00cbb420u;
    return result;
}

/* Native 0052f260; EAX word 00cbb590; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0052f260(void)
{
    static const volatile uint32_t result = 0x00cbb590u;
    return result;
}

/* Native 0053e4f0; EAX word 00cbf9bc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0053e4f0(void)
{
    static const volatile uint32_t result = 0x00cbf9bcu;
    return result;
}

/* Native 0053e560; EAX word 00cbfa08; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0053e560(void)
{
    static const volatile uint32_t result = 0x00cbfa08u;
    return result;
}

/* Native 0053e5d0; EAX word 00cbfa6c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0053e5d0(void)
{
    static const volatile uint32_t result = 0x00cbfa6cu;
    return result;
}

/* Native 0053e640; EAX word 00cbfacc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0053e640(void)
{
    static const volatile uint32_t result = 0x00cbfaccu;
    return result;
}

/* Native 0053e6b0; EAX word 00cbfb30; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0053e6b0(void)
{
    static const volatile uint32_t result = 0x00cbfb30u;
    return result;
}

/* Native 0053e720; EAX word 00cbfba8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0053e720(void)
{
    static const volatile uint32_t result = 0x00cbfba8u;
    return result;
}

/* Native 0053e790; EAX word 00cbfc14; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0053e790(void)
{
    static const volatile uint32_t result = 0x00cbfc14u;
    return result;
}

/* Native 0053e800; EAX word 00cbfc6c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0053e800(void)
{
    static const volatile uint32_t result = 0x00cbfc6cu;
    return result;
}

/* Native 0053e870; EAX word 00cbfcf4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0053e870(void)
{
    static const volatile uint32_t result = 0x00cbfcf4u;
    return result;
}

/* Native 0053e8b0; EAX word 00cbfd58; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0053e8b0(void)
{
    static const volatile uint32_t result = 0x00cbfd58u;
    return result;
}

/* Native 0053e8f0; EAX word 00cbfdb8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0053e8f0(void)
{
    static const volatile uint32_t result = 0x00cbfdb8u;
    return result;
}

/* Native 0053e930; EAX word 00cbfe18; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0053e930(void)
{
    static const volatile uint32_t result = 0x00cbfe18u;
    return result;
}

/* Native 0053e970; EAX word 00cbfe8c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0053e970(void)
{
    static const volatile uint32_t result = 0x00cbfe8cu;
    return result;
}

/* Native 0053e9b0; EAX word 00cbfef4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0053e9b0(void)
{
    static const volatile uint32_t result = 0x00cbfef4u;
    return result;
}

/* Native 0053e9f0; EAX word 00ca6800; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0053e9f0(void)
{
    static const volatile uint32_t result = 0x00ca6800u;
    return result;
}

/* Native 0053eaa0; EAX word 00cbffcc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0053eaa0(void)
{
    static const volatile uint32_t result = 0x00cbffccu;
    return result;
}

/* Native 0053ed90; EAX word 00cc00e0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0053ed90(void)
{
    static const volatile uint32_t result = 0x00cc00e0u;
    return result;
}

/* Native 0053ee80; EAX word 00cc019c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0053ee80(void)
{
    static const volatile uint32_t result = 0x00cc019cu;
    return result;
}

/* Native 00554080; EAX word 00d731ac; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00554080(void)
{
    static const volatile uint32_t result = 0x00d731acu;
    return result;
}

/* Native 005567d0; EAX word 0000007c; RET 0. */
uint32_t SC bfv_client_return_word_005567d0(void)
{
    static const volatile uint32_t result = 0x0000007cu;
    return result;
}

/* Native 005576e0; EAX word 00000050; RET 0. */
uint32_t SC bfv_client_return_word_005576e0(void)
{
    static const volatile uint32_t result = 0x00000050u;
    return result;
}

/* Native 00559510; EAX word 0000005c; RET 0. */
uint32_t SC bfv_client_return_word_00559510(void)
{
    static const volatile uint32_t result = 0x0000005cu;
    return result;
}

/* Native 00559950; EAX word 00000058; RET 0. */
uint32_t SC bfv_client_return_word_00559950(void)
{
    static const volatile uint32_t result = 0x00000058u;
    return result;
}

/* Native 0055b750; EAX word 00000040; RET 0. */
uint32_t SC bfv_client_return_word_0055b750(void)
{
    static const volatile uint32_t result = 0x00000040u;
    return result;
}

/* Native 0055d750; EAX word 00000060; RET 0. */
uint32_t SC bfv_client_return_word_0055d750(void)
{
    static const volatile uint32_t result = 0x00000060u;
    return result;
}

/* Native 00560a90; EAX word 00000054; RET 0. */
uint32_t SC bfv_client_return_word_00560a90(void)
{
    static const volatile uint32_t result = 0x00000054u;
    return result;
}

/* Native 00560cf0; EAX word 0000006c; RET 0. */
uint32_t SC bfv_client_return_word_00560cf0(void)
{
    static const volatile uint32_t result = 0x0000006cu;
    return result;
}

/* Native 00560f30; EAX word 000000ac; RET 0. */
uint32_t SC bfv_client_return_word_00560f30(void)
{
    static const volatile uint32_t result = 0x000000acu;
    return result;
}

/* Native 005636f0; EAX word 00000068; RET 0. */
uint32_t SC bfv_client_return_word_005636f0(void)
{
    static const volatile uint32_t result = 0x00000068u;
    return result;
}

/* Native 005651b0; EAX word 0000002c; RET 0. */
uint32_t SC bfv_client_return_word_005651b0(void)
{
    static const volatile uint32_t result = 0x0000002cu;
    return result;
}

/* Native 005660f0; EAX word 00000044; RET 0. */
uint32_t SC bfv_client_return_word_005660f0(void)
{
    static const volatile uint32_t result = 0x00000044u;
    return result;
}

/* Native 00566570; EAX word 0000008c; RET 0. */
uint32_t SC bfv_client_return_word_00566570(void)
{
    static const volatile uint32_t result = 0x0000008cu;
    return result;
}

/* Native 00568230; EAX word 0000004c; RET 0. */
uint32_t SC bfv_client_return_word_00568230(void)
{
    static const volatile uint32_t result = 0x0000004cu;
    return result;
}

/* Native 005697f0; EAX word 000000f4; RET 0. */
uint32_t SC bfv_client_return_word_005697f0(void)
{
    static const volatile uint32_t result = 0x000000f4u;
    return result;
}

/* Native 0056fb40; EAX word 00000074; RET 0. */
uint32_t SC bfv_client_return_word_0056fb40(void)
{
    static const volatile uint32_t result = 0x00000074u;
    return result;
}

/* Native 005922a0; EAX word 00cc4db4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_005922a0(void)
{
    static const volatile uint32_t result = 0x00cc4db4u;
    return result;
}

/* Native 00592310; EAX word 00cc4e18; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00592310(void)
{
    static const volatile uint32_t result = 0x00cc4e18u;
    return result;
}

/* Native 00592380; EAX word 00cc4e84; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00592380(void)
{
    static const volatile uint32_t result = 0x00cc4e84u;
    return result;
}

/* Native 005923f0; EAX word 00cc4ef4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_005923f0(void)
{
    static const volatile uint32_t result = 0x00cc4ef4u;
    return result;
}

/* Native 00592460; EAX word 00cc4f64; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00592460(void)
{
    static const volatile uint32_t result = 0x00cc4f64u;
    return result;
}

/* Native 005924d0; EAX word 00cc4fcc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_005924d0(void)
{
    static const volatile uint32_t result = 0x00cc4fccu;
    return result;
}

/* Native 00592540; EAX word 00cc5038; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00592540(void)
{
    static const volatile uint32_t result = 0x00cc5038u;
    return result;
}

/* Native 005925b0; EAX word 00cc50b0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_005925b0(void)
{
    static const volatile uint32_t result = 0x00cc50b0u;
    return result;
}

/* Native 00592620; EAX word 00cc513c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00592620(void)
{
    static const volatile uint32_t result = 0x00cc513cu;
    return result;
}

/* Native 00592690; EAX word 00cc51bc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00592690(void)
{
    static const volatile uint32_t result = 0x00cc51bcu;
    return result;
}

/* Native 005927a0; EAX word 00cc5254; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_005927a0(void)
{
    static const volatile uint32_t result = 0x00cc5254u;
    return result;
}

/* Native 00592920; EAX word 00cc5644; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00592920(void)
{
    static const volatile uint32_t result = 0x00cc5644u;
    return result;
}

/* Native 00592960; EAX word 00cc56a8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00592960(void)
{
    static const volatile uint32_t result = 0x00cc56a8u;
    return result;
}

/* Native 005929a0; EAX word 00cc5710; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_005929a0(void)
{
    static const volatile uint32_t result = 0x00cc5710u;
    return result;
}

/* Native 005929e0; EAX word 00cb282c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_005929e0(void)
{
    static const volatile uint32_t result = 0x00cb282cu;
    return result;
}

/* Native 00592a20; EAX word 00cc57c0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00592a20(void)
{
    static const volatile uint32_t result = 0x00cc57c0u;
    return result;
}

/* Native 00592a60; EAX word 00cb2808; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00592a60(void)
{
    static const volatile uint32_t result = 0x00cb2808u;
    return result;
}

/* Native 00592aa0; EAX word 00cc586c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00592aa0(void)
{
    static const volatile uint32_t result = 0x00cc586cu;
    return result;
}

/* Native 00592ae0; EAX word 00cc58e4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00592ae0(void)
{
    static const volatile uint32_t result = 0x00cc58e4u;
    return result;
}

/* Native 00592b20; EAX word 00cc5968; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00592b20(void)
{
    static const volatile uint32_t result = 0x00cc5968u;
    return result;
}

/* Native 00592b60; EAX word 00cc55dc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00592b60(void)
{
    static const volatile uint32_t result = 0x00cc55dcu;
    return result;
}

/* Native 00592ba0; EAX word 00cc54f4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00592ba0(void)
{
    static const volatile uint32_t result = 0x00cc54f4u;
    return result;
}

/* Native 00592c80; EAX word 00cc4cd0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00592c80(void)
{
    static const volatile uint32_t result = 0x00cc4cd0u;
    return result;
}

/* Native 00592c90; EAX word 00cc5b78; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00592c90(void)
{
    static const volatile uint32_t result = 0x00cc5b78u;
    return result;
}

/* Native 00592d30; EAX word 00cc520c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00592d30(void)
{
    static const volatile uint32_t result = 0x00cc520cu;
    return result;
}

/* Native 00592d40; EAX word 00cc5c04; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00592d40(void)
{
    static const volatile uint32_t result = 0x00cc5c04u;
    return result;
}

/* Native 005ac3f0; EAX word 0000003c; RET 0. */
uint32_t SC bfv_client_return_word_005ac3f0(void)
{
    static const volatile uint32_t result = 0x0000003cu;
    return result;
}

/* Native 005ac4c0; EAX word 00000027; RET 0. */
uint32_t SC bfv_client_return_word_005ac4c0(void)
{
    static const volatile uint32_t result = 0x00000027u;
    return result;
}

/* Native 005ac950; EAX word 00000036; RET 0. */
uint32_t SC bfv_client_return_word_005ac950(void)
{
    static const volatile uint32_t result = 0x00000036u;
    return result;
}

/* Native 005acf50; EAX word 0000002b; RET 0. */
uint32_t SC bfv_client_return_word_005acf50(void)
{
    static const volatile uint32_t result = 0x0000002bu;
    return result;
}

/* Native 005ad5f0; EAX word 0000001f; RET 0. */
uint32_t SC bfv_client_return_word_005ad5f0(void)
{
    static const volatile uint32_t result = 0x0000001fu;
    return result;
}

/* Native 005adb10; EAX word 0000002f; RET 0. */
uint32_t SC bfv_client_return_word_005adb10(void)
{
    static const volatile uint32_t result = 0x0000002fu;
    return result;
}

/* Native 005adba0; EAX word 00000030; RET 0. */
uint32_t SC bfv_client_return_word_005adba0(void)
{
    static const volatile uint32_t result = 0x00000030u;
    return result;
}

/* Native 005ae080; EAX word 0000003b; RET 0. */
uint32_t SC bfv_client_return_word_005ae080(void)
{
    static const volatile uint32_t result = 0x0000003bu;
    return result;
}

/* Native 005ae190; EAX word 0000003d; RET 0. */
uint32_t SC bfv_client_return_word_005ae190(void)
{
    static const volatile uint32_t result = 0x0000003du;
    return result;
}

/* Native 005ae790; EAX word 00000026; RET 0. */
uint32_t SC bfv_client_return_word_005ae790(void)
{
    static const volatile uint32_t result = 0x00000026u;
    return result;
}

/* Native 005d2e80; EAX word 00ccadf8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_005d2e80(void)
{
    static const volatile uint32_t result = 0x00ccadf8u;
    return result;
}

/* Native 0069b8e0; EAX word 00cd9244; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0069b8e0(void)
{
    static const volatile uint32_t result = 0x00cd9244u;
    return result;
}

/* Native 0069b950; EAX word 00cd92b8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0069b950(void)
{
    static const volatile uint32_t result = 0x00cd92b8u;
    return result;
}

/* Native 0069b9c0; EAX word 00cd9324; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0069b9c0(void)
{
    static const volatile uint32_t result = 0x00cd9324u;
    return result;
}

/* Native 0069ba60; EAX word 00cd93a4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0069ba60(void)
{
    static const volatile uint32_t result = 0x00cd93a4u;
    return result;
}

/* Native 0069baa0; EAX word 00ca7454; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0069baa0(void)
{
    static const volatile uint32_t result = 0x00ca7454u;
    return result;
}

/* Native 0069bae0; EAX word 00cad3e0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0069bae0(void)
{
    static const volatile uint32_t result = 0x00cad3e0u;
    return result;
}

/* Native 006a9be0; EAX word 00cdb538; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006a9be0(void)
{
    static const volatile uint32_t result = 0x00cdb538u;
    return result;
}

/* Native 006a9c50; EAX word 00cdb5e0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006a9c50(void)
{
    static const volatile uint32_t result = 0x00cdb5e0u;
    return result;
}

/* Native 006a9cc0; EAX word 00cdb674; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006a9cc0(void)
{
    static const volatile uint32_t result = 0x00cdb674u;
    return result;
}

/* Native 006a9d30; EAX word 00cdb6fc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006a9d30(void)
{
    static const volatile uint32_t result = 0x00cdb6fcu;
    return result;
}

/* Native 006a9da0; EAX word 00cdb774; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006a9da0(void)
{
    static const volatile uint32_t result = 0x00cdb774u;
    return result;
}

/* Native 006aa050; EAX word 00cdbb08; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006aa050(void)
{
    static const volatile uint32_t result = 0x00cdbb08u;
    return result;
}

/* Native 006aa3d0; EAX word 00cdb7f4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006aa3d0(void)
{
    static const volatile uint32_t result = 0x00cdb7f4u;
    return result;
}

/* Native 006aa3e0; EAX word 00cdb880; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006aa3e0(void)
{
    static const volatile uint32_t result = 0x00cdb880u;
    return result;
}

/* Native 006aa3f0; EAX word 00cdb904; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006aa3f0(void)
{
    static const volatile uint32_t result = 0x00cdb904u;
    return result;
}

/* Native 006aa400; EAX word 00cdb988; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006aa400(void)
{
    static const volatile uint32_t result = 0x00cdb988u;
    return result;
}

/* Native 006aa410; EAX word 00cdbb90; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006aa410(void)
{
    static const volatile uint32_t result = 0x00cdbb90u;
    return result;
}

/* Native 006aa420; EAX word 00cdb078; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006aa420(void)
{
    static const volatile uint32_t result = 0x00cdb078u;
    return result;
}

/* Native 006aa430; EAX word 00cdba8c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006aa430(void)
{
    static const volatile uint32_t result = 0x00cdba8cu;
    return result;
}

/* Native 006aa440; EAX word 00cdb0b0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006aa440(void)
{
    static const volatile uint32_t result = 0x00cdb0b0u;
    return result;
}

/* Native 006aa450; EAX word 00cdc864; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006aa450(void)
{
    static const volatile uint32_t result = 0x00cdc864u;
    return result;
}

/* Native 006aa460; EAX word 00ccc940; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006aa460(void)
{
    static const volatile uint32_t result = 0x00ccc940u;
    return result;
}

/* Native 006aa470; EAX word 00cdbc10; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006aa470(void)
{
    static const volatile uint32_t result = 0x00cdbc10u;
    return result;
}

/* Native 006aa480; EAX word 00cdb170; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006aa480(void)
{
    static const volatile uint32_t result = 0x00cdb170u;
    return result;
}

/* Native 006aa490; EAX word 00cdba10; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006aa490(void)
{
    static const volatile uint32_t result = 0x00cdba10u;
    return result;
}

/* Native 006aa4a0; EAX word 00cdb230; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006aa4a0(void)
{
    static const volatile uint32_t result = 0x00cdb230u;
    return result;
}

/* Native 006aa4b0; EAX word 00cdc9a0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006aa4b0(void)
{
    static const volatile uint32_t result = 0x00cdc9a0u;
    return result;
}

/* Native 006aa4f0; EAX word 00cdca40; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006aa4f0(void)
{
    static const volatile uint32_t result = 0x00cdca40u;
    return result;
}

/* Native 006aa530; EAX word 00cdafa8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006aa530(void)
{
    static const volatile uint32_t result = 0x00cdafa8u;
    return result;
}

/* Native 006aa570; EAX word 00cdafe0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006aa570(void)
{
    static const volatile uint32_t result = 0x00cdafe0u;
    return result;
}

/* Native 006aa5b0; EAX word 00cc756c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006aa5b0(void)
{
    static const volatile uint32_t result = 0x00cc756cu;
    return result;
}

/* Native 006aa5f0; EAX word 00cc759c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006aa5f0(void)
{
    static const volatile uint32_t result = 0x00cc759cu;
    return result;
}

/* Native 006aa630; EAX word 00cdc66c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006aa630(void)
{
    static const volatile uint32_t result = 0x00cdc66cu;
    return result;
}

/* Native 006aa670; EAX word 00ccc910; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006aa670(void)
{
    static const volatile uint32_t result = 0x00ccc910u;
    return result;
}

/* Native 006aa6b0; EAX word 00cdacb4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006aa6b0(void)
{
    static const volatile uint32_t result = 0x00cdacb4u;
    return result;
}

/* Native 006aa750; EAX word 00cc7538; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006aa750(void)
{
    static const volatile uint32_t result = 0x00cc7538u;
    return result;
}

/* Native 006b5cb0; EAX word 00ce150c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006b5cb0(void)
{
    static const volatile uint32_t result = 0x00ce150cu;
    return result;
}

/* Native 006b5e40; EAX word 00ce166c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006b5e40(void)
{
    static const volatile uint32_t result = 0x00ce166cu;
    return result;
}

/* Native 006b5eb0; EAX word 00ce16f4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006b5eb0(void)
{
    static const volatile uint32_t result = 0x00ce16f4u;
    return result;
}

/* Native 006b5f20; EAX word 00ce1770; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006b5f20(void)
{
    static const volatile uint32_t result = 0x00ce1770u;
    return result;
}

/* Native 006b5f90; EAX word 00ce17f0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006b5f90(void)
{
    static const volatile uint32_t result = 0x00ce17f0u;
    return result;
}

/* Native 006b6000; EAX word 00ce1874; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006b6000(void)
{
    static const volatile uint32_t result = 0x00ce1874u;
    return result;
}

/* Native 006b6160; EAX word 00ccc6f0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006b6160(void)
{
    static const volatile uint32_t result = 0x00ccc6f0u;
    return result;
}

/* Native 006b6170; EAX word 00cd76a0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006b6170(void)
{
    static const volatile uint32_t result = 0x00cd76a0u;
    return result;
}

/* Native 006b6180; EAX word 00cdac20; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006b6180(void)
{
    static const volatile uint32_t result = 0x00cdac20u;
    return result;
}

/* Native 006b6190; EAX word 00cdad30; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006b6190(void)
{
    static const volatile uint32_t result = 0x00cdad30u;
    return result;
}

/* Native 006b61a0; EAX word 00cdacf0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006b61a0(void)
{
    static const volatile uint32_t result = 0x00cdacf0u;
    return result;
}

/* Native 006b65b0; EAX word 00ce24f0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006b65b0(void)
{
    static const volatile uint32_t result = 0x00ce24f0u;
    return result;
}

/* Native 006b6690; EAX word 00ce25a4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006b6690(void)
{
    static const volatile uint32_t result = 0x00ce25a4u;
    return result;
}

/* Native 006b6760; EAX word 00ce264c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006b6760(void)
{
    static const volatile uint32_t result = 0x00ce264cu;
    return result;
}

/* Native 006b6b80; EAX word 00ce279c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006b6b80(void)
{
    static const volatile uint32_t result = 0x00ce279cu;
    return result;
}

/* Native 006becf0; EAX word 00ce573c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006becf0(void)
{
    static const volatile uint32_t result = 0x00ce573cu;
    return result;
}

/* Native 006bed60; EAX word 00ce57bc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006bed60(void)
{
    static const volatile uint32_t result = 0x00ce57bcu;
    return result;
}

/* Native 006bedd0; EAX word 00ce583c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006bedd0(void)
{
    static const volatile uint32_t result = 0x00ce583cu;
    return result;
}

/* Native 006bee40; EAX word 00ce58ac; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006bee40(void)
{
    static const volatile uint32_t result = 0x00ce58acu;
    return result;
}

/* Native 006beeb0; EAX word 00ce5928; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006beeb0(void)
{
    static const volatile uint32_t result = 0x00ce5928u;
    return result;
}

/* Native 006beff0; EAX word 00cd8b80; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006beff0(void)
{
    static const volatile uint32_t result = 0x00cd8b80u;
    return result;
}

/* Native 006bf000; EAX word 00ce55d0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006bf000(void)
{
    static const volatile uint32_t result = 0x00ce55d0u;
    return result;
}

/* Native 006bf010; EAX word 00ce5568; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006bf010(void)
{
    static const volatile uint32_t result = 0x00ce5568u;
    return result;
}

/* Native 006bf020; EAX word 00ce5690; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006bf020(void)
{
    static const volatile uint32_t result = 0x00ce5690u;
    return result;
}

/* Native 006bf030; EAX word 00ce5630; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006bf030(void)
{
    static const volatile uint32_t result = 0x00ce5630u;
    return result;
}

/* Native 006c3270; EAX word 00000493; RET 0. */
uint32_t SC bfv_client_return_word_006c3270(void)
{
    static const volatile uint32_t result = 0x00000493u;
    return result;
}

/* Native 006c5430; EAX word 0000003f; RET 0. */
uint32_t SC bfv_client_return_word_006c5430(void)
{
    static const volatile uint32_t result = 0x0000003fu;
    return result;
}

/* Native 006c72d0; EAX word 000019fd; RET 0. */
uint32_t SC bfv_client_return_word_006c72d0(void)
{
    static const volatile uint32_t result = 0x000019fdu;
    return result;
}

/* Native 006d7460; EAX word 00ce7b30; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006d7460(void)
{
    static const volatile uint32_t result = 0x00ce7b30u;
    return result;
}

/* Native 006d74d0; EAX word 00ce7ba8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006d74d0(void)
{
    static const volatile uint32_t result = 0x00ce7ba8u;
    return result;
}

/* Native 006d7540; EAX word 00ce7c20; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006d7540(void)
{
    static const volatile uint32_t result = 0x00ce7c20u;
    return result;
}

/* Native 006d75b0; EAX word 00ce7ca8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006d75b0(void)
{
    static const volatile uint32_t result = 0x00ce7ca8u;
    return result;
}

/* Native 006d7620; EAX word 00ce7d30; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006d7620(void)
{
    static const volatile uint32_t result = 0x00ce7d30u;
    return result;
}

/* Native 006d7690; EAX word 00ce7db0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006d7690(void)
{
    static const volatile uint32_t result = 0x00ce7db0u;
    return result;
}

/* Native 006d7840; EAX word 00cad410; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006d7840(void)
{
    static const volatile uint32_t result = 0x00cad410u;
    return result;
}

/* Native 006d7880; EAX word 00cc9f2c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006d7880(void)
{
    static const volatile uint32_t result = 0x00cc9f2cu;
    return result;
}

/* Native 006d78c0; EAX word 00cd88c4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006d78c0(void)
{
    static const volatile uint32_t result = 0x00cd88c4u;
    return result;
}

/* Native 006d7900; EAX word 00cd8bb0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006d7900(void)
{
    static const volatile uint32_t result = 0x00cd8bb0u;
    return result;
}

/* Native 006d7940; EAX word 00cad990; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006d7940(void)
{
    static const volatile uint32_t result = 0x00cad990u;
    return result;
}

/* Native 006d7980; EAX word 00cad674; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006d7980(void)
{
    static const volatile uint32_t result = 0x00cad674u;
    return result;
}

/* Native 006d7af0; EAX word 00ce86d0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006d7af0(void)
{
    static const volatile uint32_t result = 0x00ce86d0u;
    return result;
}

/* Native 006dc770; EAX word 00cea418; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006dc770(void)
{
    static const volatile uint32_t result = 0x00cea418u;
    return result;
}

/* Native 006dc840; EAX word 00cea528; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006dc840(void)
{
    static const volatile uint32_t result = 0x00cea528u;
    return result;
}

/* Native 006dc8b0; EAX word 00cea5a4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006dc8b0(void)
{
    static const volatile uint32_t result = 0x00cea5a4u;
    return result;
}

/* Native 006dc920; EAX word 00cea61c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006dc920(void)
{
    static const volatile uint32_t result = 0x00cea61cu;
    return result;
}

/* Native 006dc990; EAX word 00cea688; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006dc990(void)
{
    static const volatile uint32_t result = 0x00cea688u;
    return result;
}

/* Native 006dca00; EAX word 00cea700; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006dca00(void)
{
    static const volatile uint32_t result = 0x00cea700u;
    return result;
}

/* Native 006dca70; EAX word 00cea774; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006dca70(void)
{
    static const volatile uint32_t result = 0x00cea774u;
    return result;
}

/* Native 006dcae0; EAX word 00cea7e4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006dcae0(void)
{
    static const volatile uint32_t result = 0x00cea7e4u;
    return result;
}

/* Native 006dcb50; EAX word 00cea858; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006dcb50(void)
{
    static const volatile uint32_t result = 0x00cea858u;
    return result;
}

/* Native 006dcbc0; EAX word 00cea8d0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006dcbc0(void)
{
    static const volatile uint32_t result = 0x00cea8d0u;
    return result;
}

/* Native 006dcc30; EAX word 00cea950; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006dcc30(void)
{
    static const volatile uint32_t result = 0x00cea950u;
    return result;
}

/* Native 006dcca0; EAX word 00cea9c4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006dcca0(void)
{
    static const volatile uint32_t result = 0x00cea9c4u;
    return result;
}

/* Native 006dcd10; EAX word 00ceaa34; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006dcd10(void)
{
    static const volatile uint32_t result = 0x00ceaa34u;
    return result;
}

/* Native 006dd250; EAX word 00cea49c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006dd250(void)
{
    static const volatile uint32_t result = 0x00cea49cu;
    return result;
}

/* Native 006dd260; EAX word 00cea394; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006dd260(void)
{
    static const volatile uint32_t result = 0x00cea394u;
    return result;
}

/* Native 006dd270; EAX word 00ceabc4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006dd270(void)
{
    static const volatile uint32_t result = 0x00ceabc4u;
    return result;
}

/* Native 006dd280; EAX word 00ceaab0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006dd280(void)
{
    static const volatile uint32_t result = 0x00ceaab0u;
    return result;
}

/* Native 006dd290; EAX word 00ceab34; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006dd290(void)
{
    static const volatile uint32_t result = 0x00ceab34u;
    return result;
}

/* Native 006dd2a0; EAX word 00ceac44; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006dd2a0(void)
{
    static const volatile uint32_t result = 0x00ceac44u;
    return result;
}

/* Native 006dd2c0; EAX word 00ceacec; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006dd2c0(void)
{
    static const volatile uint32_t result = 0x00ceacecu;
    return result;
}

/* Native 006dd300; EAX word 00cc0388; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006dd300(void)
{
    static const volatile uint32_t result = 0x00cc0388u;
    return result;
}

/* Native 006dd340; EAX word 00ceacac; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006dd340(void)
{
    static const volatile uint32_t result = 0x00ceacacu;
    return result;
}

/* Native 006dd380; EAX word 00cead54; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006dd380(void)
{
    static const volatile uint32_t result = 0x00cead54u;
    return result;
}

/* Native 006dd3c0; EAX word 00ceac78; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006dd3c0(void)
{
    static const volatile uint32_t result = 0x00ceac78u;
    return result;
}

/* Native 006dd400; EAX word 00cead28; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006dd400(void)
{
    static const volatile uint32_t result = 0x00cead28u;
    return result;
}

/* Native 006dd440; EAX word 00ca747c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006dd440(void)
{
    static const volatile uint32_t result = 0x00ca747cu;
    return result;
}

/* Native 006dd480; EAX word 00ceb768; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006dd480(void)
{
    static const volatile uint32_t result = 0x00ceb768u;
    return result;
}

/* Native 006dd4c0; EAX word 00ce9b20; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006dd4c0(void)
{
    static const volatile uint32_t result = 0x00ce9b20u;
    return result;
}

/* Native 006dd500; EAX word 00ce9ca0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006dd500(void)
{
    static const volatile uint32_t result = 0x00ce9ca0u;
    return result;
}

/* Native 006dd540; EAX word 00ce9ccc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006dd540(void)
{
    static const volatile uint32_t result = 0x00ce9cccu;
    return result;
}

/* Native 006dd580; EAX word 00cd5e08; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006dd580(void)
{
    static const volatile uint32_t result = 0x00cd5e08u;
    return result;
}

/* Native 006dd5c0; EAX word 00ce9d30; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006dd5c0(void)
{
    static const volatile uint32_t result = 0x00ce9d30u;
    return result;
}

/* Native 006dd600; EAX word 00ceb3c8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006dd600(void)
{
    static const volatile uint32_t result = 0x00ceb3c8u;
    return result;
}

/* Native 006dd640; EAX word 00ceb4b4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006dd640(void)
{
    static const volatile uint32_t result = 0x00ceb4b4u;
    return result;
}

/* Native 006dd680; EAX word 00cead84; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006dd680(void)
{
    static const volatile uint32_t result = 0x00cead84u;
    return result;
}

/* Native 006dd6c0; EAX word 00ceba6c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006dd6c0(void)
{
    static const volatile uint32_t result = 0x00ceba6cu;
    return result;
}

/* Native 006dd700; EAX word 00ceb5b4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006dd700(void)
{
    static const volatile uint32_t result = 0x00ceb5b4u;
    return result;
}

/* Native 006dd740; EAX word 00ceb864; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006dd740(void)
{
    static const volatile uint32_t result = 0x00ceb864u;
    return result;
}

/* Native 006eab40; EAX word 00400000; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_006eab40(void)
{
    static const volatile uint32_t result = 0x00400000u;
    return result;
}

/* Native 00727110; EAX word 00cf4a14; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00727110(void)
{
    static const volatile uint32_t result = 0x00cf4a14u;
    return result;
}

/* Native 00727180; EAX word 00cf4a80; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00727180(void)
{
    static const volatile uint32_t result = 0x00cf4a80u;
    return result;
}

/* Native 007271f0; EAX word 00cf4b04; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_007271f0(void)
{
    static const volatile uint32_t result = 0x00cf4b04u;
    return result;
}

/* Native 00727260; EAX word 00cf4b78; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00727260(void)
{
    static const volatile uint32_t result = 0x00cf4b78u;
    return result;
}

/* Native 007272d0; EAX word 00cf4bec; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_007272d0(void)
{
    static const volatile uint32_t result = 0x00cf4becu;
    return result;
}

/* Native 007273d0; EAX word 00cf4e34; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_007273d0(void)
{
    static const volatile uint32_t result = 0x00cf4e34u;
    return result;
}

/* Native 00727410; EAX word 00cf4e9c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00727410(void)
{
    static const volatile uint32_t result = 0x00cf4e9cu;
    return result;
}

/* Native 00727450; EAX word 00cf1f68; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00727450(void)
{
    static const volatile uint32_t result = 0x00cf1f68u;
    return result;
}

/* Native 00727490; EAX word 00cf0d88; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00727490(void)
{
    static const volatile uint32_t result = 0x00cf0d88u;
    return result;
}

/* Native 007274d0; EAX word 00cf4fa0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_007274d0(void)
{
    static const volatile uint32_t result = 0x00cf4fa0u;
    return result;
}

/* Native 00727600; EAX word 00cf495c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00727600(void)
{
    static const volatile uint32_t result = 0x00cf495cu;
    return result;
}

/* Native 00727610; EAX word 00cf50bc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00727610(void)
{
    static const volatile uint32_t result = 0x00cf50bcu;
    return result;
}

/* Native 0073b570; EAX word 00cf6fd8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0073b570(void)
{
    static const volatile uint32_t result = 0x00cf6fd8u;
    return result;
}

/* Native 0073b5e0; EAX word 00cf7050; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0073b5e0(void)
{
    static const volatile uint32_t result = 0x00cf7050u;
    return result;
}

/* Native 0073b650; EAX word 00cf70c4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0073b650(void)
{
    static const volatile uint32_t result = 0x00cf70c4u;
    return result;
}

/* Native 0073b6c0; EAX word 00cf713c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0073b6c0(void)
{
    static const volatile uint32_t result = 0x00cf713cu;
    return result;
}

/* Native 0073b870; EAX word 00cf71b8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0073b870(void)
{
    static const volatile uint32_t result = 0x00cf71b8u;
    return result;
}

/* Native 0073b890; EAX word 00cf42f8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0073b890(void)
{
    static const volatile uint32_t result = 0x00cf42f8u;
    return result;
}

/* Native 0073b8d0; EAX word 00cf4264; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0073b8d0(void)
{
    static const volatile uint32_t result = 0x00cf4264u;
    return result;
}

/* Native 0073b910; EAX word 00cf4238; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0073b910(void)
{
    static const volatile uint32_t result = 0x00cf4238u;
    return result;
}

/* Native 0073b950; EAX word 00cf71e8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0073b950(void)
{
    static const volatile uint32_t result = 0x00cf71e8u;
    return result;
}

/* Native 0073b990; EAX word 00cf74bc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0073b990(void)
{
    static const volatile uint32_t result = 0x00cf74bcu;
    return result;
}

/* Native 0074ae90; EAX word 00cf8e00; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0074ae90(void)
{
    static const volatile uint32_t result = 0x00cf8e00u;
    return result;
}

/* Native 0074afc0; EAX word 00cf9500; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0074afc0(void)
{
    static const volatile uint32_t result = 0x00cf9500u;
    return result;
}

/* Native 0074afd0; EAX word 00cc03b8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0074afd0(void)
{
    static const volatile uint32_t result = 0x00cc03b8u;
    return result;
}

/* Native 0074afe0; EAX word 00cf9590; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0074afe0(void)
{
    static const volatile uint32_t result = 0x00cf9590u;
    return result;
}

/* Native 0074aff0; EAX word 00cf261c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0074aff0(void)
{
    static const volatile uint32_t result = 0x00cf261cu;
    return result;
}

/* Native 0074b040; EAX word 00cf2538; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0074b040(void)
{
    static const volatile uint32_t result = 0x00cf2538u;
    return result;
}

/* Native 00750600; EAX word 00cf9e08; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00750600(void)
{
    static const volatile uint32_t result = 0x00cf9e08u;
    return result;
}

/* Native 007506b0; EAX word 00cf9c38; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_007506b0(void)
{
    static const volatile uint32_t result = 0x00cf9c38u;
    return result;
}

/* Native 0076b3e0; EAX word 00000001; RET 12. */
uint32_t SC bfv_client_return_word_0076b3e0(uint32_t unused0 __attribute__((unused)), uint32_t unused1 __attribute__((unused)), uint32_t unused2 __attribute__((unused)))
{
    static const volatile uint32_t result = 0x00000001u;
    return result;
}

/* Native 00779c90; EAX word 00cfafec; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00779c90(void)
{
    static const volatile uint32_t result = 0x00cfafecu;
    return result;
}

/* Native 00779d00; EAX word 00cfb020; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00779d00(void)
{
    static const volatile uint32_t result = 0x00cfb020u;
    return result;
}

/* Native 00779d70; EAX word 00cfb050; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00779d70(void)
{
    static const volatile uint32_t result = 0x00cfb050u;
    return result;
}

/* Native 00779e30; EAX word 00cfb080; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00779e30(void)
{
    static const volatile uint32_t result = 0x00cfb080u;
    return result;
}

/* Native 00779ee0; EAX word 00cfb0b0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00779ee0(void)
{
    static const volatile uint32_t result = 0x00cfb0b0u;
    return result;
}

/* Native 00779f50; EAX word 00cfb0e0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00779f50(void)
{
    static const volatile uint32_t result = 0x00cfb0e0u;
    return result;
}

/* Native 00779fc0; EAX word 00cfb110; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00779fc0(void)
{
    static const volatile uint32_t result = 0x00cfb110u;
    return result;
}

/* Native 007816e0; EAX word 00cfbc48; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_007816e0(void)
{
    static const volatile uint32_t result = 0x00cfbc48u;
    return result;
}

/* Native 007816f0; EAX word 00cfbc28; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_007816f0(void)
{
    static const volatile uint32_t result = 0x00cfbc28u;
    return result;
}

/* Native 007817a0; EAX word 00cfbc88; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_007817a0(void)
{
    static const volatile uint32_t result = 0x00cfbc88u;
    return result;
}

/* Native 00781810; EAX word 00cfbce4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00781810(void)
{
    static const volatile uint32_t result = 0x00cfbce4u;
    return result;
}

/* Native 00783180; EAX word 00cfbda4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00783180(void)
{
    static const volatile uint32_t result = 0x00cfbda4u;
    return result;
}

/* Native 00783190; EAX word 00cfbdc8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00783190(void)
{
    static const volatile uint32_t result = 0x00cfbdc8u;
    return result;
}

/* Native 0079f350; EAX word 0003ad69; RET 0. */
uint32_t SC bfv_client_return_word_0079f350(void)
{
    static const volatile uint32_t result = 0x0003ad69u;
    return result;
}

/* Native 007ae1b0; EAX word 00df00e0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_007ae1b0(void)
{
    static const volatile uint32_t result = 0x00df00e0u;
    return result;
}

/* Native 007b2270; EAX word 00bb64b0; RET 0. Original data identifier: Select. */
uint32_t SC bfv_client_return_word_007b2270(void)
{
    static const volatile uint32_t result = 0x00bb64b0u;
    return result;
}

/* Native 007b2280; EAX word 00000031; RET 0. */
uint32_t SC bfv_client_return_word_007b2280(void)
{
    static const volatile uint32_t result = 0x00000031u;
    return result;
}

/* Native 007b2290; EAX word 00bb6838; RET 0. Original data identifier: Test. */
uint32_t SC bfv_client_return_word_007b2290(void)
{
    static const volatile uint32_t result = 0x00bb6838u;
    return result;
}

/* Native 007b2420; EAX word 00bb64a8; RET 0. Original data identifier: Rotate. */
uint32_t SC bfv_client_return_word_007b2420(void)
{
    static const volatile uint32_t result = 0x00bb64a8u;
    return result;
}

/* Native 007b2430; EAX word 00000033; RET 0. */
uint32_t SC bfv_client_return_word_007b2430(void)
{
    static const volatile uint32_t result = 0x00000033u;
    return result;
}

/* Native 007b33e0; EAX word 00bb64b8; RET 0. Original data identifier: Move. */
uint32_t SC bfv_client_return_word_007b33e0(void)
{
    static const volatile uint32_t result = 0x00bb64b8u;
    return result;
}

/* Native 007b33f0; EAX word 00000032; RET 0. */
uint32_t SC bfv_client_return_word_007b33f0(void)
{
    static const volatile uint32_t result = 0x00000032u;
    return result;
}

/* Native 007b4820; EAX word 00bb69dc; RET 0. Original data identifier: Link. */
uint32_t SC bfv_client_return_word_007b4820(void)
{
    static const volatile uint32_t result = 0x00bb69dcu;
    return result;
}

/* Native 007b4830; EAX word 00000039; RET 0. */
uint32_t SC bfv_client_return_word_007b4830(void)
{
    static const volatile uint32_t result = 0x00000039u;
    return result;
}

/* Native 007b4840; EAX word 00bb6a14; RET 0. Original data identifier: Delete. */
uint32_t SC bfv_client_return_word_007b4840(void)
{
    static const volatile uint32_t result = 0x00bb6a14u;
    return result;
}

/* Native 007b4850; EAX word 00bb6a4c; RET 0. Original data identifier: Replace. */
uint32_t SC bfv_client_return_word_007b4850(void)
{
    static const volatile uint32_t result = 0x00bb6a4cu;
    return result;
}

/* Native 007b4860; EAX word 00000037; RET 0. */
uint32_t SC bfv_client_return_word_007b4860(void)
{
    static const volatile uint32_t result = 0x00000037u;
    return result;
}

/* Native 007b4cf0; EAX word 00bb6b14; RET 0. Original data identifier: Create. */
uint32_t SC bfv_client_return_word_007b4cf0(void)
{
    static const volatile uint32_t result = 0x00bb6b14u;
    return result;
}

/* Native 007b4d00; EAX word 00000035; RET 0. */
uint32_t SC bfv_client_return_word_007b4d00(void)
{
    static const volatile uint32_t result = 0x00000035u;
    return result;
}

/* Native 007c66d0; EAX word 00e22d24; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_007c66d0(void)
{
    static const volatile uint32_t result = 0x00e22d24u;
    return result;
}

/* Native 0081f410; EAX word 000f4240; RET 4. */
uint32_t SC bfv_client_return_word_0081f410(uint32_t unused0 __attribute__((unused)))
{
    static const volatile uint32_t result = 0x000f4240u;
    return result;
}

/* Native 00868ca0; EAX word 00000070; RET 0. */
uint32_t SC bfv_client_return_word_00868ca0(void)
{
    static const volatile uint32_t result = 0x00000070u;
    return result;
}

/* Native 00869f60; EAX word 00bd279c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00869f60(void)
{
    static const volatile uint32_t result = 0x00bd279cu;
    return result;
}

/* Native 00869fe0; EAX word 00bd27e4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00869fe0(void)
{
    static const volatile uint32_t result = 0x00bd27e4u;
    return result;
}

/* Native 0086e680; EAX word 00000048; RET 0. */
uint32_t SC bfv_client_return_word_0086e680(void)
{
    static const volatile uint32_t result = 0x00000048u;
    return result;
}

/* Native 00874600; EAX word 00000001; RET 4. */
uint32_t SC bfv_client_return_word_00874600(uint32_t unused0 __attribute__((unused)))
{
    static const volatile uint32_t result = 0x00000001u;
    return result;
}

/* Native 00875310; EAX word 00df68fc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00875310(void)
{
    static const volatile uint32_t result = 0x00df68fcu;
    return result;
}

/* Native 0087a120; EAX word 00000814; RET 0. */
uint32_t SC bfv_client_return_word_0087a120(void)
{
    static const volatile uint32_t result = 0x00000814u;
    return result;
}

/* Native 0087ba60; EAX word 00000478; RET 0. */
uint32_t SC bfv_client_return_word_0087ba60(void)
{
    static const volatile uint32_t result = 0x00000478u;
    return result;
}

/* Native 00885170; EAX word 00d0d71c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00885170(void)
{
    static const volatile uint32_t result = 0x00d0d71cu;
    return result;
}

/* Native 008851e0; EAX word 00d0d744; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_008851e0(void)
{
    static const volatile uint32_t result = 0x00d0d744u;
    return result;
}

/* Native 00885250; EAX word 00d0d778; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00885250(void)
{
    static const volatile uint32_t result = 0x00d0d778u;
    return result;
}

/* Native 00885330; EAX word 00d0db64; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00885330(void)
{
    static const volatile uint32_t result = 0x00d0db64u;
    return result;
}

/* Native 008853a0; EAX word 00d0dbc4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_008853a0(void)
{
    static const volatile uint32_t result = 0x00d0dbc4u;
    return result;
}

/* Native 00885410; EAX word 00d0dc24; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00885410(void)
{
    static const volatile uint32_t result = 0x00d0dc24u;
    return result;
}

/* Native 00885480; EAX word 00d0dc7c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00885480(void)
{
    static const volatile uint32_t result = 0x00d0dc7cu;
    return result;
}

/* Native 00885550; EAX word 00d0dd3c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00885550(void)
{
    static const volatile uint32_t result = 0x00d0dd3cu;
    return result;
}

/* Native 00885670; EAX word 00cbaff8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00885670(void)
{
    static const volatile uint32_t result = 0x00cbaff8u;
    return result;
}

/* Native 00885680; EAX word 00ce85e0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00885680(void)
{
    static const volatile uint32_t result = 0x00ce85e0u;
    return result;
}

/* Native 00885690; EAX word 00cbff48; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00885690(void)
{
    static const volatile uint32_t result = 0x00cbff48u;
    return result;
}

/* Native 008856a0; EAX word 00cbff68; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_008856a0(void)
{
    static const volatile uint32_t result = 0x00cbff68u;
    return result;
}

/* Native 008856b0; EAX word 00d0e124; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_008856b0(void)
{
    static const volatile uint32_t result = 0x00d0e124u;
    return result;
}

/* Native 008856c0; EAX word 00ce15ec; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_008856c0(void)
{
    static const volatile uint32_t result = 0x00ce15ecu;
    return result;
}

/* Native 008856d0; EAX word 00ce157c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_008856d0(void)
{
    static const volatile uint32_t result = 0x00ce157cu;
    return result;
}

/* Native 008856e0; EAX word 00ce2430; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_008856e0(void)
{
    static const volatile uint32_t result = 0x00ce2430u;
    return result;
}

/* Native 00885800; EAX word 00d0dcdc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00885800(void)
{
    static const volatile uint32_t result = 0x00d0dcdcu;
    return result;
}

/* Native 00885810; EAX word 00d0dda4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00885810(void)
{
    static const volatile uint32_t result = 0x00d0dda4u;
    return result;
}

/* Native 00885820; EAX word 00d0e714; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00885820(void)
{
    static const volatile uint32_t result = 0x00d0e714u;
    return result;
}

/* Native 00885860; EAX word 00d0e774; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00885860(void)
{
    static const volatile uint32_t result = 0x00d0e774u;
    return result;
}

/* Native 008858a0; EAX word 00d0e3d0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_008858a0(void)
{
    static const volatile uint32_t result = 0x00d0e3d0u;
    return result;
}

/* Native 008858e0; EAX word 00d0e808; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_008858e0(void)
{
    static const volatile uint32_t result = 0x00d0e808u;
    return result;
}

/* Native 00885920; EAX word 00d0e544; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00885920(void)
{
    static const volatile uint32_t result = 0x00d0e544u;
    return result;
}

/* Native 00885960; EAX word 00d0e89c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00885960(void)
{
    static const volatile uint32_t result = 0x00d0e89cu;
    return result;
}

/* Native 008859a0; EAX word 00d0e620; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_008859a0(void)
{
    static const volatile uint32_t result = 0x00d0e620u;
    return result;
}

/* Native 00885e70; EAX word 00d0f190; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00885e70(void)
{
    static const volatile uint32_t result = 0x00d0f190u;
    return result;
}

/* Native 00885f10; EAX word 00d0f260; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00885f10(void)
{
    static const volatile uint32_t result = 0x00d0f260u;
    return result;
}

/* Native 00886140; EAX word 00d0f3d0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00886140(void)
{
    static const volatile uint32_t result = 0x00d0f3d0u;
    return result;
}

/* Native 00886490; EAX word 00d0de14; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00886490(void)
{
    static const volatile uint32_t result = 0x00d0de14u;
    return result;
}

/* Native 008864a0; EAX word 00d0f6c0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_008864a0(void)
{
    static const volatile uint32_t result = 0x00d0f6c0u;
    return result;
}

/* Native 0088ba10; EAX word 00d1027c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0088ba10(void)
{
    static const volatile uint32_t result = 0x00d1027cu;
    return result;
}

/* Native 0088c0f0; EAX word 00d0e900; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0088c0f0(void)
{
    static const volatile uint32_t result = 0x00d0e900u;
    return result;
}

/* Native 0088cdd0; EAX word 00d0eb64; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0088cdd0(void)
{
    static const volatile uint32_t result = 0x00d0eb64u;
    return result;
}

/* Native 0088d560; EAX word 00d0eba4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0088d560(void)
{
    static const volatile uint32_t result = 0x00d0eba4u;
    return result;
}

/* Native 0088dd40; EAX word 00d0ebe4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0088dd40(void)
{
    static const volatile uint32_t result = 0x00d0ebe4u;
    return result;
}

/* Native 0088e4d0; EAX word 00d0ec28; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0088e4d0(void)
{
    static const volatile uint32_t result = 0x00d0ec28u;
    return result;
}

/* Native 0088eca0; EAX word 00d0ed78; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0088eca0(void)
{
    static const volatile uint32_t result = 0x00d0ed78u;
    return result;
}

/* Native 008c0b70; EAX word 000005c0; RET 0. */
uint32_t SC bfv_client_return_word_008c0b70(void)
{
    static const volatile uint32_t result = 0x000005c0u;
    return result;
}

/* Native 008d9bd0; EAX word 00000009; RET 4. */
uint32_t SC bfv_client_return_word_008d9bd0(uint32_t unused0 __attribute__((unused)))
{
    static const volatile uint32_t result = 0x00000009u;
    return result;
}

/* Native 0094a5f0; EAX word 00d1b3e0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0094a5f0(void)
{
    static const volatile uint32_t result = 0x00d1b3e0u;
    return result;
}

/* Native 0094a660; EAX word 00d1b444; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0094a660(void)
{
    static const volatile uint32_t result = 0x00d1b444u;
    return result;
}

/* Native 0094a6d0; EAX word 00d1b4bc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0094a6d0(void)
{
    static const volatile uint32_t result = 0x00d1b4bcu;
    return result;
}

/* Native 0094a740; EAX word 00d1b538; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0094a740(void)
{
    static const volatile uint32_t result = 0x00d1b538u;
    return result;
}

/* Native 0094a7b0; EAX word 00d1b5b4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0094a7b0(void)
{
    static const volatile uint32_t result = 0x00d1b5b4u;
    return result;
}

/* Native 0094a820; EAX word 00d1b62c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0094a820(void)
{
    static const volatile uint32_t result = 0x00d1b62cu;
    return result;
}

/* Native 0094a890; EAX word 00d1b68c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0094a890(void)
{
    static const volatile uint32_t result = 0x00d1b68cu;
    return result;
}

/* Native 0094a900; EAX word 00d1b6ec; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0094a900(void)
{
    static const volatile uint32_t result = 0x00d1b6ecu;
    return result;
}

/* Native 0094a970; EAX word 00d1b750; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0094a970(void)
{
    static const volatile uint32_t result = 0x00d1b750u;
    return result;
}

/* Native 0094a9e0; EAX word 00d1b7b8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0094a9e0(void)
{
    static const volatile uint32_t result = 0x00d1b7b8u;
    return result;
}

/* Native 0094aa50; EAX word 00d1b82c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0094aa50(void)
{
    static const volatile uint32_t result = 0x00d1b82cu;
    return result;
}

/* Native 0094aac0; EAX word 00d1b898; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0094aac0(void)
{
    static const volatile uint32_t result = 0x00d1b898u;
    return result;
}

/* Native 0094ab30; EAX word 00d1b900; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0094ab30(void)
{
    static const volatile uint32_t result = 0x00d1b900u;
    return result;
}

/* Native 0094aba0; EAX word 00d1b970; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0094aba0(void)
{
    static const volatile uint32_t result = 0x00d1b970u;
    return result;
}

/* Native 0094ac10; EAX word 00d1b9ec; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0094ac10(void)
{
    static const volatile uint32_t result = 0x00d1b9ecu;
    return result;
}

/* Native 0094ac80; EAX word 00d1ba58; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0094ac80(void)
{
    static const volatile uint32_t result = 0x00d1ba58u;
    return result;
}

/* Native 0094acf0; EAX word 00d1babc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0094acf0(void)
{
    static const volatile uint32_t result = 0x00d1babcu;
    return result;
}

/* Native 0094b3a0; EAX word 00d1e1c0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0094b3a0(void)
{
    static const volatile uint32_t result = 0x00d1e1c0u;
    return result;
}

/* Native 0094b3e0; EAX word 00d1a870; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0094b3e0(void)
{
    static const volatile uint32_t result = 0x00d1a870u;
    return result;
}

/* Native 0094b420; EAX word 00d1a808; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0094b420(void)
{
    static const volatile uint32_t result = 0x00d1a808u;
    return result;
}

/* Native 0094b460; EAX word 00d1a7d8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0094b460(void)
{
    static const volatile uint32_t result = 0x00d1a7d8u;
    return result;
}

/* Native 0094b4a0; EAX word 00d1a838; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0094b4a0(void)
{
    static const volatile uint32_t result = 0x00d1a838u;
    return result;
}

/* Native 0094b4e0; EAX word 00d1a7ac; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0094b4e0(void)
{
    static const volatile uint32_t result = 0x00d1a7acu;
    return result;
}

/* Native 0094b520; EAX word 00d1a08c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0094b520(void)
{
    static const volatile uint32_t result = 0x00d1a08cu;
    return result;
}

/* Native 0094b560; EAX word 00d1a1a4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0094b560(void)
{
    static const volatile uint32_t result = 0x00d1a1a4u;
    return result;
}

/* Native 0094b5a0; EAX word 00d1a17c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0094b5a0(void)
{
    static const volatile uint32_t result = 0x00d1a17cu;
    return result;
}

/* Native 0094b5e0; EAX word 00d1a1cc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0094b5e0(void)
{
    static const volatile uint32_t result = 0x00d1a1ccu;
    return result;
}

/* Native 0094b620; EAX word 00d1cc24; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0094b620(void)
{
    static const volatile uint32_t result = 0x00d1cc24u;
    return result;
}

/* Native 0094b660; EAX word 00d1a1f8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0094b660(void)
{
    static const volatile uint32_t result = 0x00d1a1f8u;
    return result;
}

/* Native 0094b6a0; EAX word 00d1a220; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0094b6a0(void)
{
    static const volatile uint32_t result = 0x00d1a220u;
    return result;
}

/* Native 0094b6e0; EAX word 00d1d898; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0094b6e0(void)
{
    static const volatile uint32_t result = 0x00d1d898u;
    return result;
}

/* Native 0094b720; EAX word 00d1a128; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0094b720(void)
{
    static const volatile uint32_t result = 0x00d1a128u;
    return result;
}

/* Native 0094b760; EAX word 00d1a154; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0094b760(void)
{
    static const volatile uint32_t result = 0x00d1a154u;
    return result;
}

/* Native 0094b7a0; EAX word 00d1a0d4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_0094b7a0(void)
{
    static const volatile uint32_t result = 0x00d1a0d4u;
    return result;
}

/* Native 00969890; EAX word 08000000; RET 0. */
uint32_t SC bfv_client_return_word_00969890(void)
{
    static const volatile uint32_t result = 0x08000000u;
    return result;
}

/* Native 009842d0; EAX word 00000003; RET 0. */
uint32_t SC bfv_client_return_word_009842d0(void)
{
    static const volatile uint32_t result = 0x00000003u;
    return result;
}

/* Native 00984530; EAX word 00000002; RET 0. */
uint32_t SC bfv_client_return_word_00984530(void)
{
    static const volatile uint32_t result = 0x00000002u;
    return result;
}

/* Native 00996f70; EAX word 00000029; RET 0. */
uint32_t SC bfv_client_return_word_00996f70(void)
{
    static const volatile uint32_t result = 0x00000029u;
    return result;
}

/* Native 009dc530; EAX word 0000000a; RET 0. */
uint32_t SC bfv_client_return_word_009dc530(void)
{
    static const volatile uint32_t result = 0x0000000au;
    return result;
}

/* Native 009f20d0; EAX word 0000000d; RET 0. */
uint32_t SC bfv_client_return_word_009f20d0(void)
{
    static const volatile uint32_t result = 0x0000000du;
    return result;
}

/* Native 009f2300; EAX word 0000000e; RET 0. */
uint32_t SC bfv_client_return_word_009f2300(void)
{
    static const volatile uint32_t result = 0x0000000eu;
    return result;
}

/* Native 009f2890; EAX word 00000020; RET 0. */
uint32_t SC bfv_client_return_word_009f2890(void)
{
    static const volatile uint32_t result = 0x00000020u;
    return result;
}

/* Native 009f2a10; EAX word 0000001c; RET 0. */
uint32_t SC bfv_client_return_word_009f2a10(void)
{
    static const volatile uint32_t result = 0x0000001cu;
    return result;
}

/* Native 009f2b30; EAX word 0000001a; RET 0. */
uint32_t SC bfv_client_return_word_009f2b30(void)
{
    static const volatile uint32_t result = 0x0000001au;
    return result;
}

/* Native 009f2d00; EAX word 00000022; RET 0. */
uint32_t SC bfv_client_return_word_009f2d00(void)
{
    static const volatile uint32_t result = 0x00000022u;
    return result;
}

/* Native 009f2df0; EAX word 00000023; RET 0. */
uint32_t SC bfv_client_return_word_009f2df0(void)
{
    static const volatile uint32_t result = 0x00000023u;
    return result;
}

/* Native 009f37b0; EAX word 00000025; RET 0. */
uint32_t SC bfv_client_return_word_009f37b0(void)
{
    static const volatile uint32_t result = 0x00000025u;
    return result;
}

/* Native 009f3d70; EAX word 00000017; RET 0. */
uint32_t SC bfv_client_return_word_009f3d70(void)
{
    static const volatile uint32_t result = 0x00000017u;
    return result;
}

/* Native 009f53a0; EAX word 00000005; RET 0. */
uint32_t SC bfv_client_return_word_009f53a0(void)
{
    static const volatile uint32_t result = 0x00000005u;
    return result;
}

/* Native 009f5880; EAX word 00000019; RET 0. */
uint32_t SC bfv_client_return_word_009f5880(void)
{
    static const volatile uint32_t result = 0x00000019u;
    return result;
}

/* Native 009f6cf0; EAX word 0000001e; RET 0. */
uint32_t SC bfv_client_return_word_009f6cf0(void)
{
    static const volatile uint32_t result = 0x0000001eu;
    return result;
}

/* Native 009f6de0; EAX word 0000000f; RET 0. */
uint32_t SC bfv_client_return_word_009f6de0(void)
{
    static const volatile uint32_t result = 0x0000000fu;
    return result;
}

/* Native 009f70b0; EAX word 00000015; RET 0. */
uint32_t SC bfv_client_return_word_009f70b0(void)
{
    static const volatile uint32_t result = 0x00000015u;
    return result;
}

/* Native 009f7720; EAX word 0000001b; RET 0. */
uint32_t SC bfv_client_return_word_009f7720(void)
{
    static const volatile uint32_t result = 0x0000001bu;
    return result;
}

/* Native 009f7840; EAX word 00000028; RET 0. */
uint32_t SC bfv_client_return_word_009f7840(void)
{
    static const volatile uint32_t result = 0x00000028u;
    return result;
}

/* Native 009f78f0; EAX word 00000021; RET 0. */
uint32_t SC bfv_client_return_word_009f78f0(void)
{
    static const volatile uint32_t result = 0x00000021u;
    return result;
}

/* Native 009f7f00; EAX word 00000010; RET 0. */
uint32_t SC bfv_client_return_word_009f7f00(void)
{
    static const volatile uint32_t result = 0x00000010u;
    return result;
}

/* Native 009f8330; EAX word 00000011; RET 0. */
uint32_t SC bfv_client_return_word_009f8330(void)
{
    static const volatile uint32_t result = 0x00000011u;
    return result;
}

/* Native 009f83e0; EAX word 00000012; RET 0. */
uint32_t SC bfv_client_return_word_009f83e0(void)
{
    static const volatile uint32_t result = 0x00000012u;
    return result;
}

/* Native 009f8590; EAX word 0000000b; RET 0. */
uint32_t SC bfv_client_return_word_009f8590(void)
{
    static const volatile uint32_t result = 0x0000000bu;
    return result;
}

/* Native 009f87a0; EAX word 0000000c; RET 0. */
uint32_t SC bfv_client_return_word_009f87a0(void)
{
    static const volatile uint32_t result = 0x0000000cu;
    return result;
}

/* Native 009f9090; EAX word 00000013; RET 0. */
uint32_t SC bfv_client_return_word_009f9090(void)
{
    static const volatile uint32_t result = 0x00000013u;
    return result;
}

/* Native 009f9290; EAX word 00000009; RET 0. */
uint32_t SC bfv_client_return_word_009f9290(void)
{
    static const volatile uint32_t result = 0x00000009u;
    return result;
}

/* Native 009f9390; EAX word 00000008; RET 0. */
uint32_t SC bfv_client_return_word_009f9390(void)
{
    static const volatile uint32_t result = 0x00000008u;
    return result;
}

/* Native 009f9660; EAX word 00000006; RET 0. */
uint32_t SC bfv_client_return_word_009f9660(void)
{
    static const volatile uint32_t result = 0x00000006u;
    return result;
}

/* Native 009f9830; EAX word 00000001; RET 0. */
uint32_t SC bfv_client_return_word_009f9830(void)
{
    static const volatile uint32_t result = 0x00000001u;
    return result;
}

/* Native 009f9dc0; EAX word 0000001d; RET 0. */
uint32_t SC bfv_client_return_word_009f9dc0(void)
{
    static const volatile uint32_t result = 0x0000001du;
    return result;
}

/* Native 009f9e30; EAX word 00000004; RET 0. */
uint32_t SC bfv_client_return_word_009f9e30(void)
{
    static const volatile uint32_t result = 0x00000004u;
    return result;
}

/* Native 009fa060; EAX word 00000016; RET 0. */
uint32_t SC bfv_client_return_word_009fa060(void)
{
    static const volatile uint32_t result = 0x00000016u;
    return result;
}

/* Native 009fc8c0; EAX word 00000007; RET 0. */
uint32_t SC bfv_client_return_word_009fc8c0(void)
{
    static const volatile uint32_t result = 0x00000007u;
    return result;
}

/* Native 00a37870; EAX word 00e173e8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00a37870(void)
{
    static const volatile uint32_t result = 0x00e173e8u;
    return result;
}

/* Native 00a43ed0; EAX word 00000001; RET 32. */
uint32_t SC bfv_client_return_word_00a43ed0(uint32_t unused0 __attribute__((unused)), uint32_t unused1 __attribute__((unused)), uint32_t unused2 __attribute__((unused)), uint32_t unused3 __attribute__((unused)), uint32_t unused4 __attribute__((unused)), uint32_t unused5 __attribute__((unused)), uint32_t unused6 __attribute__((unused)), uint32_t unused7 __attribute__((unused)))
{
    static const volatile uint32_t result = 0x00000001u;
    return result;
}

/* Native 00a43ef0; EAX word 00000001; RET 36. */
uint32_t SC bfv_client_return_word_00a43ef0(uint32_t unused0 __attribute__((unused)), uint32_t unused1 __attribute__((unused)), uint32_t unused2 __attribute__((unused)), uint32_t unused3 __attribute__((unused)), uint32_t unused4 __attribute__((unused)), uint32_t unused5 __attribute__((unused)), uint32_t unused6 __attribute__((unused)), uint32_t unused7 __attribute__((unused)), uint32_t unused8 __attribute__((unused)))
{
    static const volatile uint32_t result = 0x00000001u;
    return result;
}

/* Native 00a579a0; EAX word 00d2b258; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00a579a0(void)
{
    static const volatile uint32_t result = 0x00d2b258u;
    return result;
}

/* Native 00a57a10; EAX word 00d2b2c8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00a57a10(void)
{
    static const volatile uint32_t result = 0x00d2b2c8u;
    return result;
}

/* Native 00a57a80; EAX word 00d2b344; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00a57a80(void)
{
    static const volatile uint32_t result = 0x00d2b344u;
    return result;
}

/* Native 00a57af0; EAX word 00d2b3c8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00a57af0(void)
{
    static const volatile uint32_t result = 0x00d2b3c8u;
    return result;
}

/* Native 00a57b60; EAX word 00d2b438; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00a57b60(void)
{
    static const volatile uint32_t result = 0x00d2b438u;
    return result;
}

/* Native 00a57bd0; EAX word 00d2b4a8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00a57bd0(void)
{
    static const volatile uint32_t result = 0x00d2b4a8u;
    return result;
}

/* Native 00a57c40; EAX word 00d2b524; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00a57c40(void)
{
    static const volatile uint32_t result = 0x00d2b524u;
    return result;
}

/* Native 00a57e00; EAX word 00d2b660; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00a57e00(void)
{
    static const volatile uint32_t result = 0x00d2b660u;
    return result;
}

/* Native 00a57e40; EAX word 00d2b88c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00a57e40(void)
{
    static const volatile uint32_t result = 0x00d2b88cu;
    return result;
}

/* Native 00a57e80; EAX word 00d2b908; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00a57e80(void)
{
    static const volatile uint32_t result = 0x00d2b908u;
    return result;
}

/* Native 00a57ec0; EAX word 00d2b988; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00a57ec0(void)
{
    static const volatile uint32_t result = 0x00d2b988u;
    return result;
}

/* Native 00a57f00; EAX word 00d2a9a0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00a57f00(void)
{
    static const volatile uint32_t result = 0x00d2a9a0u;
    return result;
}

/* Native 00a57f40; EAX word 00cadb34; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00a57f40(void)
{
    static const volatile uint32_t result = 0x00cadb34u;
    return result;
}

/* Native 00a57f80; EAX word 00cadb04; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00a57f80(void)
{
    static const volatile uint32_t result = 0x00cadb04u;
    return result;
}

/* Native 00a57ff0; EAX word 00d2b068; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00a57ff0(void)
{
    static const volatile uint32_t result = 0x00d2b068u;
    return result;
}

/* Native 00a58000; EAX word 00d2bc38; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00a58000(void)
{
    static const volatile uint32_t result = 0x00d2bc38u;
    return result;
}

/* Native 00a580a0; EAX word 00d2afd8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00a580a0(void)
{
    static const volatile uint32_t result = 0x00d2afd8u;
    return result;
}

/* Native 00a580b0; EAX word 00d2bd20; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00a580b0(void)
{
    static const volatile uint32_t result = 0x00d2bd20u;
    return result;
}

/* Native 00a58150; EAX word 00d2af54; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00a58150(void)
{
    static const volatile uint32_t result = 0x00d2af54u;
    return result;
}

/* Native 00a58160; EAX word 00d2bdec; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00a58160(void)
{
    static const volatile uint32_t result = 0x00d2bdecu;
    return result;
}

/* Native 00a5c7a0; EAX word 00d2d864; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00a5c7a0(void)
{
    static const volatile uint32_t result = 0x00d2d864u;
    return result;
}

/* Native 00a5c810; EAX word 00d2d8e8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00a5c810(void)
{
    static const volatile uint32_t result = 0x00d2d8e8u;
    return result;
}

/* Native 00a5c880; EAX word 00d2d964; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00a5c880(void)
{
    static const volatile uint32_t result = 0x00d2d964u;
    return result;
}

/* Native 00a5c8f0; EAX word 00d2d9dc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00a5c8f0(void)
{
    static const volatile uint32_t result = 0x00d2d9dcu;
    return result;
}

/* Native 00a5c9c0; EAX word 00d2dafc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00a5c9c0(void)
{
    static const volatile uint32_t result = 0x00d2dafcu;
    return result;
}

/* Native 00a5ca00; EAX word 00d2a610; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00a5ca00(void)
{
    static const volatile uint32_t result = 0x00d2a610u;
    return result;
}

/* Native 00a5ca40; EAX word 00d2aa58; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00a5ca40(void)
{
    static const volatile uint32_t result = 0x00d2aa58u;
    return result;
}

/* Native 00a5cb50; EAX word 00d2dcac; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_client_return_word_00a5cb50(void)
{
    static const volatile uint32_t result = 0x00d2dcacu;
    return result;
}

/* Native 00a7f170; EAX word 00000800; RET 0. */
uint32_t SC bfv_client_return_word_00a7f170(void)
{
    static const volatile uint32_t result = 0x00000800u;
    return result;
}

/* Native 00a7f1a0; EAX word 00000200; RET 0. */
uint32_t SC bfv_client_return_word_00a7f1a0(void)
{
    static const volatile uint32_t result = 0x00000200u;
    return result;
}

/* Native 00abe4f0; EAX word 00000004; RET 4. */
uint32_t SC bfv_client_return_word_00abe4f0(uint32_t unused0 __attribute__((unused)))
{
    static const volatile uint32_t result = 0x00000004u;
    return result;
}

/* Native 00ac2ac0; EAX word 00000034; RET 0. */
uint32_t SC bfv_client_return_word_00ac2ac0(void)
{
    static const volatile uint32_t result = 0x00000034u;
    return result;
}

/* Native 00ac3950; EAX word 00000018; RET 0. */
uint32_t SC bfv_client_return_word_00ac3950(void)
{
    static const volatile uint32_t result = 0x00000018u;
    return result;
}

/* Native 00ac4490; EAX word 00000014; RET 0. */
uint32_t SC bfv_client_return_word_00ac4490(void)
{
    static const volatile uint32_t result = 0x00000014u;
    return result;
}

#endif

#if BFV_BUILD_SERVER
/* Native 00422af0; EAX word 00000003; RET 16. */
uint32_t SC bfv_server_return_word_00422af0(uint32_t unused0 __attribute__((unused)), uint32_t unused1 __attribute__((unused)), uint32_t unused2 __attribute__((unused)), uint32_t unused3 __attribute__((unused)))
{
    static const volatile uint32_t result = 0x00000003u;
    return result;
}

/* Native 0048b5e0; EAX word 00000024; RET 0. */
uint32_t SC bfv_server_return_word_0048b5e0(void)
{
    static const volatile uint32_t result = 0x00000024u;
    return result;
}

/* Native 0048b650; EAX word 00000039; RET 0. */
uint32_t SC bfv_server_return_word_0048b650(void)
{
    static const volatile uint32_t result = 0x00000039u;
    return result;
}

/* Native 0048b6c0; EAX word 00000038; RET 0. */
uint32_t SC bfv_server_return_word_0048b6c0(void)
{
    static const volatile uint32_t result = 0x00000038u;
    return result;
}

/* Native 0048b8f0; EAX word 0000002a; RET 0. */
uint32_t SC bfv_server_return_word_0048b8f0(void)
{
    static const volatile uint32_t result = 0x0000002au;
    return result;
}

/* Native 0048b900; EAX word 00000034; RET 0. */
uint32_t SC bfv_server_return_word_0048b900(void)
{
    static const volatile uint32_t result = 0x00000034u;
    return result;
}

/* Native 0048b930; EAX word 0000003a; RET 0. */
uint32_t SC bfv_server_return_word_0048b930(void)
{
    static const volatile uint32_t result = 0x0000003au;
    return result;
}

/* Native 00498900; EAX word 008ed124; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00498900(void)
{
    static const volatile uint32_t result = 0x008ed124u;
    return result;
}

/* Native 00498970; EAX word 008ea918; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00498970(void)
{
    static const volatile uint32_t result = 0x008ea918u;
    return result;
}

/* Native 004989e0; EAX word 008ebe38; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_004989e0(void)
{
    static const volatile uint32_t result = 0x008ebe38u;
    return result;
}

/* Native 00498c30; EAX word 008ed2f8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00498c30(void)
{
    static const volatile uint32_t result = 0x008ed2f8u;
    return result;
}

/* Native 00498ca0; EAX word 008ed35c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00498ca0(void)
{
    static const volatile uint32_t result = 0x008ed35cu;
    return result;
}

/* Native 00498d10; EAX word 008ed3c0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00498d10(void)
{
    static const volatile uint32_t result = 0x008ed3c0u;
    return result;
}

/* Native 00498d80; EAX word 008ed40c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00498d80(void)
{
    static const volatile uint32_t result = 0x008ed40cu;
    return result;
}

/* Native 00498df0; EAX word 008ed454; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00498df0(void)
{
    static const volatile uint32_t result = 0x008ed454u;
    return result;
}

/* Native 00498e60; EAX word 008ed4b0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00498e60(void)
{
    static const volatile uint32_t result = 0x008ed4b0u;
    return result;
}

/* Native 00498ed0; EAX word 008ed514; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00498ed0(void)
{
    static const volatile uint32_t result = 0x008ed514u;
    return result;
}

/* Native 00498f40; EAX word 008ed57c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00498f40(void)
{
    static const volatile uint32_t result = 0x008ed57cu;
    return result;
}

/* Native 00498fb0; EAX word 008ed5f0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00498fb0(void)
{
    static const volatile uint32_t result = 0x008ed5f0u;
    return result;
}

/* Native 00499020; EAX word 008ed664; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00499020(void)
{
    static const volatile uint32_t result = 0x008ed664u;
    return result;
}

/* Native 00499090; EAX word 008ed6c8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00499090(void)
{
    static const volatile uint32_t result = 0x008ed6c8u;
    return result;
}

/* Native 00499100; EAX word 008ed72c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00499100(void)
{
    static const volatile uint32_t result = 0x008ed72cu;
    return result;
}

/* Native 00499170; EAX word 008ed798; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00499170(void)
{
    static const volatile uint32_t result = 0x008ed798u;
    return result;
}

/* Native 004991e0; EAX word 008ed7fc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_004991e0(void)
{
    static const volatile uint32_t result = 0x008ed7fcu;
    return result;
}

/* Native 00499250; EAX word 008ed860; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00499250(void)
{
    static const volatile uint32_t result = 0x008ed860u;
    return result;
}

/* Native 004992c0; EAX word 008ed8c8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_004992c0(void)
{
    static const volatile uint32_t result = 0x008ed8c8u;
    return result;
}

/* Native 00499330; EAX word 008ed950; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00499330(void)
{
    static const volatile uint32_t result = 0x008ed950u;
    return result;
}

/* Native 004993a0; EAX word 008ed9d0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_004993a0(void)
{
    static const volatile uint32_t result = 0x008ed9d0u;
    return result;
}

/* Native 00499410; EAX word 008eda34; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00499410(void)
{
    static const volatile uint32_t result = 0x008eda34u;
    return result;
}

/* Native 00499480; EAX word 008edaa4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00499480(void)
{
    static const volatile uint32_t result = 0x008edaa4u;
    return result;
}

/* Native 004994f0; EAX word 008edb14; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_004994f0(void)
{
    static const volatile uint32_t result = 0x008edb14u;
    return result;
}

/* Native 00499560; EAX word 008edb7c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00499560(void)
{
    static const volatile uint32_t result = 0x008edb7cu;
    return result;
}

/* Native 004995d0; EAX word 008edc28; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_004995d0(void)
{
    static const volatile uint32_t result = 0x008edc28u;
    return result;
}

/* Native 00499640; EAX word 008edcd4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00499640(void)
{
    static const volatile uint32_t result = 0x008edcd4u;
    return result;
}

/* Native 004996b0; EAX word 008edd40; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_004996b0(void)
{
    static const volatile uint32_t result = 0x008edd40u;
    return result;
}

/* Native 00499720; EAX word 008eddf0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00499720(void)
{
    static const volatile uint32_t result = 0x008eddf0u;
    return result;
}

/* Native 00499790; EAX word 008eded8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00499790(void)
{
    static const volatile uint32_t result = 0x008eded8u;
    return result;
}

/* Native 00499800; EAX word 008edfc8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00499800(void)
{
    static const volatile uint32_t result = 0x008edfc8u;
    return result;
}

/* Native 00499870; EAX word 008ee070; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00499870(void)
{
    static const volatile uint32_t result = 0x008ee070u;
    return result;
}

/* Native 004998e0; EAX word 008ee0d8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_004998e0(void)
{
    static const volatile uint32_t result = 0x008ee0d8u;
    return result;
}

/* Native 00499950; EAX word 008ee13c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00499950(void)
{
    static const volatile uint32_t result = 0x008ee13cu;
    return result;
}

/* Native 004999c0; EAX word 008ee1a0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_004999c0(void)
{
    static const volatile uint32_t result = 0x008ee1a0u;
    return result;
}

/* Native 00499a30; EAX word 008ee204; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00499a30(void)
{
    static const volatile uint32_t result = 0x008ee204u;
    return result;
}

/* Native 00499aa0; EAX word 008ee260; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00499aa0(void)
{
    static const volatile uint32_t result = 0x008ee260u;
    return result;
}

/* Native 00499b50; EAX word 0080752c; RET 0. Original data identifier: void. */
uint32_t SC bfv_server_return_word_00499b50(void)
{
    static const volatile uint32_t result = 0x0080752cu;
    return result;
}

/* Native 00499b60; EAX word 008ec42c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00499b60(void)
{
    static const volatile uint32_t result = 0x008ec42cu;
    return result;
}

/* Native 00499b70; EAX word 008ee2a0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00499b70(void)
{
    static const volatile uint32_t result = 0x008ee2a0u;
    return result;
}

/* Native 0049a5f0; EAX word 008f1344; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a5f0(void)
{
    static const volatile uint32_t result = 0x008f1344u;
    return result;
}

/* Native 0049a640; EAX word 008e6edc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a640(void)
{
    static const volatile uint32_t result = 0x008e6edcu;
    return result;
}

/* Native 0049a650; EAX word 008e7070; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a650(void)
{
    static const volatile uint32_t result = 0x008e7070u;
    return result;
}

/* Native 0049a660; EAX word 008ee2f8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a660(void)
{
    static const volatile uint32_t result = 0x008ee2f8u;
    return result;
}

/* Native 0049a670; EAX word 008ee320; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a670(void)
{
    static const volatile uint32_t result = 0x008ee320u;
    return result;
}

/* Native 0049a680; EAX word 008ee34c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a680(void)
{
    static const volatile uint32_t result = 0x008ee34cu;
    return result;
}

/* Native 0049a690; EAX word 008f1910; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a690(void)
{
    static const volatile uint32_t result = 0x008f1910u;
    return result;
}

/* Native 0049a6a0; EAX word 008eff9c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a6a0(void)
{
    static const volatile uint32_t result = 0x008eff9cu;
    return result;
}

/* Native 0049a6b0; EAX word 008f1988; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a6b0(void)
{
    static const volatile uint32_t result = 0x008f1988u;
    return result;
}

/* Native 0049a6c0; EAX word 008f007c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a6c0(void)
{
    static const volatile uint32_t result = 0x008f007cu;
    return result;
}

/* Native 0049a6d0; EAX word 008f1a08; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a6d0(void)
{
    static const volatile uint32_t result = 0x008f1a08u;
    return result;
}

/* Native 0049a6e0; EAX word 008f0164; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a6e0(void)
{
    static const volatile uint32_t result = 0x008f0164u;
    return result;
}

/* Native 0049a6f0; EAX word 008f1a7c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a6f0(void)
{
    static const volatile uint32_t result = 0x008f1a7cu;
    return result;
}

/* Native 0049a700; EAX word 008f0240; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a700(void)
{
    static const volatile uint32_t result = 0x008f0240u;
    return result;
}

/* Native 0049a710; EAX word 008f1aec; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a710(void)
{
    static const volatile uint32_t result = 0x008f1aecu;
    return result;
}

/* Native 0049a720; EAX word 008f0318; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a720(void)
{
    static const volatile uint32_t result = 0x008f0318u;
    return result;
}

/* Native 0049a730; EAX word 008f1b68; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a730(void)
{
    static const volatile uint32_t result = 0x008f1b68u;
    return result;
}

/* Native 0049a740; EAX word 008f0404; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a740(void)
{
    static const volatile uint32_t result = 0x008f0404u;
    return result;
}

/* Native 0049a750; EAX word 008f1be8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a750(void)
{
    static const volatile uint32_t result = 0x008f1be8u;
    return result;
}

/* Native 0049a760; EAX word 008f04ec; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a760(void)
{
    static const volatile uint32_t result = 0x008f04ecu;
    return result;
}

/* Native 0049a770; EAX word 008f1c64; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a770(void)
{
    static const volatile uint32_t result = 0x008f1c64u;
    return result;
}

/* Native 0049a780; EAX word 008f05c8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a780(void)
{
    static const volatile uint32_t result = 0x008f05c8u;
    return result;
}

/* Native 0049a790; EAX word 008f1cdc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a790(void)
{
    static const volatile uint32_t result = 0x008f1cdcu;
    return result;
}

/* Native 0049a7a0; EAX word 008f06a8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a7a0(void)
{
    static const volatile uint32_t result = 0x008f06a8u;
    return result;
}

/* Native 0049a7b0; EAX word 008f1d54; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a7b0(void)
{
    static const volatile uint32_t result = 0x008f1d54u;
    return result;
}

/* Native 0049a7c0; EAX word 008f0780; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a7c0(void)
{
    static const volatile uint32_t result = 0x008f0780u;
    return result;
}

/* Native 0049a7d0; EAX word 008f1dc8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a7d0(void)
{
    static const volatile uint32_t result = 0x008f1dc8u;
    return result;
}

/* Native 0049a7e0; EAX word 008f0854; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a7e0(void)
{
    static const volatile uint32_t result = 0x008f0854u;
    return result;
}

/* Native 0049a7f0; EAX word 008f1e40; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a7f0(void)
{
    static const volatile uint32_t result = 0x008f1e40u;
    return result;
}

/* Native 0049a800; EAX word 008f0934; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a800(void)
{
    static const volatile uint32_t result = 0x008f0934u;
    return result;
}

/* Native 0049a810; EAX word 008f1ec0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a810(void)
{
    static const volatile uint32_t result = 0x008f1ec0u;
    return result;
}

/* Native 0049a820; EAX word 008f0a1c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a820(void)
{
    static const volatile uint32_t result = 0x008f0a1cu;
    return result;
}

/* Native 0049a830; EAX word 008f1f38; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a830(void)
{
    static const volatile uint32_t result = 0x008f1f38u;
    return result;
}

/* Native 0049a840; EAX word 008f0afc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a840(void)
{
    static const volatile uint32_t result = 0x008f0afcu;
    return result;
}

/* Native 0049a850; EAX word 008f1fb0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a850(void)
{
    static const volatile uint32_t result = 0x008f1fb0u;
    return result;
}

/* Native 0049a860; EAX word 008f0bdc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a860(void)
{
    static const volatile uint32_t result = 0x008f0bdcu;
    return result;
}

/* Native 0049a870; EAX word 008f2038; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a870(void)
{
    static const volatile uint32_t result = 0x008f2038u;
    return result;
}

/* Native 0049a880; EAX word 008f0cd4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a880(void)
{
    static const volatile uint32_t result = 0x008f0cd4u;
    return result;
}

/* Native 0049a890; EAX word 008f20c8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a890(void)
{
    static const volatile uint32_t result = 0x008f20c8u;
    return result;
}

/* Native 0049a8a0; EAX word 008f0dd4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a8a0(void)
{
    static const volatile uint32_t result = 0x008f0dd4u;
    return result;
}

/* Native 0049a8b0; EAX word 008f214c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a8b0(void)
{
    static const volatile uint32_t result = 0x008f214cu;
    return result;
}

/* Native 0049a8c0; EAX word 008f0eb8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a8c0(void)
{
    static const volatile uint32_t result = 0x008f0eb8u;
    return result;
}

/* Native 0049a8d0; EAX word 008f21c4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a8d0(void)
{
    static const volatile uint32_t result = 0x008f21c4u;
    return result;
}

/* Native 0049a8e0; EAX word 008f0f98; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a8e0(void)
{
    static const volatile uint32_t result = 0x008f0f98u;
    return result;
}

/* Native 0049a8f0; EAX word 008f2240; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a8f0(void)
{
    static const volatile uint32_t result = 0x008f2240u;
    return result;
}

/* Native 0049a900; EAX word 008f1084; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a900(void)
{
    static const volatile uint32_t result = 0x008f1084u;
    return result;
}

/* Native 0049a910; EAX word 008f22c0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a910(void)
{
    static const volatile uint32_t result = 0x008f22c0u;
    return result;
}

/* Native 0049a920; EAX word 008f116c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a920(void)
{
    static const volatile uint32_t result = 0x008f116cu;
    return result;
}

/* Native 0049a930; EAX word 008f2338; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a930(void)
{
    static const volatile uint32_t result = 0x008f2338u;
    return result;
}

/* Native 0049a940; EAX word 008f1244; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049a940(void)
{
    static const volatile uint32_t result = 0x008f1244u;
    return result;
}

/* Native 0049abe0; EAX word 008ecc2c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049abe0(void)
{
    static const volatile uint32_t result = 0x008ecc2cu;
    return result;
}

/* Native 0049ac20; EAX word 008ea8f0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049ac20(void)
{
    static const volatile uint32_t result = 0x008ea8f0u;
    return result;
}

/* Native 0049acb0; EAX word 008ee774; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049acb0(void)
{
    static const volatile uint32_t result = 0x008ee774u;
    return result;
}

/* Native 0049acf0; EAX word 008ee848; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049acf0(void)
{
    static const volatile uint32_t result = 0x008ee848u;
    return result;
}

/* Native 0049ad30; EAX word 008ee920; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049ad30(void)
{
    static const volatile uint32_t result = 0x008ee920u;
    return result;
}

/* Native 0049ae90; EAX word 008eee5c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049ae90(void)
{
    static const volatile uint32_t result = 0x008eee5cu;
    return result;
}

/* Native 0049aed0; EAX word 008e6efc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049aed0(void)
{
    static const volatile uint32_t result = 0x008e6efcu;
    return result;
}

/* Native 0049af10; EAX word 008f1628; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049af10(void)
{
    static const volatile uint32_t result = 0x008f1628u;
    return result;
}

/* Native 0049af50; EAX word 008ee37c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049af50(void)
{
    static const volatile uint32_t result = 0x008ee37cu;
    return result;
}

/* Native 0049af90; EAX word 008ea940; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049af90(void)
{
    static const volatile uint32_t result = 0x008ea940u;
    return result;
}

/* Native 0049afd0; EAX word 008ee3d0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049afd0(void)
{
    static const volatile uint32_t result = 0x008ee3d0u;
    return result;
}

/* Native 0049b010; EAX word 008ee3a8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049b010(void)
{
    static const volatile uint32_t result = 0x008ee3a8u;
    return result;
}

/* Native 0049b050; EAX word 008ef188; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049b050(void)
{
    static const volatile uint32_t result = 0x008ef188u;
    return result;
}

/* Native 0049b090; EAX word 008ea964; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049b090(void)
{
    static const volatile uint32_t result = 0x008ea964u;
    return result;
}

/* Native 0049b0d0; EAX word 008ee3fc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049b0d0(void)
{
    static const volatile uint32_t result = 0x008ee3fcu;
    return result;
}

/* Native 0049b110; EAX word 008ee424; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049b110(void)
{
    static const volatile uint32_t result = 0x008ee424u;
    return result;
}

/* Native 0049b150; EAX word 008ef598; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049b150(void)
{
    static const volatile uint32_t result = 0x008ef598u;
    return result;
}

/* Native 0049b190; EAX word 008ef678; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049b190(void)
{
    static const volatile uint32_t result = 0x008ef678u;
    return result;
}

/* Native 0049b1d0; EAX word 008ef7d8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049b1d0(void)
{
    static const volatile uint32_t result = 0x008ef7d8u;
    return result;
}

/* Native 0049b210; EAX word 008ef8fc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049b210(void)
{
    static const volatile uint32_t result = 0x008ef8fcu;
    return result;
}

/* Native 0049b250; EAX word 008ef9e4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049b250(void)
{
    static const volatile uint32_t result = 0x008ef9e4u;
    return result;
}

/* Native 0049b290; EAX word 008efb40; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049b290(void)
{
    static const volatile uint32_t result = 0x008efb40u;
    return result;
}

/* Native 0049b2d0; EAX word 008efcd8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049b2d0(void)
{
    static const volatile uint32_t result = 0x008efcd8u;
    return result;
}

/* Native 0049b310; EAX word 008efe80; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049b310(void)
{
    static const volatile uint32_t result = 0x008efe80u;
    return result;
}

/* Native 0049b350; EAX word 008ee6a0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049b350(void)
{
    static const volatile uint32_t result = 0x008ee6a0u;
    return result;
}

/* Native 0049b390; EAX word 008eecb8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049b390(void)
{
    static const volatile uint32_t result = 0x008eecb8u;
    return result;
}

/* Native 0049b3d0; EAX word 008ee5c4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049b3d0(void)
{
    static const volatile uint32_t result = 0x008ee5c4u;
    return result;
}

/* Native 0049b410; EAX word 008ef27c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049b410(void)
{
    static const volatile uint32_t result = 0x008ef27cu;
    return result;
}

/* Native 0049b450; EAX word 008ef4bc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049b450(void)
{
    static const volatile uint32_t result = 0x008ef4bcu;
    return result;
}

/* Native 0049b4b0; EAX word 008ee4f8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049b4b0(void)
{
    static const volatile uint32_t result = 0x008ee4f8u;
    return result;
}

/* Native 0049b880; EAX word 008ed1e4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049b880(void)
{
    static const volatile uint32_t result = 0x008ed1e4u;
    return result;
}

/* Native 0049b890; EAX word 008f3ad4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049b890(void)
{
    static const volatile uint32_t result = 0x008f3ad4u;
    return result;
}

/* Native 0049b930; EAX word 008ed214; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049b930(void)
{
    static const volatile uint32_t result = 0x008ed214u;
    return result;
}

/* Native 0049b940; EAX word 008f3b2c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049b940(void)
{
    static const volatile uint32_t result = 0x008f3b2cu;
    return result;
}

/* Native 0049b9e0; EAX word 008ed244; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049b9e0(void)
{
    static const volatile uint32_t result = 0x008ed244u;
    return result;
}

/* Native 0049b9f0; EAX word 008f3b80; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049b9f0(void)
{
    static const volatile uint32_t result = 0x008f3b80u;
    return result;
}

/* Native 0049bac0; EAX word 008ed290; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049bac0(void)
{
    static const volatile uint32_t result = 0x008ed290u;
    return result;
}

/* Native 0049bad0; EAX word 008f3c14; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049bad0(void)
{
    static const volatile uint32_t result = 0x008f3c14u;
    return result;
}

/* Native 0049bb40; EAX word 008f3a48; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049bb40(void)
{
    static const volatile uint32_t result = 0x008f3a48u;
    return result;
}

/* Native 0049bc40; EAX word 008f39f8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049bc40(void)
{
    static const volatile uint32_t result = 0x008f39f8u;
    return result;
}

/* Native 0049cdf0; EAX word 008f3e20; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049cdf0(void)
{
    static const volatile uint32_t result = 0x008f3e20u;
    return result;
}

/* Native 0049cec0; EAX word 008f3f90; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0049cec0(void)
{
    static const volatile uint32_t result = 0x008f3f90u;
    return result;
}

/* Native 004bfbf0; EAX word 00000008; RET 0. */
uint32_t SC bfv_server_return_word_004bfbf0(void)
{
    static const volatile uint32_t result = 0x00000008u;
    return result;
}

/* Native 004bfcb0; EAX word 0000003c; RET 0. */
uint32_t SC bfv_server_return_word_004bfcb0(void)
{
    static const volatile uint32_t result = 0x0000003cu;
    return result;
}

/* Native 004bfcc0; EAX word 00000015; RET 0. */
uint32_t SC bfv_server_return_word_004bfcc0(void)
{
    static const volatile uint32_t result = 0x00000015u;
    return result;
}

/* Native 004bfd90; EAX word 00000027; RET 0. */
uint32_t SC bfv_server_return_word_004bfd90(void)
{
    static const volatile uint32_t result = 0x00000027u;
    return result;
}

/* Native 004bfdc0; EAX word 0000000e; RET 0. */
uint32_t SC bfv_server_return_word_004bfdc0(void)
{
    static const volatile uint32_t result = 0x0000000eu;
    return result;
}

/* Native 004bfe30; EAX word 0000000d; RET 0. */
uint32_t SC bfv_server_return_word_004bfe30(void)
{
    static const volatile uint32_t result = 0x0000000du;
    return result;
}

/* Native 004c0020; EAX word 00000017; RET 0. */
uint32_t SC bfv_server_return_word_004c0020(void)
{
    static const volatile uint32_t result = 0x00000017u;
    return result;
}

/* Native 004c0350; EAX word 00000013; RET 0. */
uint32_t SC bfv_server_return_word_004c0350(void)
{
    static const volatile uint32_t result = 0x00000013u;
    return result;
}

/* Native 004c0440; EAX word 00000014; RET 0. */
uint32_t SC bfv_server_return_word_004c0440(void)
{
    static const volatile uint32_t result = 0x00000014u;
    return result;
}

/* Native 004c0600; EAX word 0000001a; RET 0. */
uint32_t SC bfv_server_return_word_004c0600(void)
{
    static const volatile uint32_t result = 0x0000001au;
    return result;
}

/* Native 004c06b0; EAX word 0000001b; RET 0. */
uint32_t SC bfv_server_return_word_004c06b0(void)
{
    static const volatile uint32_t result = 0x0000001bu;
    return result;
}

/* Native 004c0730; EAX word 00000028; RET 0. */
uint32_t SC bfv_server_return_word_004c0730(void)
{
    static const volatile uint32_t result = 0x00000028u;
    return result;
}

/* Native 004c07f0; EAX word 0000001d; RET 0. */
uint32_t SC bfv_server_return_word_004c07f0(void)
{
    static const volatile uint32_t result = 0x0000001du;
    return result;
}

/* Native 004c0860; EAX word 0000002b; RET 0. */
uint32_t SC bfv_server_return_word_004c0860(void)
{
    static const volatile uint32_t result = 0x0000002bu;
    return result;
}

/* Native 004c0b00; EAX word 00000006; RET 0. */
uint32_t SC bfv_server_return_word_004c0b00(void)
{
    static const volatile uint32_t result = 0x00000006u;
    return result;
}

/* Native 004c0ba0; EAX word 00000007; RET 0. */
uint32_t SC bfv_server_return_word_004c0ba0(void)
{
    static const volatile uint32_t result = 0x00000007u;
    return result;
}

/* Native 004c0c20; EAX word 00000005; RET 0. */
uint32_t SC bfv_server_return_word_004c0c20(void)
{
    static const volatile uint32_t result = 0x00000005u;
    return result;
}

/* Native 004c0e20; EAX word 0000000b; RET 0. */
uint32_t SC bfv_server_return_word_004c0e20(void)
{
    static const volatile uint32_t result = 0x0000000bu;
    return result;
}

/* Native 004c0f40; EAX word 0000001f; RET 0. */
uint32_t SC bfv_server_return_word_004c0f40(void)
{
    static const volatile uint32_t result = 0x0000001fu;
    return result;
}

/* Native 004c0fd0; EAX word 0000000c; RET 0. */
uint32_t SC bfv_server_return_word_004c0fd0(void)
{
    static const volatile uint32_t result = 0x0000000cu;
    return result;
}

/* Native 004c10e0; EAX word 0000000f; RET 0. */
uint32_t SC bfv_server_return_word_004c10e0(void)
{
    static const volatile uint32_t result = 0x0000000fu;
    return result;
}

/* Native 004c1170; EAX word 00000011; RET 0. */
uint32_t SC bfv_server_return_word_004c1170(void)
{
    static const volatile uint32_t result = 0x00000011u;
    return result;
}

/* Native 004c1200; EAX word 0000001e; RET 0. */
uint32_t SC bfv_server_return_word_004c1200(void)
{
    static const volatile uint32_t result = 0x0000001eu;
    return result;
}

/* Native 004c1290; EAX word 00000023; RET 0. */
uint32_t SC bfv_server_return_word_004c1290(void)
{
    static const volatile uint32_t result = 0x00000023u;
    return result;
}

/* Native 004c13c0; EAX word 00000025; RET 0. */
uint32_t SC bfv_server_return_word_004c13c0(void)
{
    static const volatile uint32_t result = 0x00000025u;
    return result;
}

/* Native 004c14c0; EAX word 0000002f; RET 0. */
uint32_t SC bfv_server_return_word_004c14c0(void)
{
    static const volatile uint32_t result = 0x0000002fu;
    return result;
}

/* Native 004c1550; EAX word 00000030; RET 0. */
uint32_t SC bfv_server_return_word_004c1550(void)
{
    static const volatile uint32_t result = 0x00000030u;
    return result;
}

/* Native 004c15d0; EAX word 00000031; RET 0. */
uint32_t SC bfv_server_return_word_004c15d0(void)
{
    static const volatile uint32_t result = 0x00000031u;
    return result;
}

/* Native 004c1650; EAX word 00000032; RET 0. */
uint32_t SC bfv_server_return_word_004c1650(void)
{
    static const volatile uint32_t result = 0x00000032u;
    return result;
}

/* Native 004c16e0; EAX word 00000033; RET 0. */
uint32_t SC bfv_server_return_word_004c16e0(void)
{
    static const volatile uint32_t result = 0x00000033u;
    return result;
}

/* Native 004c17f0; EAX word 00000035; RET 0. */
uint32_t SC bfv_server_return_word_004c17f0(void)
{
    static const volatile uint32_t result = 0x00000035u;
    return result;
}

/* Native 004c1890; EAX word 00000036; RET 0. */
uint32_t SC bfv_server_return_word_004c1890(void)
{
    static const volatile uint32_t result = 0x00000036u;
    return result;
}

/* Native 004c1a20; EAX word 00000037; RET 0. */
uint32_t SC bfv_server_return_word_004c1a20(void)
{
    static const volatile uint32_t result = 0x00000037u;
    return result;
}

/* Native 004c1ab0; EAX word 0000003b; RET 0. */
uint32_t SC bfv_server_return_word_004c1ab0(void)
{
    static const volatile uint32_t result = 0x0000003bu;
    return result;
}

/* Native 004c1bc0; EAX word 0000003d; RET 0. */
uint32_t SC bfv_server_return_word_004c1bc0(void)
{
    static const volatile uint32_t result = 0x0000003du;
    return result;
}

/* Native 004c1cd0; EAX word 00000012; RET 0. */
uint32_t SC bfv_server_return_word_004c1cd0(void)
{
    static const volatile uint32_t result = 0x00000012u;
    return result;
}

/* Native 004c1e00; EAX word 0000003e; RET 0. */
uint32_t SC bfv_server_return_word_004c1e00(void)
{
    static const volatile uint32_t result = 0x0000003eu;
    return result;
}

/* Native 004c20e0; EAX word 00000018; RET 0. */
uint32_t SC bfv_server_return_word_004c20e0(void)
{
    static const volatile uint32_t result = 0x00000018u;
    return result;
}

/* Native 004c21f0; EAX word 00000026; RET 0. */
uint32_t SC bfv_server_return_word_004c21f0(void)
{
    static const volatile uint32_t result = 0x00000026u;
    return result;
}

/* Native 004c24f0; EAX word 0000001c; RET 0. */
uint32_t SC bfv_server_return_word_004c24f0(void)
{
    static const volatile uint32_t result = 0x0000001cu;
    return result;
}

/* Native 0057ec20; EAX word 00907024; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0057ec20(void)
{
    static const volatile uint32_t result = 0x00907024u;
    return result;
}

/* Native 0057ec90; EAX word 00907098; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0057ec90(void)
{
    static const volatile uint32_t result = 0x00907098u;
    return result;
}

/* Native 0057ed00; EAX word 00907104; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0057ed00(void)
{
    static const volatile uint32_t result = 0x00907104u;
    return result;
}

/* Native 0057ee30; EAX word 009071b4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0057ee30(void)
{
    static const volatile uint32_t result = 0x009071b4u;
    return result;
}

/* Native 0057ee70; EAX word 008e6fcc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0057ee70(void)
{
    static const volatile uint32_t result = 0x008e6fccu;
    return result;
}

/* Native 0057eeb0; EAX word 008eaa34; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0057eeb0(void)
{
    static const volatile uint32_t result = 0x008eaa34u;
    return result;
}

/* Native 0057ef20; EAX word 00907158; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0057ef20(void)
{
    static const volatile uint32_t result = 0x00907158u;
    return result;
}

/* Native 0057ef30; EAX word 009072f8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0057ef30(void)
{
    static const volatile uint32_t result = 0x009072f8u;
    return result;
}

/* Native 0057f050; EAX word 00907294; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0057f050(void)
{
    static const volatile uint32_t result = 0x00907294u;
    return result;
}

/* Native 0057f060; EAX word 009073d8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0057f060(void)
{
    static const volatile uint32_t result = 0x009073d8u;
    return result;
}

/* Native 0058a710; EAX word 009094d0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0058a710(void)
{
    static const volatile uint32_t result = 0x009094d0u;
    return result;
}

/* Native 0058a780; EAX word 00909578; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0058a780(void)
{
    static const volatile uint32_t result = 0x00909578u;
    return result;
}

/* Native 0058a7f0; EAX word 0090960c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0058a7f0(void)
{
    static const volatile uint32_t result = 0x0090960cu;
    return result;
}

/* Native 0058a860; EAX word 00909668; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0058a860(void)
{
    static const volatile uint32_t result = 0x00909668u;
    return result;
}

/* Native 0058a8d0; EAX word 009096c4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0058a8d0(void)
{
    static const volatile uint32_t result = 0x009096c4u;
    return result;
}

/* Native 0058a940; EAX word 0090973c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0058a940(void)
{
    static const volatile uint32_t result = 0x0090973cu;
    return result;
}

/* Native 0058a9b0; EAX word 009097bc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0058a9b0(void)
{
    static const volatile uint32_t result = 0x009097bcu;
    return result;
}

/* Native 0058aa20; EAX word 00909848; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0058aa20(void)
{
    static const volatile uint32_t result = 0x00909848u;
    return result;
}

/* Native 0058aa90; EAX word 009098cc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0058aa90(void)
{
    static const volatile uint32_t result = 0x009098ccu;
    return result;
}

/* Native 0058ab00; EAX word 00909950; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0058ab00(void)
{
    static const volatile uint32_t result = 0x00909950u;
    return result;
}

/* Native 0058ab70; EAX word 009099d8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0058ab70(void)
{
    static const volatile uint32_t result = 0x009099d8u;
    return result;
}

/* Native 0058abe0; EAX word 00909a54; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0058abe0(void)
{
    static const volatile uint32_t result = 0x00909a54u;
    return result;
}

/* Native 0058ac50; EAX word 00909ad8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0058ac50(void)
{
    static const volatile uint32_t result = 0x00909ad8u;
    return result;
}

/* Native 0058acc0; EAX word 00909b58; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0058acc0(void)
{
    static const volatile uint32_t result = 0x00909b58u;
    return result;
}

/* Native 0058af90; EAX word 008f8d58; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0058af90(void)
{
    static const volatile uint32_t result = 0x008f8d58u;
    return result;
}

/* Native 0058afa0; EAX word 0090a5b4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0058afa0(void)
{
    static const volatile uint32_t result = 0x0090a5b4u;
    return result;
}

/* Native 0058afb0; EAX word 008fbe08; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0058afb0(void)
{
    static const volatile uint32_t result = 0x008fbe08u;
    return result;
}

/* Native 0058afc0; EAX word 00908c4c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0058afc0(void)
{
    static const volatile uint32_t result = 0x00908c4cu;
    return result;
}

/* Native 0058afd0; EAX word 00909010; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0058afd0(void)
{
    static const volatile uint32_t result = 0x00909010u;
    return result;
}

/* Native 0058afe0; EAX word 00909048; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0058afe0(void)
{
    static const volatile uint32_t result = 0x00909048u;
    return result;
}

/* Native 0058aff0; EAX word 0090a7ac; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0058aff0(void)
{
    static const volatile uint32_t result = 0x0090a7acu;
    return result;
}

/* Native 0058b000; EAX word 008fbe38; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0058b000(void)
{
    static const volatile uint32_t result = 0x008fbe38u;
    return result;
}

/* Native 0058b010; EAX word 00909108; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0058b010(void)
{
    static const volatile uint32_t result = 0x00909108u;
    return result;
}

/* Native 0058b020; EAX word 009091c8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0058b020(void)
{
    static const volatile uint32_t result = 0x009091c8u;
    return result;
}

/* Native 0058b030; EAX word 0090a8e8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0058b030(void)
{
    static const volatile uint32_t result = 0x0090a8e8u;
    return result;
}

/* Native 0058b070; EAX word 0090a988; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0058b070(void)
{
    static const volatile uint32_t result = 0x0090a988u;
    return result;
}

/* Native 0058b0b0; EAX word 00908f40; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0058b0b0(void)
{
    static const volatile uint32_t result = 0x00908f40u;
    return result;
}

/* Native 0058b0f0; EAX word 00908f78; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0058b0f0(void)
{
    static const volatile uint32_t result = 0x00908f78u;
    return result;
}

/* Native 0058b130; EAX word 008f8d28; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0058b130(void)
{
    static const volatile uint32_t result = 0x008f8d28u;
    return result;
}

/* Native 0058b4b0; EAX word 0090b05c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0058b4b0(void)
{
    static const volatile uint32_t result = 0x0090b05cu;
    return result;
}

/* Native 00596af0; EAX word 0090f414; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00596af0(void)
{
    static const volatile uint32_t result = 0x0090f414u;
    return result;
}

/* Native 00596bd0; EAX word 0090f484; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00596bd0(void)
{
    static const volatile uint32_t result = 0x0090f484u;
    return result;
}

/* Native 00596c80; EAX word 0090f4f4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00596c80(void)
{
    static const volatile uint32_t result = 0x0090f4f4u;
    return result;
}

/* Native 00596cf0; EAX word 0090f574; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00596cf0(void)
{
    static const volatile uint32_t result = 0x0090f574u;
    return result;
}

/* Native 00596d60; EAX word 0090f5fc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00596d60(void)
{
    static const volatile uint32_t result = 0x0090f5fcu;
    return result;
}

/* Native 00596dd0; EAX word 0090f678; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00596dd0(void)
{
    static const volatile uint32_t result = 0x0090f678u;
    return result;
}

/* Native 00596e40; EAX word 0090f6f8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00596e40(void)
{
    static const volatile uint32_t result = 0x0090f6f8u;
    return result;
}

/* Native 00596eb0; EAX word 0090f77c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00596eb0(void)
{
    static const volatile uint32_t result = 0x0090f77cu;
    return result;
}

/* Native 00597010; EAX word 008fbb90; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00597010(void)
{
    static const volatile uint32_t result = 0x008fbb90u;
    return result;
}

/* Native 00597020; EAX word 0090f1d8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00597020(void)
{
    static const volatile uint32_t result = 0x0090f1d8u;
    return result;
}

/* Native 00597030; EAX word 00908bb8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00597030(void)
{
    static const volatile uint32_t result = 0x00908bb8u;
    return result;
}

/* Native 00597040; EAX word 00908cc8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00597040(void)
{
    static const volatile uint32_t result = 0x00908cc8u;
    return result;
}

/* Native 00597050; EAX word 00908c88; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00597050(void)
{
    static const volatile uint32_t result = 0x00908c88u;
    return result;
}

/* Native 00597480; EAX word 009103f8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00597480(void)
{
    static const volatile uint32_t result = 0x009103f8u;
    return result;
}

/* Native 00597560; EAX word 009104ac; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00597560(void)
{
    static const volatile uint32_t result = 0x009104acu;
    return result;
}

/* Native 00597600; EAX word 00910554; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00597600(void)
{
    static const volatile uint32_t result = 0x00910554u;
    return result;
}

/* Native 005979d0; EAX word 00910338; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005979d0(void)
{
    static const volatile uint32_t result = 0x00910338u;
    return result;
}

/* Native 00597ab0; EAX word 009106a4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00597ab0(void)
{
    static const volatile uint32_t result = 0x009106a4u;
    return result;
}

/* Native 0059ecf0; EAX word 0091366c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0059ecf0(void)
{
    static const volatile uint32_t result = 0x0091366cu;
    return result;
}

/* Native 0059ed00; EAX word 009137dc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0059ed00(void)
{
    static const volatile uint32_t result = 0x009137dcu;
    return result;
}

/* Native 0059ed10; EAX word 009136ec; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0059ed10(void)
{
    static const volatile uint32_t result = 0x009136ecu;
    return result;
}

/* Native 0059ed20; EAX word 00913858; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0059ed20(void)
{
    static const volatile uint32_t result = 0x00913858u;
    return result;
}

/* Native 0059ed30; EAX word 0091376c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0059ed30(void)
{
    static const volatile uint32_t result = 0x0091376cu;
    return result;
}

/* Native 0059ed40; EAX word 0090598c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0059ed40(void)
{
    static const volatile uint32_t result = 0x0090598cu;
    return result;
}

/* Native 0059ed80; EAX word 00913498; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0059ed80(void)
{
    static const volatile uint32_t result = 0x00913498u;
    return result;
}

/* Native 0059edc0; EAX word 00913560; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0059edc0(void)
{
    static const volatile uint32_t result = 0x00913560u;
    return result;
}

/* Native 0059ee00; EAX word 00913500; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0059ee00(void)
{
    static const volatile uint32_t result = 0x00913500u;
    return result;
}

/* Native 0059ee40; EAX word 009135c0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0059ee40(void)
{
    static const volatile uint32_t result = 0x009135c0u;
    return result;
}

/* Native 005a3330; EAX word 00000493; RET 0. */
uint32_t SC bfv_server_return_word_005a3330(void)
{
    static const volatile uint32_t result = 0x00000493u;
    return result;
}

/* Native 005a59a0; EAX word 0000003f; RET 0. */
uint32_t SC bfv_server_return_word_005a59a0(void)
{
    static const volatile uint32_t result = 0x0000003fu;
    return result;
}

/* Native 005a7840; EAX word 000019fd; RET 0. */
uint32_t SC bfv_server_return_word_005a7840(void)
{
    static const volatile uint32_t result = 0x000019fdu;
    return result;
}

/* Native 005b8c00; EAX word 00915b70; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005b8c00(void)
{
    static const volatile uint32_t result = 0x00915b70u;
    return result;
}

/* Native 005b8c70; EAX word 00915be8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005b8c70(void)
{
    static const volatile uint32_t result = 0x00915be8u;
    return result;
}

/* Native 005b8ce0; EAX word 00915c60; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005b8ce0(void)
{
    static const volatile uint32_t result = 0x00915c60u;
    return result;
}

/* Native 005b8d60; EAX word 00915ce8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005b8d60(void)
{
    static const volatile uint32_t result = 0x00915ce8u;
    return result;
}

/* Native 005b8dd0; EAX word 00915d70; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005b8dd0(void)
{
    static const volatile uint32_t result = 0x00915d70u;
    return result;
}

/* Native 005b8e40; EAX word 00915df0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005b8e40(void)
{
    static const volatile uint32_t result = 0x00915df0u;
    return result;
}

/* Native 005b8ff0; EAX word 009056c8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005b8ff0(void)
{
    static const volatile uint32_t result = 0x009056c8u;
    return result;
}

/* Native 005b9030; EAX word 008fa80c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005b9030(void)
{
    static const volatile uint32_t result = 0x008fa80cu;
    return result;
}

/* Native 005b9070; EAX word 00905928; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005b9070(void)
{
    static const volatile uint32_t result = 0x00905928u;
    return result;
}

/* Native 005b90b0; EAX word 009059bc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005b90b0(void)
{
    static const volatile uint32_t result = 0x009059bcu;
    return result;
}

/* Native 005b90f0; EAX word 008fa9cc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005b90f0(void)
{
    static const volatile uint32_t result = 0x008fa9ccu;
    return result;
}

/* Native 005b9130; EAX word 008eaa64; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005b9130(void)
{
    static const volatile uint32_t result = 0x008eaa64u;
    return result;
}

/* Native 005b9260; EAX word 00916620; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005b9260(void)
{
    static const volatile uint32_t result = 0x00916620u;
    return result;
}

/* Native 005b9270; EAX word 00916710; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005b9270(void)
{
    static const volatile uint32_t result = 0x00916710u;
    return result;
}

/* Native 005bd9b0; EAX word 0091813c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005bd9b0(void)
{
    static const volatile uint32_t result = 0x0091813cu;
    return result;
}

/* Native 005bda20; EAX word 009181c0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005bda20(void)
{
    static const volatile uint32_t result = 0x009181c0u;
    return result;
}

/* Native 005bda90; EAX word 00918244; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005bda90(void)
{
    static const volatile uint32_t result = 0x00918244u;
    return result;
}

/* Native 005bdb00; EAX word 009182d4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005bdb00(void)
{
    static const volatile uint32_t result = 0x009182d4u;
    return result;
}

/* Native 005bdb70; EAX word 0091834c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005bdb70(void)
{
    static const volatile uint32_t result = 0x0091834cu;
    return result;
}

/* Native 005bdbe0; EAX word 009183b8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005bdbe0(void)
{
    static const volatile uint32_t result = 0x009183b8u;
    return result;
}

/* Native 005bdc50; EAX word 0091842c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005bdc50(void)
{
    static const volatile uint32_t result = 0x0091842cu;
    return result;
}

/* Native 005bdcc0; EAX word 0091849c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005bdcc0(void)
{
    static const volatile uint32_t result = 0x0091849cu;
    return result;
}

/* Native 005bdd30; EAX word 00918510; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005bdd30(void)
{
    static const volatile uint32_t result = 0x00918510u;
    return result;
}

/* Native 005bdda0; EAX word 00918588; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005bdda0(void)
{
    static const volatile uint32_t result = 0x00918588u;
    return result;
}

/* Native 005bde10; EAX word 00918608; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005bde10(void)
{
    static const volatile uint32_t result = 0x00918608u;
    return result;
}

/* Native 005bde80; EAX word 0091867c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005bde80(void)
{
    static const volatile uint32_t result = 0x0091867cu;
    return result;
}

/* Native 005be120; EAX word 009186dc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005be120(void)
{
    static const volatile uint32_t result = 0x009186dcu;
    return result;
}

/* Native 005be130; EAX word 0091871c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005be130(void)
{
    static const volatile uint32_t result = 0x0091871cu;
    return result;
}

/* Native 005be170; EAX word 00918f1c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005be170(void)
{
    static const volatile uint32_t result = 0x00918f1cu;
    return result;
}

/* Native 005be1e0; EAX word 009186a8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005be1e0(void)
{
    static const volatile uint32_t result = 0x009186a8u;
    return result;
}

/* Native 005be220; EAX word 00918758; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005be220(void)
{
    static const volatile uint32_t result = 0x00918758u;
    return result;
}

/* Native 005be260; EAX word 008ec3e4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005be260(void)
{
    static const volatile uint32_t result = 0x008ec3e4u;
    return result;
}

/* Native 005be2a0; EAX word 00917b60; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005be2a0(void)
{
    static const volatile uint32_t result = 0x00917b60u;
    return result;
}

/* Native 005be2e0; EAX word 00917bc0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005be2e0(void)
{
    static const volatile uint32_t result = 0x00917bc0u;
    return result;
}

/* Native 005be320; EAX word 00917c20; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005be320(void)
{
    static const volatile uint32_t result = 0x00917c20u;
    return result;
}

/* Native 005be360; EAX word 00904c30; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005be360(void)
{
    static const volatile uint32_t result = 0x00904c30u;
    return result;
}

/* Native 005be3b0; EAX word 00917c80; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005be3b0(void)
{
    static const volatile uint32_t result = 0x00917c80u;
    return result;
}

/* Native 005be3f0; EAX word 00918d98; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005be3f0(void)
{
    static const volatile uint32_t result = 0x00918d98u;
    return result;
}

/* Native 005c73b0; EAX word 00400000; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_005c73b0(void)
{
    static const volatile uint32_t result = 0x00400000u;
    return result;
}

/* Native 00601c40; EAX word 00920344; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00601c40(void)
{
    static const volatile uint32_t result = 0x00920344u;
    return result;
}

/* Native 00601cb0; EAX word 009203b0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00601cb0(void)
{
    static const volatile uint32_t result = 0x009203b0u;
    return result;
}

/* Native 00601d20; EAX word 00920434; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00601d20(void)
{
    static const volatile uint32_t result = 0x00920434u;
    return result;
}

/* Native 00601d90; EAX word 009204a8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00601d90(void)
{
    static const volatile uint32_t result = 0x009204a8u;
    return result;
}

/* Native 00601e00; EAX word 0092051c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00601e00(void)
{
    static const volatile uint32_t result = 0x0092051cu;
    return result;
}

/* Native 00601e90; EAX word 0092028c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00601e90(void)
{
    static const volatile uint32_t result = 0x0092028cu;
    return result;
}

/* Native 00601f00; EAX word 00920764; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00601f00(void)
{
    static const volatile uint32_t result = 0x00920764u;
    return result;
}

/* Native 00601f40; EAX word 009207cc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00601f40(void)
{
    static const volatile uint32_t result = 0x009207ccu;
    return result;
}

/* Native 00601f80; EAX word 0091dec0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00601f80(void)
{
    static const volatile uint32_t result = 0x0091dec0u;
    return result;
}

/* Native 00601fc0; EAX word 0091ce98; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00601fc0(void)
{
    static const volatile uint32_t result = 0x0091ce98u;
    return result;
}

/* Native 00602000; EAX word 0091ffac; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00602000(void)
{
    static const volatile uint32_t result = 0x0091ffacu;
    return result;
}

/* Native 00602070; EAX word 009209b4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00602070(void)
{
    static const volatile uint32_t result = 0x009209b4u;
    return result;
}

/* Native 00603f50; EAX word 00921ef8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00603f50(void)
{
    static const volatile uint32_t result = 0x00921ef8u;
    return result;
}

/* Native 00603fc0; EAX word 00921f70; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00603fc0(void)
{
    static const volatile uint32_t result = 0x00921f70u;
    return result;
}

/* Native 00604030; EAX word 00921fe4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00604030(void)
{
    static const volatile uint32_t result = 0x00921fe4u;
    return result;
}

/* Native 006040a0; EAX word 0092205c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_006040a0(void)
{
    static const volatile uint32_t result = 0x0092205cu;
    return result;
}

/* Native 00604110; EAX word 009220d8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00604110(void)
{
    static const volatile uint32_t result = 0x009220d8u;
    return result;
}

/* Native 00604260; EAX word 009223dc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00604260(void)
{
    static const volatile uint32_t result = 0x009223dcu;
    return result;
}

/* Native 00604270; EAX word 0091fa70; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00604270(void)
{
    static const volatile uint32_t result = 0x0091fa70u;
    return result;
}

/* Native 006042b0; EAX word 0091f9e4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_006042b0(void)
{
    static const volatile uint32_t result = 0x0091f9e4u;
    return result;
}

/* Native 006042f0; EAX word 0091f9b8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_006042f0(void)
{
    static const volatile uint32_t result = 0x0091f9b8u;
    return result;
}

/* Native 00604330; EAX word 00922108; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00604330(void)
{
    static const volatile uint32_t result = 0x00922108u;
    return result;
}

/* Native 0060d820; EAX word 00923a08; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0060d820(void)
{
    static const volatile uint32_t result = 0x00923a08u;
    return result;
}

/* Native 0060d950; EAX word 00924108; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0060d950(void)
{
    static const volatile uint32_t result = 0x00924108u;
    return result;
}

/* Native 0060d960; EAX word 0091e33c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0060d960(void)
{
    static const volatile uint32_t result = 0x0091e33cu;
    return result;
}

/* Native 0060d980; EAX word 00924198; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0060d980(void)
{
    static const volatile uint32_t result = 0x00924198u;
    return result;
}

/* Native 0060d990; EAX word 00920188; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0060d990(void)
{
    static const volatile uint32_t result = 0x00920188u;
    return result;
}

/* Native 0060d9a0; EAX word 0091e2b0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0060d9a0(void)
{
    static const volatile uint32_t result = 0x0091e2b0u;
    return result;
}

/* Native 006100b0; EAX word 009248f0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_006100b0(void)
{
    static const volatile uint32_t result = 0x009248f0u;
    return result;
}

/* Native 00610160; EAX word 00924840; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00610160(void)
{
    static const volatile uint32_t result = 0x00924840u;
    return result;
}

/* Native 006239c0; EAX word 00925334; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_006239c0(void)
{
    static const volatile uint32_t result = 0x00925334u;
    return result;
}

/* Native 00623a30; EAX word 00925368; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00623a30(void)
{
    static const volatile uint32_t result = 0x00925368u;
    return result;
}

/* Native 00623ae0; EAX word 00925398; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00623ae0(void)
{
    static const volatile uint32_t result = 0x00925398u;
    return result;
}

/* Native 00623b60; EAX word 009253c8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00623b60(void)
{
    static const volatile uint32_t result = 0x009253c8u;
    return result;
}

/* Native 00623bd0; EAX word 009253f8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00623bd0(void)
{
    static const volatile uint32_t result = 0x009253f8u;
    return result;
}

/* Native 00623c80; EAX word 00925428; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00623c80(void)
{
    static const volatile uint32_t result = 0x00925428u;
    return result;
}

/* Native 00623d30; EAX word 00925458; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00623d30(void)
{
    static const volatile uint32_t result = 0x00925458u;
    return result;
}

/* Native 0062b480; EAX word 00925fc0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0062b480(void)
{
    static const volatile uint32_t result = 0x00925fc0u;
    return result;
}

/* Native 0062b490; EAX word 00925fa0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0062b490(void)
{
    static const volatile uint32_t result = 0x00925fa0u;
    return result;
}

/* Native 0062b540; EAX word 00926000; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0062b540(void)
{
    static const volatile uint32_t result = 0x00926000u;
    return result;
}

/* Native 0062b5b0; EAX word 0092605c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0062b5b0(void)
{
    static const volatile uint32_t result = 0x0092605cu;
    return result;
}

/* Native 0062d040; EAX word 0092611c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0062d040(void)
{
    static const volatile uint32_t result = 0x0092611cu;
    return result;
}

/* Native 0062d050; EAX word 00926140; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0062d050(void)
{
    static const volatile uint32_t result = 0x00926140u;
    return result;
}

/* Native 00653940; EAX word 009286ac; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00653940(void)
{
    static const volatile uint32_t result = 0x009286acu;
    return result;
}

/* Native 006539b0; EAX word 0092870c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_006539b0(void)
{
    static const volatile uint32_t result = 0x0092870cu;
    return result;
}

/* Native 00653a20; EAX word 0092876c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00653a20(void)
{
    static const volatile uint32_t result = 0x0092876cu;
    return result;
}

/* Native 00653a90; EAX word 009287c4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00653a90(void)
{
    static const volatile uint32_t result = 0x009287c4u;
    return result;
}

/* Native 00653b60; EAX word 00928884; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00653b60(void)
{
    static const volatile uint32_t result = 0x00928884u;
    return result;
}

/* Native 00653e30; EAX word 00928824; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00653e30(void)
{
    static const volatile uint32_t result = 0x00928824u;
    return result;
}

/* Native 00653e40; EAX word 009288ec; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00653e40(void)
{
    static const volatile uint32_t result = 0x009288ecu;
    return result;
}

/* Native 00653e50; EAX word 0092927c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00653e50(void)
{
    static const volatile uint32_t result = 0x0092927cu;
    return result;
}

/* Native 00653e90; EAX word 009292dc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00653e90(void)
{
    static const volatile uint32_t result = 0x009292dcu;
    return result;
}

/* Native 00653ed0; EAX word 00928f38; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00653ed0(void)
{
    static const volatile uint32_t result = 0x00928f38u;
    return result;
}

/* Native 00653f10; EAX word 00929370; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00653f10(void)
{
    static const volatile uint32_t result = 0x00929370u;
    return result;
}

/* Native 00653f50; EAX word 009290ac; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00653f50(void)
{
    static const volatile uint32_t result = 0x009290acu;
    return result;
}

/* Native 00653f90; EAX word 00929404; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00653f90(void)
{
    static const volatile uint32_t result = 0x00929404u;
    return result;
}

/* Native 00653fd0; EAX word 00929188; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00653fd0(void)
{
    static const volatile uint32_t result = 0x00929188u;
    return result;
}

/* Native 00654610; EAX word 0092825c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00654610(void)
{
    static const volatile uint32_t result = 0x0092825cu;
    return result;
}

/* Native 00654620; EAX word 00929cf8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00654620(void)
{
    static const volatile uint32_t result = 0x00929cf8u;
    return result;
}

/* Native 006546c0; EAX word 00928290; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_006546c0(void)
{
    static const volatile uint32_t result = 0x00928290u;
    return result;
}

/* Native 006546d0; EAX word 00929dc8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_006546d0(void)
{
    static const volatile uint32_t result = 0x00929dc8u;
    return result;
}

/* Native 00654990; EAX word 00928c8c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00654990(void)
{
    static const volatile uint32_t result = 0x00928c8cu;
    return result;
}

/* Native 006549a0; EAX word 00929f6c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_006549a0(void)
{
    static const volatile uint32_t result = 0x00929f6cu;
    return result;
}

/* Native 00654aa0; EAX word 00928c2c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00654aa0(void)
{
    static const volatile uint32_t result = 0x00928c2cu;
    return result;
}

/* Native 00654ab0; EAX word 0092a028; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00654ab0(void)
{
    static const volatile uint32_t result = 0x0092a028u;
    return result;
}

/* Native 00654b50; EAX word 00000004; RET 0. */
uint32_t SC bfv_server_return_word_00654b50(void)
{
    static const volatile uint32_t result = 0x00000004u;
    return result;
}

/* Native 00654d50; EAX word 0092895c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00654d50(void)
{
    static const volatile uint32_t result = 0x0092895cu;
    return result;
}

/* Native 00654df0; EAX word 0092a318; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00654df0(void)
{
    static const volatile uint32_t result = 0x0092a318u;
    return result;
}

/* Native 00654f20; EAX word 00000002; RET 0. */
uint32_t SC bfv_server_return_word_00654f20(void)
{
    static const volatile uint32_t result = 0x00000002u;
    return result;
}

/* Native 0065a890; EAX word 00928234; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0065a890(void)
{
    static const volatile uint32_t result = 0x00928234u;
    return result;
}

/* Native 0065a8a0; EAX word 0092af34; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0065a8a0(void)
{
    static const volatile uint32_t result = 0x0092af34u;
    return result;
}

/* Native 0065af80; EAX word 00929468; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0065af80(void)
{
    static const volatile uint32_t result = 0x00929468u;
    return result;
}

/* Native 0065bc60; EAX word 009296cc; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0065bc60(void)
{
    static const volatile uint32_t result = 0x009296ccu;
    return result;
}

/* Native 0065c470; EAX word 0092970c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0065c470(void)
{
    static const volatile uint32_t result = 0x0092970cu;
    return result;
}

/* Native 0065cbe0; EAX word 0092974c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0065cbe0(void)
{
    static const volatile uint32_t result = 0x0092974cu;
    return result;
}

/* Native 0065d420; EAX word 00929790; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0065d420(void)
{
    static const volatile uint32_t result = 0x00929790u;
    return result;
}

/* Native 0065db90; EAX word 009298e0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_0065db90(void)
{
    static const volatile uint32_t result = 0x009298e0u;
    return result;
}

/* Native 0068cd50; EAX word 000005c0; RET 0. */
uint32_t SC bfv_server_return_word_0068cd50(void)
{
    static const volatile uint32_t result = 0x000005c0u;
    return result;
}

/* Native 00707920; EAX word 00935718; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00707920(void)
{
    static const volatile uint32_t result = 0x00935718u;
    return result;
}

/* Native 00707990; EAX word 0093577c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00707990(void)
{
    static const volatile uint32_t result = 0x0093577cu;
    return result;
}

/* Native 00707a00; EAX word 009357f4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00707a00(void)
{
    static const volatile uint32_t result = 0x009357f4u;
    return result;
}

/* Native 00707a70; EAX word 00935870; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00707a70(void)
{
    static const volatile uint32_t result = 0x00935870u;
    return result;
}

/* Native 00707ae0; EAX word 009358ec; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00707ae0(void)
{
    static const volatile uint32_t result = 0x009358ecu;
    return result;
}

/* Native 00707b50; EAX word 00935964; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00707b50(void)
{
    static const volatile uint32_t result = 0x00935964u;
    return result;
}

/* Native 00707bc0; EAX word 009359c4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00707bc0(void)
{
    static const volatile uint32_t result = 0x009359c4u;
    return result;
}

/* Native 00707c30; EAX word 00935a24; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00707c30(void)
{
    static const volatile uint32_t result = 0x00935a24u;
    return result;
}

/* Native 00707ca0; EAX word 00935a88; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00707ca0(void)
{
    static const volatile uint32_t result = 0x00935a88u;
    return result;
}

/* Native 00707d10; EAX word 00935af0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00707d10(void)
{
    static const volatile uint32_t result = 0x00935af0u;
    return result;
}

/* Native 00707d80; EAX word 00935b64; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00707d80(void)
{
    static const volatile uint32_t result = 0x00935b64u;
    return result;
}

/* Native 00707df0; EAX word 00935bd0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00707df0(void)
{
    static const volatile uint32_t result = 0x00935bd0u;
    return result;
}

/* Native 00707e60; EAX word 00935c38; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00707e60(void)
{
    static const volatile uint32_t result = 0x00935c38u;
    return result;
}

/* Native 00707ed0; EAX word 00935ca8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00707ed0(void)
{
    static const volatile uint32_t result = 0x00935ca8u;
    return result;
}

/* Native 00707f40; EAX word 00935d24; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00707f40(void)
{
    static const volatile uint32_t result = 0x00935d24u;
    return result;
}

/* Native 00707fb0; EAX word 00935d90; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00707fb0(void)
{
    static const volatile uint32_t result = 0x00935d90u;
    return result;
}

/* Native 00708020; EAX word 00935df4; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00708020(void)
{
    static const volatile uint32_t result = 0x00935df4u;
    return result;
}

/* Native 007086d0; EAX word 009384f8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_007086d0(void)
{
    static const volatile uint32_t result = 0x009384f8u;
    return result;
}

/* Native 00708710; EAX word 00934c30; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00708710(void)
{
    static const volatile uint32_t result = 0x00934c30u;
    return result;
}

/* Native 00708750; EAX word 00934bc8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00708750(void)
{
    static const volatile uint32_t result = 0x00934bc8u;
    return result;
}

/* Native 00708790; EAX word 00934b98; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00708790(void)
{
    static const volatile uint32_t result = 0x00934b98u;
    return result;
}

/* Native 007087d0; EAX word 00934bf8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_007087d0(void)
{
    static const volatile uint32_t result = 0x00934bf8u;
    return result;
}

/* Native 00708810; EAX word 00934b6c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00708810(void)
{
    static const volatile uint32_t result = 0x00934b6cu;
    return result;
}

/* Native 00708850; EAX word 0093444c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00708850(void)
{
    static const volatile uint32_t result = 0x0093444cu;
    return result;
}

/* Native 00708890; EAX word 00934564; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00708890(void)
{
    static const volatile uint32_t result = 0x00934564u;
    return result;
}

/* Native 007088d0; EAX word 0093453c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_007088d0(void)
{
    static const volatile uint32_t result = 0x0093453cu;
    return result;
}

/* Native 00708910; EAX word 0093458c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00708910(void)
{
    static const volatile uint32_t result = 0x0093458cu;
    return result;
}

/* Native 00708950; EAX word 00936f5c; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00708950(void)
{
    static const volatile uint32_t result = 0x00936f5cu;
    return result;
}

/* Native 00708990; EAX word 009345b8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00708990(void)
{
    static const volatile uint32_t result = 0x009345b8u;
    return result;
}

/* Native 007089d0; EAX word 009345e0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_007089d0(void)
{
    static const volatile uint32_t result = 0x009345e0u;
    return result;
}

/* Native 00708a10; EAX word 00937bd0; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00708a10(void)
{
    static const volatile uint32_t result = 0x00937bd0u;
    return result;
}

/* Native 00708a50; EAX word 009344e8; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00708a50(void)
{
    static const volatile uint32_t result = 0x009344e8u;
    return result;
}

/* Native 00708a90; EAX word 00934514; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00708a90(void)
{
    static const volatile uint32_t result = 0x00934514u;
    return result;
}

/* Native 00708ad0; EAX word 00934494; RET 0. Value falls within original image; data dependency retained. */
uint32_t SC bfv_server_return_word_00708ad0(void)
{
    static const volatile uint32_t result = 0x00934494u;
    return result;
}

/* Native 007210c0; EAX word 08000000; RET 0. */
uint32_t SC bfv_server_return_word_007210c0(void)
{
    static const volatile uint32_t result = 0x08000000u;
    return result;
}

/* Native 007416f0; EAX word 00000029; RET 0. */
uint32_t SC bfv_server_return_word_007416f0(void)
{
    static const volatile uint32_t result = 0x00000029u;
    return result;
}

/* Native 00774a30; EAX word 00000003; RET 0. */
uint32_t SC bfv_server_return_word_00774a30(void)
{
    static const volatile uint32_t result = 0x00000003u;
    return result;
}

/* Native 00774c20; EAX word 00000001; RET 0. */
uint32_t SC bfv_server_return_word_00774c20(void)
{
    static const volatile uint32_t result = 0x00000001u;
    return result;
}

/* Native 007a2e80; EAX word 0000000a; RET 0. */
uint32_t SC bfv_server_return_word_007a2e80(void)
{
    static const volatile uint32_t result = 0x0000000au;
    return result;
}

/* Native 007a4a30; EAX word 00000009; RET 0. */
uint32_t SC bfv_server_return_word_007a4a30(void)
{
    static const volatile uint32_t result = 0x00000009u;
    return result;
}

/* Native 007a6840; EAX word 00000010; RET 0. */
uint32_t SC bfv_server_return_word_007a6840(void)
{
    static const volatile uint32_t result = 0x00000010u;
    return result;
}

/* Native 007a69d0; EAX word 00000020; RET 0. */
uint32_t SC bfv_server_return_word_007a69d0(void)
{
    static const volatile uint32_t result = 0x00000020u;
    return result;
}

/* Native 007a6e20; EAX word 00000022; RET 0. */
uint32_t SC bfv_server_return_word_007a6e20(void)
{
    static const volatile uint32_t result = 0x00000022u;
    return result;
}

/* Native 007a80f0; EAX word 00000019; RET 0. */
uint32_t SC bfv_server_return_word_007a80f0(void)
{
    static const volatile uint32_t result = 0x00000019u;
    return result;
}

/* Native 007abe80; EAX word 00000021; RET 0. */
uint32_t SC bfv_server_return_word_007abe80(void)
{
    static const volatile uint32_t result = 0x00000021u;
    return result;
}

/* Native 007ae670; EAX word 00000016; RET 0. */
uint32_t SC bfv_server_return_word_007ae670(void)
{
    static const volatile uint32_t result = 0x00000016u;
    return result;
}

#endif
