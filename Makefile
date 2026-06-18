CC=gcc
CPPC=g++

CFLAGS=
CPPFLAGS= -std=c++17
LINKFLAGS= -lz 

all: vgm-to-midi-converter

vgm-to-midi-converter: main.cpp libsmfc.o libsmfcx.o adpcm.o YM2151.o data-bank-to-sf2.o sf2cute/build/libsf2cute.a
	$(CPPC) $^ -o $@ $(CPPFLAGS) $(LINKFLAGS)
	
libsmfc.o: libsmf/libsmfc.c
	$(CC) -c $^ -o $@ $(CFLAGS)

libsmfcx.o: libsmf/libsmfcx.c
	$(CC) -c $^ -o $@ $(CFLAGS)

adpcm.o: adpcm/adpcm.c
	$(CC) -c $^ -o $@ $(CFLAGS)

YM2151.o: YM2151.cpp
	$(CPPC) -c $^ -o $@ $(CPPFLAGS)

data-bank-to-sf2.o: data-bank-to-sf2.cpp
	$(CPPC) -c $^ -o $@ $(CPPFLAGS)

sf2cute/build/libsf2cute.a:
	cd sf2cute && cmake -B build . && cd build && make

clean:
	-rm -f vgm-to-midi-converter *.exe *.o
	-rm -rf sf2cute/build