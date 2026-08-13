#include <exec/libraries.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <prometheus.h>
#include <proto/prometheus.h>

#include "boardinfo.h"
#include "card.h"

#define DMA_PAGE_SIZE         4096UL
#define DMA_PAGE_MASK         (DMA_PAGE_SIZE - 1UL)
#define DMA_PAGE_CONTINUATION 0xffffffffUL

#define PCI_VENDOR_ATI       0x1002
#define RADEON_MIN_FB_SIZE   (4UL * 1024UL * 1024UL)
#define RADEON_MAX_APERTURE  (64UL * 1024UL * 1024UL)
#define RADEON_CONFIG_MEMSIZE   0x00f8UL
#define RADEON_CONFIG_APER_SIZE 0x0108UL

static BOOL SupportedRadeon(ULONG device)
  {
    return device == 0x5960 || device == 0x5961 || device == 0x5964;
  }

static ULONG SwapLong(ULONG value)
  {
    return ((value & 0x000000ffUL) << 24) |
           ((value & 0x0000ff00UL) << 8) |
           ((value & 0x00ff0000UL) >> 8) |
           ((value & 0xff000000UL) >> 24);
  }

BOOL InitEarlyRadeonDMAMemory(struct CardBase *cb)
  {
    struct Library *PrometheusBase = cb->cb_PrometheusBase;
    PCIBoard *board = NULL;

    if (cb->cb_DMAArena || !PrometheusBase ||
        PrometheusBase->lib_Version < 2)
      return cb->cb_DMAArena != NULL;
    if (cb->cb_DMAEarlyAttempted)
      return FALSE;
    cb->cb_DMAEarlyAttempted = TRUE;

    while ((board = Prm_FindBoardTags(board, PRM_Vendor, PCI_VENDOR_ATI,
                                      TAG_END)) != NULL)
      {
        ULONG device = 0;
        ULONG framebufferSize = 0;
        ULONG memorySize;
        ULONG apertureSize;
        ULONG usableSize;
        APTR framebuffer = NULL;
        APTR mmio = NULL;
        ULONG mmioSize = 0;

        Prm_GetBoardAttrsTags(board,
          PRM_Device, (ULONG)&device,
          PRM_MemoryAddr0, (ULONG)&framebuffer,
          PRM_MemorySize0, (ULONG)&framebufferSize,
          PRM_MemoryAddr2, (ULONG)&mmio,
          PRM_MemorySize2, (ULONG)&mmioSize,
          TAG_END);
        if (!SupportedRadeon(device) || !framebuffer || !mmio ||
            framebufferSize < RADEON_MIN_FB_SIZE + EARLY_DMA_SIZE ||
            mmioSize < RADEON_CONFIG_APER_SIZE + sizeof(ULONG))
          continue;

        memorySize = SwapLong(*(volatile ULONG *)
          ((UBYTE *)mmio + RADEON_CONFIG_MEMSIZE));
        apertureSize = SwapLong(*(volatile ULONG *)
          ((UBYTE *)mmio + RADEON_CONFIG_APER_SIZE));
        usableSize = framebufferSize;
        if (usableSize > RADEON_MAX_APERTURE) usableSize = RADEON_MAX_APERTURE;
        if (memorySize >= RADEON_MIN_FB_SIZE + EARLY_DMA_SIZE &&
            (memorySize & 0x000fffffUL) == 0 && memorySize < usableSize)
          usableSize = memorySize;
        if (apertureSize >= RADEON_MIN_FB_SIZE + EARLY_DMA_SIZE &&
            (apertureSize & (apertureSize - 1UL)) == 0 &&
            apertureSize < usableSize)
          usableSize = apertureSize;
        if (usableSize < RADEON_MIN_FB_SIZE + EARLY_DMA_SIZE)
          continue;

        if (InitDMAMemory(cb,
              (APTR)((ULONG)framebuffer + usableSize - EARLY_DMA_SIZE),
              EARLY_DMA_SIZE, FALSE))
          {
            cb->cb_DMAEarly = TRUE;
            D(kprintf("prometheus.card: early Radeon DMA arena $%08lx size %ld\n",
                      (LONG)cb->cb_DMAArena->dma_Base,
                      cb->cb_DMAArena->dma_Size));
            return TRUE;
          }
        return cb->cb_DMAArena != NULL;
      }
    D(kprintf("prometheus.card: no supported Radeon for early DMA\n"));
    return FALSE;
  }

APTR AllocDMAMemory(__REGD0(ULONG size), __REGA6(struct CardBase *cb))
  {
    struct Library *SysBase = cb->cb_SysBase;
    struct DMAMemArena *arena;
    ULONG aligned;
    ULONG requestedPages;
    ULONG bestStart = 0;
    ULONG bestLength = 0xffffffffUL;
    ULONG index;
    APTR result = NULL;

    if (!size || size > ~0UL - DMA_PAGE_MASK)
      return NULL;

    if (!cb->cb_DMAArena)
      InitEarlyRadeonDMAMemory(cb);

    ObtainSemaphore(cb->cb_MemSem);
    arena = cb->cb_DMAArena;
    if (!arena)
      goto done;
    aligned = (size + DMA_PAGE_MASK) & ~DMA_PAGE_MASK;
    requestedPages = aligned / DMA_PAGE_SIZE;
    if (!requestedPages || requestedPages > arena->dma_PageCount)
      goto done;
    index = 0;
    while (index < arena->dma_PageCount)
      {
        ULONG start;
        ULONG length;

        if (arena->dma_Pages[index].dmp_RunLength)
          {
            ++index;
            continue;
          }
        start = index;
        while (index < arena->dma_PageCount &&
               !arena->dma_Pages[index].dmp_RunLength)
          ++index;
        length = index - start;
        if (length >= requestedPages && length < bestLength)
          {
            bestStart = start;
            bestLength = length;
            if (length == requestedPages)
              break;
          }
      }

    if (bestLength != 0xffffffffUL)
      {
        ULONG page;

        arena->dma_Pages[bestStart].dmp_RunLength = requestedPages;
        arena->dma_Pages[bestStart].dmp_RequestedSize = size;
        for (page = 1; page < requestedPages; ++page)
          {
            arena->dma_Pages[bestStart + page].dmp_RunLength =
              DMA_PAGE_CONTINUATION;
            arena->dma_Pages[bestStart + page].dmp_RequestedSize = 0;
          }
        result = (APTR)((ULONG)arena->dma_Base +
                        bestStart * DMA_PAGE_SIZE);
      }
done:
    ReleaseSemaphore(cb->cb_MemSem);
    return result;
  }

