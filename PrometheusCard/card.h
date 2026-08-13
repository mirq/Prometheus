#include "boardinfo.h"

#define JSetOldDPMSLevel    CardData[14]
#define JDPMSLevel          CardData[15]

#define VENDOR_E3B            3643
#define VENDOR_MATAY          44359
#define DEVICE_FIRESTORM      200
#define DEVICE_PROMETHEUS     1
#define BOARD_NAME          "Prometheus"
#define BOARD_TYPE          BT_Prometheus
#define EARLY_DMA_SIZE      (2UL * 1024UL * 1024UL)

#define LIB(a) struct Library *##a = cb->cb_##a

#ifdef DEBUG
#define D(x) x
void kprintf(STRPTR format, ...);
#else
#define D(x)
#endif

struct DMAMemPage
  {
    ULONG dmp_RunLength;
    ULONG dmp_RequestedSize;
  };

struct DMAMemArena
  {
    APTR             dma_Base;
    ULONG            dma_Size;
    ULONG            dma_PageCount;
    ULONG            dma_AllocationSize;
    BOOL             dma_LegacyFree;
    struct DMAMemPage dma_Pages[1];
  };

struct CardBase
  {
    struct Library        cb_Library;
    UBYTE                 cb_Flags;
    UBYTE                 cb_Pad;
    struct Library       *cb_SysBase;
    struct Library       *cb_ExpansionBase;
    APTR                  cb_SegList;
    STRPTR                cb_Name;

    /* standard fields ends here */

    struct Library         *cb_PrometheusBase;
    BOOL                    cb_DMAMemGranted; /* retained for private layout compatibility */
    APTR                    cb_LegacyIOBase;
    APTR                    cb_MemPool;
    struct DMAMemArena     *cb_DMAArena;
    struct SignalSemaphore *cb_MemSem;
    BOOL                    cb_DMAEarly;
    BOOL                    cb_DMAEarlyAttempted;
  };

BOOL Init3dfxVoodoo(struct CardBase *cb, struct BoardInfo *bi);      // check Banshee/Voodoo3/4/5 based cards
BOOL Init3DLabsPermedia2(struct CardBase *cb, struct BoardInfo *bi); // check Permedia2 based cards (3DLabs/TI)
BOOL InitCirrusGD5446(struct BoardInfo *bi);    // check GD5446 based Cirrus cards
BOOL InitS3ViRGE(struct CardBase *cb, struct BoardInfo *bi);         // check ViRGE based S3 cards
BOOL InitRadeon9200(struct CardBase *cb, struct BoardInfo *bi);
BOOL InitRadeon9200Features(struct BoardInfo *bi, ULONG features);
void CompleteRadeon9200(struct CardBase *cb, struct BoardInfo *bi);
void AbortRadeon9200(struct BoardInfo *bi);
BOOL InitDMAMemory(struct CardBase *cb, APTR memory, ULONG size,
                   BOOL legacyFree);
BOOL InitEarlyRadeonDMAMemory(struct CardBase *cb);
VOID FreeDMAMemoryArena(struct CardBase *cb);

void RegisterIntServer(struct CardBase *cb, void *board, struct Interrupt *interrupt);
void RegisterOwner(struct CardBase *cb, void *board, struct Node *driver);

BOOL InitCard(__REGA0(struct BoardInfo *bi), __REGA1(char **ToolTypes), __REGA6(struct CardBase *base));
BOOL FindCard(__REGA0(struct BoardInfo *bi), __REGA6(struct CardBase *base));
APTR AllocDMAMemory(__REGD0(ULONG size), __REGA6(struct CardBase *cb));
void FreeDMAMemory(__REGA0(APTR membase), __REGD0(ULONG memsize), __REGA6(struct CardBase *cb));

#define NewList(l) (((struct List*)l)->lh_TailPred = (struct Node*)(l), \
                    ((struct List*)l)->lh_Tail = 0, \
                    ((struct List*)l)->lh_Head = (struct Node*)&(((struct List*)l)->lh_Tail))
