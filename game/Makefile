GBDK ?= $(CURDIR)/.tools/gbdk
LCC = $(GBDK)/bin/lcc
PYTHON ?= $(CURDIR)/.venv/bin/python
TARGET = build/cipherspace.gbc
SOURCES = src/main.c src/render.c generated/ui.c generated/scene0.c generated/scene1.c generated/scene2.c generated/scene3.c generated/flight.c
OBJECTS = $(patsubst %.c,build/%.o,$(SOURCES))
CFLAGS = -Isrc -Igenerated -debug -Wf--max-allocs-per-node50000
LDFLAGS = -Wm-yC -Wm-yt0x1B -Wm-yo8 -Wm-ya1 -Wm-ynCIPHERSPACE -Wl-j -Wl-m

.PHONY: all test release clean
all: $(TARGET)
$(TARGET): $(OBJECTS)
	$(LCC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJECTS)
	$(PYTHON) tools/check-rom.py $@
build/%.o: %.c src/game.h
	@mkdir -p $(dir $@)
	$(LCC) $(CFLAGS) -c -o $@ $<
test: $(TARGET)
	$(PYTHON) tests/playthrough.py
release: test
	$(PYTHON) tools/package-release.py
clean:
	rm -f $(OBJECTS) $(TARGET)
