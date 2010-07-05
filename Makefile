TARGETS = fontpacker fvterm

FILES = \
	TerminalFont \
	TerminalPTY \
	TerminalView \
	TerminalWindow \
	TerminalWindow_Emulation \
	main

NIBS = MainMenu TerminalWindow
FONTS = fixed13
APPEXTRAS = Info.plist Credits.rtf

APPDIR = build/fvterm.app/Contents



CC = /usr/bin/gcc-4.2

CFLAGS = -std=gnu99 -Wall \
	 -Ibuild -Winvalid-pch \
	 -O2 -ggdb

LDFLAGS = -framework Cocoa



default: build/fvterm.bin $(NIBS:%=build/%.nib) $(FONTS:%=build/%.vtf)
	@mkdir -p $(APPDIR)/{MacOS,Resources}/
	cp build/fvterm.bin $(APPDIR)/MacOS/fvterm
	cp -a $(NIBS:%=build/%.nib) $(FONTS:%=build/%.vtf) $(APPEXTRAS) $(APPDIR)/Resources/
	cp Info.plist $(APPDIR)/

build/fontpacker: build/TerminalFont.o build/fontpacker.o
	$(CC) $(LDFLAGS) $+ -o build/fontpacker

build/fvterm.bin: $(FILES:%=build/%.o)
	$(CC) $(LDFLAGS) $+ -o $@

build/%.gch: %.h
	$(CC) -c $(CFLAGS) -x objective-c-header $< -o $@

build/%.o: %.m build/Prefix.gch
	$(CC) -MMD -MP -c $(CFLAGS) $< -o $@

build/%.nib: %.xib
	ibtool $< --compile $@

build/%.vtf: build/fontpacker
	touch $@
	build/fontpacker fonts/$*/fontconfig.ini $@

-include $(FILES:%=build/%.d)

.PHONY: build/fvterm.app
.PRECIOUS: build/Prefix.gch

run: default
	@gdb $(APPDIR)/Contents/MacOS/fvterm

clean:
	rm -rf build/*.{o,d,vtf,nib} build/fvterm.app
