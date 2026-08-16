#ifndef _INLINE_CHIP_H
#define _INLINE_CHIP_H

#ifndef __INLINE_MACROS_H
#include <inline/macros.h>
#endif

#ifndef CHIP_BASE_NAME
#define CHIP_BASE_NAME ChipBase
#endif

#define InitChip(bi) \
	LP1(0x1e, BOOL, InitChip, struct BoardInfo *, bi, a0, \
	, CHIP_BASE_NAME)

#define Radeon3DDetachOwner(bi) \
	LP1(0x3c, BOOL, Radeon3DDetachOwner, struct BoardInfo *, bi, a0, \
	, CHIP_BASE_NAME)

#endif /*  _INLINE_CHIP_H  */
