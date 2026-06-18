#pragma once

#define MIDI_PITCH_BEND_CENTER 0 // uses libsmf format. Other midi writing libraries may use 8192 as center.
#define MIDI_PITCH_BEND_MAX 8192
#define MIDI_PITCH_BEND_MIN -8192
#define MIDI_CC_MAX 0x7F // 127 in decimal
#define MIDI_CC_MIN 0