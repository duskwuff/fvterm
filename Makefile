FILES = \
	TerminalFont \
	TerminalPTY \
	TerminalView \
	TerminalWindow \
	TerminalEmulator \
	main

NIBS = MainMenu TerminalWindow
FONTS = \
	fixed-13 \
	monaco-12 \
	vga-16 \
	terminus-32

APPEXTRAS = Info.plist Credits.rtf fvterm.icns

APPDIR = build/fvterm.app/Contents



CC = /usr/bin/gcc-4.2

CFLAGS = -std=gnu99 -Wall -Werror -Wno-multichar \
	 -Ibuild -Winvalid-pch \
	 -O0 -ggdb -DDEBUG

LDFLAGS = -framework Cocoa



default: build/fvterm.bin $(NIBS:%=build/%.nib) $(FONTS:%=build/%.vtf)
	@mkdir -p $(APPDIR)/{MacOS,Resources}/
	cp build/fvterm.bin $(APPDIR)/MacOS/fvterm
	cp -a $(NIBS:%=build/%.nib) $(FONTS:%=build/%.vtf) $(APPEXTRAS) \
	    $(APPDIR)/Resources/
	cp Info.plist $(APPDIR)/

build/fontpacker: build/TerminalFont.o build/fontpacker.o
	$(CC) $(LDFLAGS) $+ -o build/fontpacker

build/fvterm.bin: $(FILES:%=build/%.o)
	$(CC) $(LDFLAGS) $+ -o $@

build/%.gch: %.h
	$(CC) -c $(CFLAGS) -x objective-c-header $< -o $@

build/%.o: %.[cm] build/Prefix.gch
	$(CC) -MMD -MP -c $(CFLAGS) $< -o $@

build/%.nib: %.xib
	ibtool $< --compile $@

build/%.vtf: build/fontpacker fonts/%/fontconfig.ini
	touch $@
	build/fontpacker fonts/$*/fontconfig.ini $@

-include $(FILES:%=build/%.d)

.PHONY: build/fvterm.app
.PRECIOUS: build/Prefix.gch

run: default
	$(APPDIR)/MacOS/fvterm

debug: default
	@gdb -x .gdbscript $(APPDIR)/MacOS/fvterm

clean:
	rm -rf \
	    build/*.{o,d,vtf,nib} \
	    build/{fvterm.app,fvterm.bin,fontpacker}
