#include <proto/exec.h>
#include <prometheus.h>
#include <proto/prometheus.h>
#include <inline/macros.h>
#include <proto/picasso96_chip.h>

#include "card.h"
#include "prometheus_geforce.h"

/*
 * NVIDIA GeForce 7200 GS / G72 family (PCI 10DE:01Dx) with
 * GeForce7200.chip. The card only finds and claims the board and hands the
 * library-assigned addresses over; the chip repairs the PCI setup (bridge
 * windows, 64-bit BAR upper halves, ROM BAR) and POSTs the GPU itself.
 */

#define PCI_VENDOR_NVIDIA    0x10DE
#define CHIP_NAME ((CONST_STRPTR)"picasso96/GeForce7200.chip")
#define GEFORCE_BAR_MMIO     0
#define GEFORCE_BAR_FB       1
#define GEFORCE_BAR_RAMIN    3
#define GEFORCE_MIN_FB_SIZE   0x00400000UL
#define GEFORCE_MIN_MMIO_SIZE 0x01000000UL

static void ClearBytes(APTR memory, ULONG size)
  {
    UBYTE *byte = memory;

    while (size--) *byte++ = 0;
  }

static BOOL SupportedDevice(ULONG device)
  {
    return (device & 0xfff0) == 0x01d0; /* G72 */
  }

static void ClearGeForceBoardInfo(struct BoardInfo *bi)
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
    ClearBytes(bi->CardData, sizeof(struct PrometheusGeForceHandoff));
  }

BOOL InitGeForce7200(struct CardBase *cb, struct BoardInfo *bi,
                     char **toolTypes)
  {
    struct Library *SysBase = cb->cb_SysBase;
    struct Library *PrometheusBase = cb->cb_PrometheusBase;
    PCIBoard *board = NULL;

    ClearGeForceBoardInfo(bi);

    while ((board = Prm_FindBoardTags(board, PRM_Vendor, PCI_VENDOR_NVIDIA,
                                      TAG_END)) != NULL)
      {
        struct PrometheusGeForceHandoff handoff;
        struct ChipBase *ChipBase;
        ULONG device = 0, cls = 0;

        ClearBytes(&handoff, sizeof(handoff));
        Prm_GetBoardAttrsTags(board,
          PRM_Device, (ULONG)&device,
          PRM_Class, (ULONG)&cls,
          PRM_MemoryAddr0 + GEFORCE_BAR_MMIO, (ULONG)&handoff.RegisterBase,
          PRM_MemorySize0 + GEFORCE_BAR_MMIO, (ULONG)&handoff.RegisterSize,
          PRM_MemoryAddr0 + GEFORCE_BAR_FB, (ULONG)&handoff.FramebufferBase,
          PRM_MemorySize0 + GEFORCE_BAR_FB, (ULONG)&handoff.FramebufferSize,
          PRM_MemoryAddr0 + GEFORCE_BAR_RAMIN, (ULONG)&handoff.RaminBase,
          PRM_MemorySize0 + GEFORCE_BAR_RAMIN, (ULONG)&handoff.RaminSize,
          TAG_END);

        if (cls != 3 || !SupportedDevice(device) || !handoff.RegisterBase ||
            !handoff.FramebufferBase ||
            handoff.RegisterSize < GEFORCE_MIN_MMIO_SIZE ||
            handoff.FramebufferSize < GEFORCE_MIN_FB_SIZE)
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

        handoff.Magic = PROM_GEFORCE_HANDOFF_MAGIC;
        handoff.Version = PROM_GEFORCE_HANDOFF_VERSION;
        handoff.DeviceId = (UWORD)device;
        handoff.Board = board;
        handoff.ToolTypes = toolTypes;
        {
          UBYTE *source = (UBYTE *)&handoff;
          UBYTE *destination = (UBYTE *)bi->CardData;
          ULONG index;

          for (index = 0; index < sizeof(handoff); ++index)
            destination[index] = source[index];
        }

        bi->ChipBase = ChipBase;
        bi->RegisterBase = NULL;
        bi->MemoryBase = handoff.FramebufferBase;
        bi->MemoryIOBase = handoff.RegisterBase;
        bi->MemorySize = handoff.FramebufferSize;
        bi->MemorySpaceBase = bi->MemoryBase;
        bi->MemorySpaceSize = handoff.FramebufferSize;
        if (!InitChip(bi))
          {
            (void)Prm_SetBoardAttrsTags(board, PRM_BoardOwner, 0, TAG_END);
            CloseLibrary((struct Library *)ChipBase);
            ClearGeForceBoardInfo(bi);
            continue;
          }

        return TRUE;
      }
    D(kprintf("prometheus.card: no usable GeForce 7200 found\n"));
    ClearGeForceBoardInfo(bi);
    return FALSE;
  }

void AbortGeForce7200(struct CardBase *cb, struct BoardInfo *bi)
  {
    struct ExecBase *SysBase = bi ? bi->ExecBase : NULL;
    struct Library *PrometheusBase = cb ? cb->cb_PrometheusBase : NULL;
    struct ChipBase *ChipBase = bi ? bi->ChipBase : NULL;
    struct PrometheusGeForceHandoff *handoff =
      bi ? (struct PrometheusGeForceHandoff *)bi->CardData : NULL;

    if (SysBase && ChipBase) {
      if (PrometheusBase && handoff &&
          handoff->Magic == PROM_GEFORCE_HANDOFF_MAGIC && handoff->Board)
        (void)Prm_SetBoardAttrsTags(handoff->Board, PRM_BoardOwner, 0,
                                    TAG_END);
      CloseLibrary((struct Library *)ChipBase);
    }
    ClearGeForceBoardInfo(bi);
  }

BOOL InitGeForce7200Features(struct BoardInfo *bi, ULONG features)
  {
    struct ChipBase *ChipBase = bi ? bi->ChipBase : NULL;

    if (!ChipBase)
      return FALSE;
    return LP2(0x24, BOOL, InitGeForceFeatures,
               struct BoardInfo *, bi, a0, ULONG, features, d0,
               , ChipBase);
  }
