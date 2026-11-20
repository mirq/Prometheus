#ifndef PROMETHEUS_GEFORCE_H
#define PROMETHEUS_GEFORCE_H

#include <exec/types.h>

/*
 * Handoff from Prometheus.card (card_geforce7200.c) to GeForce7200.chip,
 * byte-copied into bi->CardData[0..] before InitChip(). CardData[14] and
 * [15] belong to the card (JAVOSOFT switch), so the record must stay within
 * 56 bytes.
 *
 * The CPU addresses and sizes are what prometheus.library assigned; the chip
 * repairs the bus itself (bridge windows, 64-bit BAR upper halves, ROM BAR)
 * before touching any of them. ToolTypes is the monitor icon's array, valid
 * only during InitChip() and InitGeForceFeatures(); the chip parses its own
 * options from it.
 */

#define PROM_GEFORCE_HANDOFF_MAGIC   0x50524d47UL /* 'PRMG' */
#define PROM_GEFORCE_HANDOFF_VERSION 1

struct PrometheusGeForceHandoff {
    ULONG Magic;
    UWORD Version;
    UWORD DeviceId;
    APTR Board;            /* PCIBoard * of the GPU (owned by the chip) */
    APTR RegisterBase;     /* BAR0 */
    ULONG RegisterSize;
    APTR FramebufferBase;  /* BAR1 */
    ULONG FramebufferSize;
    APTR RaminBase;        /* BAR3 */
    ULONG RaminSize;
    char **ToolTypes;
};

/* InitGeForceFeatures() feature bits, from the card's generic ToolTypes */
#define PROM_GEFORCE_FEATURE_HWSPRITE (1UL << 1)
#define PROM_GEFORCE_FEATURE_HWTEXT   (1UL << 2)
#define PROM_GEFORCE_FEATURE_PLANAR   (1UL << 5)

#endif
