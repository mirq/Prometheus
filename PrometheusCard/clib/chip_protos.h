#ifndef  CLIB_PICASSO96_CHIP_PROTOS_H
#define  CLIB_PICASSO96_CHIP_PROTOS_H

/*
**	$VER: Picasso96_chip_protos.h
**	
**
**	C prototypes. For use with 32 bit integers only.
*/

#ifndef boardinfo_H
#include "boardinfo.h"
#endif

BOOL	InitChip(struct BoardInfo *bi);
BOOL	Radeon3DDetachOwner(struct BoardInfo *bi);

#endif	 /* CLIB_PICASSO96_CHIP_PROTOS_H */
