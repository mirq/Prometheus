CROSS ?= /opt/amiga/bin/m68k-amigaos-
CC := $(CROSS)gcc
STRIP := $(CROSS)strip

TARGET := Prometheus.card
BUILD_DIR := build
CARD_DIR := PrometheusCard
SOURCES := \
	$(CARD_DIR)/vbcc_libinit.c \
	$(CARD_DIR)/dma.c \
	$(CARD_DIR)/card_radeon9200.c \
	$(CARD_DIR)/card_s3virge.c \
	$(CARD_DIR)/card_3dlabspermedia2.c \
	$(CARD_DIR)/card_3dfxvoodoo.c \
	$(CARD_DIR)/card.c
OBJECTS := $(patsubst $(CARD_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))

CPPFLAGS := \
	-I$(CARD_DIR) \
	-IPromLib \
	-IPromLib/include \
	-I$(CARD_DIR)/proto \
	-I$(CARD_DIR)/clib \
	-I$(CARD_DIR)/inline
CFLAGS := \
	-std=gnu99 \
	-O2 \
	-Wall \
	-m68020-60 \
	-mregparm=4 \
	-noixemul \
	-ffreestanding \
	-fno-builtin
LDFLAGS := \
	-m68020-60 \
	-mregparm=4 \
	-noixemul \
	-ramiga-lib \
	-nostartfiles \
	-nodefaultlibs

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) $^ -lamiga -lgcc -o $(BUILD_DIR)/$@
	$(STRIP) --strip-unneeded $(BUILD_DIR)/$@ -o $@

$(BUILD_DIR)/%.o: $(CARD_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(OBJECTS:.o=.d)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