void FreeDMAMemory(__REGA0(APTR memory), __REGD0(ULONG size),
                   __REGA6(struct CardBase *cb))
  {
    struct Library *SysBase = cb->cb_SysBase;
    struct DMAMemArena *arena;
    ULONG base;
    ULONG address;
    ULONG offset;
    ULONG index;
    ULONG runLength;
    ULONG suppliedPages;
    ULONG page;

    if (!memory || !size || size > ~0UL - 3UL)
      return;

    ObtainSemaphore(cb->cb_MemSem);
    arena = cb->cb_DMAArena;
    if (!arena)
      goto done;
    base = (ULONG)arena->dma_Base;
    address = (ULONG)memory;
    if (address < base)
      goto done;
    offset = address - base;
    if (offset >= arena->dma_Size || (offset & DMA_PAGE_MASK))
      goto done;
    index = offset / DMA_PAGE_SIZE;
    runLength = arena->dma_Pages[index].dmp_RunLength;
    suppliedPages = (size + DMA_PAGE_MASK) / DMA_PAGE_SIZE;
    if (!runLength || runLength == DMA_PAGE_CONTINUATION ||
        (!arena->dma_LegacyFree && suppliedPages != runLength) ||
        runLength > arena->dma_PageCount - index)
      {
        D(kprintf("prometheus.card: invalid DMA free $%08lx size %ld\n",
                  (LONG)memory, size));
        goto done;
      }
    for (page = 1; page < runLength; ++page)
      if (arena->dma_Pages[index + page].dmp_RunLength !=
          DMA_PAGE_CONTINUATION)
        goto done;
    for (page = 0; page < runLength; ++page)
      {
        arena->dma_Pages[index + page].dmp_RunLength = 0;
        arena->dma_Pages[index + page].dmp_RequestedSize = 0;
      }
done:
    ReleaseSemaphore(cb->cb_MemSem);
  }

BOOL InitDMAMemory(struct CardBase *cb, APTR memory, ULONG size,
                   BOOL legacyFree)
  {
    struct Library *SysBase = cb->cb_SysBase;
    struct DMAMemArena *arena;
    ULONG pages;
    ULONG extra;
    ULONG allocationSize;

    ULONG start;
    ULONG end;

    if (cb->cb_DMAArena || !memory || !size ||
        size > ~0UL - (ULONG)memory ||
        (ULONG)memory > ~0UL - DMA_PAGE_MASK)
      return FALSE;
    start = ((ULONG)memory + DMA_PAGE_MASK) & ~DMA_PAGE_MASK;
    end = ((ULONG)memory + size) & ~DMA_PAGE_MASK;
    pages = end > start ? (end - start) / DMA_PAGE_SIZE : 0;
    if (!pages)
      {
        arena = AllocMem(sizeof(*arena), MEMF_PUBLIC | MEMF_CLEAR);
        if (!arena)
          return FALSE;
        arena->dma_Base = (APTR)start;
        arena->dma_Size = 0;
        arena->dma_PageCount = 0;
        arena->dma_AllocationSize = sizeof(*arena);
        arena->dma_LegacyFree = legacyFree;
        cb->cb_DMAArena = arena;
        return TRUE;
      }
    extra = pages - 1UL;
    if (extra > (~0UL - sizeof(*arena)) / sizeof(struct DMAMemPage))
      return FALSE;
    allocationSize = sizeof(*arena) + extra * sizeof(struct DMAMemPage);
    arena = AllocMem(allocationSize, MEMF_PUBLIC | MEMF_CLEAR);
    if (!arena)
      return FALSE;
    arena->dma_Base = (APTR)start;
    arena->dma_Size = end - start;
    arena->dma_PageCount = pages;
    arena->dma_AllocationSize = allocationSize;
    arena->dma_LegacyFree = legacyFree;
    cb->cb_DMAArena = arena;
    return TRUE;
  }

VOID FreeDMAMemoryArena(struct CardBase *cb)
  {
    struct Library *SysBase = cb->cb_SysBase;
    struct DMAMemArena *arena;

    if (!cb->cb_MemSem)
      return;
    ObtainSemaphore(cb->cb_MemSem);
    arena = cb->cb_DMAArena;
    if (!arena)
      {
        ReleaseSemaphore(cb->cb_MemSem);
        return;
      }
    cb->cb_DMAArena = NULL;
    cb->cb_DMAEarly = FALSE;
    ReleaseSemaphore(cb->cb_MemSem);
    FreeMem(arena, arena->dma_AllocationSize);
  }
