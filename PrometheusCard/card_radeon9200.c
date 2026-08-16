#include <proto/exec.h>
#include <prometheus.h>
#include <proto/prometheus.h>
#include <inline/macros.h>
#include <proto/picasso96_chip.h>

#include "card.h"
#include "prometheus_radeon.h"

#define PCI_VENDOR_ATI       0x1002
#ifdef RADEON3D_DEBUG_CHIP
#define CHIP_NAME ((CONST_STRPTR)"picasso96/Radeon9200-debug.chip")
#else
#define CHIP_NAME ((CONST_STRPTR)"picasso96/Radeon9200.chip")
#endif
#define RADEON_BAR_FB        0
#define RADEON_BAR_MMIO      2
#define RADEON_MIN_FB_SIZE   0x00400000UL
#define RADEON_MIN_MMIO_SIZE 0x00010000UL

static void ClearBytes(APTR memory, ULONG size)
  {
    UBYTE *byte = memory;

    while (size--) *byte++ = 0;
  }

static BOOL SupportedDevice(ULONG device)
  {
    return device == 0x5960 || device == 0x5961 || device == 0x5964;
  }

static void ClearRadeonBoardInfo(struct BoardInfo *bi)
  {
    if (!bi)
      return;
    bi->ChipBase = NULL;
    bi->RegisterBase = NULL;
    bi->MemoryBase = NULL;
    bi->MemoryIOBase = NULL;
    bi->MemorySize = 0;
    bi->MemorySpaceBase = NULL;
    bi->MemorySpaceSize = 0;
    ClearBytes(bi->CardData, sizeof(struct PrometheusRadeonHandoff));
  }

BOOL InitRadeon9200(struct CardBase *cb, struct BoardInfo *bi)
  {
    struct Library *SysBase = cb->cb_SysBase;
    struct Library *PrometheusBase = cb->cb_PrometheusBase;
    PCIBoard *board = NULL;

    ClearRadeonBoardInfo(bi);

    while ((board = Prm_FindBoardTags(board, PRM_Vendor, PCI_VENDOR_ATI,
                                      TAG_END)) != NULL)
      {
        struct PrometheusRadeonHandoff handoff;
        struct ChipBase *ChipBase;
        ULONG device;
        APTR framebuffer;
        APTR mmio;

        ClearBytes(&handoff, sizeof(handoff));
        device = 0;
        framebuffer = NULL;
        mmio = NULL;

        Prm_GetBoardAttrsTags(board,
          PRM_Device, (ULONG)&device,
          PRM_MemoryAddr0 + RADEON_BAR_FB, (ULONG)&framebuffer,
          PRM_MemorySize0 + RADEON_BAR_FB, (ULONG)&handoff.FramebufferSize,
          PRM_MemoryAddr0 + RADEON_BAR_MMIO, (ULONG)&mmio,
          PRM_MemorySize0 + RADEON_BAR_MMIO, (ULONG)&handoff.MmioSize,
          PRM_ROM_Address, (ULONG)&handoff.RomBase,
          PRM_ROM_Size, (ULONG)&handoff.RomSize,
          TAG_END);

        handoff.DeviceId = (UWORD)device;
        if (!SupportedDevice(device) || !framebuffer || !mmio ||
            handoff.FramebufferSize < RADEON_MIN_FB_SIZE ||
            handoff.MmioSize < RADEON_MIN_MMIO_SIZE)
          continue;
        ChipBase = (struct ChipBase *)OpenLibrary(CHIP_NAME, 1);
        if (!ChipBase)
          continue;
        if (!Prm_SetBoardAttrsTags(board,
                                   PRM_BoardOwner, (ULONG)ChipBase,
                                   TAG_END))
          {
            CloseLibrary((struct Library *)ChipBase);
            continue;
          }

        handoff.Magic = PROM_RADEON_HANDOFF_MAGIC;
        handoff.Board = board;
        handoff.Reserved = 0;
        {
          UBYTE *source = (UBYTE *)&handoff;
          UBYTE *destination = (UBYTE *)bi->CardData;
          ULONG index;

          for (index = 0; index < sizeof(handoff); ++index)
            destination[index] = source[index];
        }

        bi->ChipBase = ChipBase;
        bi->RegisterBase = NULL;
        bi->MemoryBase = framebuffer;
        bi->MemoryIOBase = mmio;
        bi->MemorySize = handoff.FramebufferSize;
        bi->MemorySpaceBase = bi->MemoryBase;
        bi->MemorySpaceSize = handoff.FramebufferSize;
        if (!InitChip(bi))
          {
            (void)Prm_SetBoardAttrsTags(board,
                                        PRM_BoardOwner, 0,
                                        TAG_END);
            CloseLibrary((struct Library *)ChipBase);
            ClearRadeonBoardInfo(bi);
            continue;
          }

        return TRUE;
      }
    D(kprintf("prometheus.card: no usable Radeon 9200 found\n"));
    ClearRadeonBoardInfo(bi);
    return FALSE;
  }

void AbortRadeon9200(struct CardBase *cb, struct BoardInfo *bi)
  {
    struct ExecBase *SysBase = bi ? bi->ExecBase : NULL;
    struct Library *PrometheusBase = cb ? cb->cb_PrometheusBase : NULL;
    struct ChipBase *ChipBase = bi ? bi->ChipBase : NULL;
    struct PrometheusRadeonHandoff *handoff =
      bi ? (struct PrometheusRadeonHandoff *)bi->CardData : NULL;

    if (SysBase && ChipBase) {
      if (ChipBase->LibBase.lib_Version >= 2)
        (void)Radeon3DDetachOwner(bi);
      if (PrometheusBase && handoff &&
          handoff->Magic == PROM_RADEON_HANDOFF_MAGIC && handoff->Board)
        (void)Prm_SetBoardAttrsTags(handoff->Board,
                                    PRM_BoardOwner, 0,
                                    TAG_END);
      CloseLibrary((struct Library *)ChipBase);
    }
    ClearRadeonBoardInfo(bi);
  }

BOOL InitRadeon9200Features(struct BoardInfo *bi, ULONG features)
  {
    struct ChipBase *ChipBase = bi ? bi->ChipBase : NULL;

    if (!ChipBase)
      return FALSE;
    return LP2(0x24, BOOL, InitRadeonFeatures,
               struct BoardInfo *, bi, a0, ULONG, features, d0,
               , ChipBase);
  }
