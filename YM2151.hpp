#pragma once

#include <cstdio>
#include <cstdint>
#include <cstring>

extern "C" {
	#include "libsmf/libsmfc.h"
	#include "libsmf/libsmfcx.h"
}

bool initYM2151(Smf* smf, uint32_t firstTrackOfChip, uint32_t clock);
bool handleYM2151regWrite(uint8_t reg, uint8_t data, Smf* smf, uint64_t midiTime, uint32_t firstTrackOfChip, uint64_t midiTicksPerMillisecond = 100);