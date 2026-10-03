GBDK ?= $(CURDIR)/.tools/gbdk
LCC = $(GBDK)/bin/lcc
PYTHON ?= $(CURDIR)/.venv/bin/python
BUILD_DIR ?= build
TARGET = $(BUILD_DIR)/cipherspace.gbc
SOURCES = src/main.c src/render.c src/cinema.c generated/ui.c generated/scene0.c generated/scene1.c generated/scene2.c generated/scene3.c generated/flight.c $(wildcard generated/cinema*.c)
OBJECTS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(SOURCES))
CFLAGS = -Isrc -Igenerated -debug -Wf--max-allocs-per-node50000
LDFLAGS = -Wm-yC -Wm-yt0x1B -Wm-yo16 -Wm-ya1 -Wm-ynCIPHERSPACE -Wl-j -Wl-m

.PHONY: all test release clean
all: $(TARGET)
$(TARGET): $(OBJECTS)
	"$(LCC)" $(CFLAGS) $(LDFLAGS) -o $@ $(OBJECTS)
	"$(PYTHON)" tools/check-rom.py $@
$(BUILD_DIR)/%.o: %.c src/game.h
	@mkdir -p $(dir $@)
	"$(LCC)" $(CFLAGS) -c -o $@ $<
test: $(TARGET)
	CIPHERSPACE_ROM="$(abspath $(TARGET))" CIPHERSPACE_TEST_OUTPUT="$(abspath $(BUILD_DIR))" "$(PYTHON)" tests/playthrough.py
release: test
	CIPHERSPACE_BUILD_DIR="$(abspath $(BUILD_DIR))" "$(PYTHON)" tools/package-release.py
clean:
	rm -f $(OBJECTS) $(TARGET)
