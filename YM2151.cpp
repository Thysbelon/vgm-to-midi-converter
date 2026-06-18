#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <stdexcept>
#include <cmath>
#include <memory>
#include "macros.hpp"

extern "C" {
	#include "libsmf/libsmfc.h"
	#include "libsmf/libsmfcx.h"
}
//#include "vgm-file.hpp" // The ChipID enum "YM2151" and the object that stores current chip information "YM2151" have conflicting names. Also, I shouldn't need anything from vgm-file.hpp in this file.

#define UINT32 uint32_t
#define UINT16 uint16_t
#define UINT8 uint8_t

// TODO: for all the functions that return a bool, make them return "false" on failure.
#define INVERT_VALUES true

enum VOPMexCC {
	HardwareLFOfreqMSB = 1, // Hardware LFO FRQ (MSB). The speed of LFO. The LFO frequency register is 8 bits, and a Midi Control Change value is 7 bits, so this VOPMex CC likely only stores the single most significant (rightmost) bit of the hardware register data value.
	HardwareLFO_PMD,
	HardwareLFO_AMD,
	HardwareLFOwaveform = 12,
	Con = 14, // connection algorithm. Determines how the operators will be connected to each other. 3 bits; value range is 0-7.
	FeedbackLevel,
	TL1, // TL OP1. Total Level for Operator 1.
	TL2,
	TL3,
	TL4,
	MUL1, // MUL OP1. Multiply setting for Operator 1.
	MUL2,
	MUL3,
	MUL4,
	DT1_1,
	DT1_2,
	DT1_3,
	DT1_4,
	DT2_1,
	DT2_2,
	DT2_3,
	DT2_4,
	HardwareLFOfreqLSB = 33,
	KS1 = 39, // keyScale
	KS2,
	KS3,
	KS4,
	AR1, // AR OP1. Attack rate for Operator 1.
	AR2,
	AR3,
	AR4,
	D1R1,
	D1R2,
	D1R3,
	D1R4,
	D2R1,
	D2R2,
	D2R3,
	D2R4,
	D1L1,
	D1L2,
	D1L3,
	D1L4,
	RR1,
	RR2,
	RR3,
	RR4,
	AME1 = 70, // Amplitude Modulation Enable for Operator 1.
	AME2,
	AME3,
	AME4,
	HardwareLFOforceReset,
	HardwareLFO_PMS,
	HardwareLFO_AMS,
	NoiseEnable = 80,
	PitchBendRange, // Midi pitch bend range can be set with an RPN, but that requires 3 CC messages, while this VOPMex exclusive way of changing pitch bend range only requires one CC.
	NoiseFreq,
	OpMsk = 93, // Operator Mask. Used to turn operators on or off. All Midi Control Changes are 7 bit values. In OpMsk, the most significant (rightmost) 4 bits will enable or disable operators one through four. A value of 120 to this CC will enable all operators. This CC's values are in YM2151 register format; from least significant to most significant, the bits represent OP1, OP3, OP2, OP4.
};

class Operator {
public:
	//bool isNoteOn = false;
	uint8_t detune1 = 0xFF; // DT1
	uint8_t multiply = 0xFF;
	uint8_t totalLevel = 0xFF; // volume
	uint8_t keyScale = 0xFF; // allows envelope speed to scale with the note being played.
	uint8_t attack = 0xFF;
	uint8_t AMSenable = 0xFF;
	uint8_t decayRate1 = 0xFF; // D1R
	uint8_t detune2 = 0xFF; // DT2
	uint8_t decayRate2 = 0xFF; // D2R
	uint8_t decayLevel = 0xFF; // D1L
	uint8_t releaseRate = 0xFF;
};

class YM2151channel {
public:
	Operator operators[4];
	uint8_t opMask = 0xFF; // currently not used, here just in case it's needed later. In YM2151 register format. From least significant to most significant, the bits represent OP1, OP3, OP2, OP4.
	uint8_t octave = 0;
	uint8_t note = 0; // last note written to register. This will be in YM2151 format. Defaults to 0 because I think that's what it does on hardware.
	int8_t midiNote = -1; // currently playing midi note. -1 means no note is playing.
	uint8_t keyFraction = 0; // 6-bits. Values ranges from 0-63. When keyFraction is 63, the note is pitched up by 63/64ths of a semitone.
	int midiPitchBend = -1; // centered
	uint8_t pan = 0xFF; // 0b10 is right, 0b01 is left, 0b11 is centered.
	uint8_t feedback = 0xFF;
	uint8_t conAlgo = 0xFF; // connection algorithm. Determines how the operators will be connected to each other. 3 bits; value range is 0-7.
	uint8_t AMS = 0xFF;
	uint8_t PMS = 0xFF;
};

class YM2151class { // tracks current state of the chip. This won't be accessible to any cpp file outside of this one.
public:
	YM2151channel channels[8];
	uint8_t noiseEnable = 0xFF;
	uint8_t noiseFreq = 0xFF;
	uint8_t clockA1 = 0xFF;
	uint8_t clockA2 = 0xFF;
	uint8_t clockB = 0xFF;
	//uint32_t clock = 0;
	int LFOfreq = -1; // $18
	bool PMDenable = false; // true for pmd, false for amd.
	uint8_t PMD = 0xFF;
	uint8_t AMD = 0xFF;
	uint8_t ADPCMclockSet = 0xFF; // 0: 4MHz, 1: 8MHz
	uint8_t LFOwave = 0xFF;
};

YM2151class YM2151; // TODO: optimize memory to make sure this object doesn't get created and take up memory unless the vgm file has a YM2151 chip. Maybe YM2151 should be inside the usedChips vector as an object that's a child class of Soundchip, and every chip should have a specialized child class of Soundchip?

static uint8_t swap2and3(uint8_t input){ // func name refers to one-indexed op numbers. function takes and outputs zero-indexed numbers.
	switch(input){
		case 1:
			return 2;
			break;
		case 2:
			return 1;
			break;
		default:
			return input;
			break;
	}
}

static const int8_t ym2151NoteToSemitone[16] = { // Notes in the YM2151 registers are coded in an unintuitive way, thus a lookup table is needed. -1 is a value that is undefined in YM2151 registers.
//  opm register values:
//  0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
//  the notes that the opm register values correspond to ("--" is undefined):
//  D#  E   F   --  F#  G   G#  --  A   A#  B   --  C   C#  D   --
//  The midi note number that will produce the same note:
		3,  4,  5, -1,  6,  7,  8,  -1, 9,  10, 11, -1, 0,  1,  2, -1
};

int YM2151noteToMidi(int channel){ // takes the current octave and note in the YM2151 object and converts it into a midi note.
	if (YM2151.channels[channel].note > 0xF) return -1; // out of bounds of ym2151NoteToSemitone.
	int noteInOctave = ym2151NoteToSemitone[YM2151.channels[channel].note];
	if (noteInOctave < 0) return -1; // undefined note value
	int octave = YM2151.channels[channel].octave;
	// C, C#, D (semitones 0, 1, 2) sit at the top of the scale
	// but the YM2151 octave register treats them as the bottom,
	// so compensate by incrementing the octave for these notes.
	if (noteInOctave <= 2)
		octave += 1;
	return (noteInOctave + ((octave) * 12));
}

int YM2151noteAndKeyFractionToMidiPitchBend(int channel){ // Generates a midi pitch bend that will change the currently playing midi note to the current note, octave and key fraction of the YM2151.
	int midiNote = YM2151noteToMidi(channel);
	if (midiNote < 0) return -1; // undefined note value
	int keyFraction = YM2151.channels[channel].keyFraction;
	int noteDiff = midiNote - YM2151.channels[channel].midiNote; // the number of semitones by which the pitch has changed. If negative, go down that many number of semitones.
	int midiPitchBend = MIDI_PITCH_BEND_CENTER + (noteDiff * 256 /*pitch bend value of one semitone, when the pitch bend range is 32*/) + (keyFraction * 4); // midiPitchBend needs to include both midiNote and keyFraction.
	return midiPitchBend;
}

bool initYM2151(Smf* smf, uint32_t firstTrackOfChip, uint32_t clock){ // write Midi CC that are necessary for YM2151 into the start of every track.
	uint8_t tempSysex1[] = {0xF0, 0x00, 0x43, 0x16, 0x08, 0x03, 0x00, 0xF7};
	smfInsertSysex(smf, 0, 0, firstTrackOfChip, tempSysex1, 8); // Thaumoc: initialize YM2151 chip.
	uint8_t tempSysex2[] = {0xF0, 0x00, 0x43, 0x16, 0x08, 0x03, 0x7F, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF7};
	smfInsertSysex(smf, 0, 0, firstTrackOfChip, tempSysex2, 15); // clock
	for (int i = 0; i < 8; i++){
		smfInsertControl(smf, 0, i, firstTrackOfChip + i, PitchBendRange, 32); // set pitch bend range to +/-32 semitones for all tracks.
		smfInsertPitchBend(smf, 0, i, firstTrackOfChip + i, MIDI_PITCH_BEND_CENTER);
	}
	return true;
}

int registerToCC(uint8_t input /*from register data*/, uint8_t bits /*number of bits that the register value uses*/){
	int output = input;
	int bitShiftAmount = 7 - bits;
	output <<= bitShiftAmount;
	return output;
	// this is equivalent to:
	// return ((double)input / (double)max) * MIDI_CC_MAX
	// this function converts one range of values to another range.
	// using bit shifts is probably more performant than doubles and division.
	// This function was written by referencing VOPMex's source code.
}

bool handleYM2151regWrite(uint8_t reg, uint8_t data, Smf* smf, uint64_t midiTime, uint32_t firstTrackOfChip, uint64_t midiTicksPerMillisecond = 100){
	switch (reg) {
		case 0x01: {
			if (data & 0b10) {
				smfInsertControl(smf, midiTime, 0, firstTrackOfChip, HardwareLFOforceReset, MIDI_CC_MAX);
				smfInsertControl(smf, midiTime, 0, firstTrackOfChip, HardwareLFOforceReset, MIDI_CC_MIN); // TEST
			}
			break;
		}
		case 0x08: {
			int channel = data & 0b111;
			int opMask = (data >> 3) & 0b1111;
			int prevMidiNote = YM2151.channels[channel].midiNote;
			if (opMask == 0) {
				//printf("YM2151: channel %d, midiTime %lu: note-off\n", channel, midiTime);
				if (prevMidiNote != -1){
					smfInsertNoteOff(smf, midiTime, channel, firstTrackOfChip + channel, prevMidiNote, MIDI_CC_MAX);
					YM2151.channels[channel].midiNote = -1;
				}
				break;
			}
			//printf("YM2151: channel %d, midiTime %lu: note-on\n", channel, midiTime);
			if (prevMidiNote != -1){
				smfInsertNoteOff(smf, midiTime + midiTicksPerMillisecond, channel, firstTrackOfChip + channel, prevMidiNote, MIDI_CC_MAX); // disable note-off to OPM by adding trailing note end.
				//printf("YM2151: channel %d: note-on happened while a previous note was still playing.\n", channel);
			}
			if (opMask != YM2151.channels[channel].opMask) {
				smfInsertControl(smf, midiTime, channel, firstTrackOfChip + channel, OpMsk, opMask << 3);
				YM2151.channels[channel].opMask = opMask;
			}
			//smfInsertControl(smf, midiTime, channel, firstTrackOfChip + channel, PitchBendRange, 32); // re-insert pitch bend range for every note to work around VOPMex forgetting the pitch bend range when skipping through the midi.
			int midiPitchBend = YM2151.channels[channel].keyFraction * 4;
			if (midiPitchBend != YM2151.channels[channel].midiPitchBend) {
				smfInsertPitchBend(smf, midiTime, channel, firstTrackOfChip + channel, midiPitchBend);
				YM2151.channels[channel].midiPitchBend = midiPitchBend;
			}
			int newMidiNote = YM2151noteToMidi(channel);
			//printf("YM2151.channels[channel].note: %u, newMidiNote: %d\n", YM2151.channels[channel].note, newMidiNote);
			if (newMidiNote == -1) {
				printf("YM2151: note-on not written to midi channel %d due to undefined note value 0x%02X\n", channel, YM2151.channels[channel].note);
				break;
			}
			bool result = smfInsertNoteOn(smf, midiTime, channel, firstTrackOfChip + channel, newMidiNote, 127);
			//printf("smfInsertNoteOn: %s\n", result ? "success" : "failed");
			if (result == false) printf("smfInsertNoteOn: failed\n");
			YM2151.channels[channel].midiNote = newMidiNote;
			break;
		}
		case 0x0F: { // TEST
			int noiseEnable = (data >> 7) & 1;
			int noiseFreq = data & 0b11111;
			if(YM2151.noiseEnable != noiseEnable){
				smfInsertControl(smf, midiTime, 0, firstTrackOfChip, NoiseEnable, registerToCC(noiseEnable, 1));
			}
			if(YM2151.noiseFreq != noiseFreq){
				smfInsertControl(smf, midiTime, 0, firstTrackOfChip, NoiseFreq, registerToCC(noiseFreq, 5));
			}
			YM2151.noiseEnable = noiseEnable;
			YM2151.noiseFreq = noiseFreq;
			break;
		}
		case 0x14: {
			// This register is used to set music timing logic so a video game can properly switch between music and sound effects. It won't be implemented because it's not relevant to this program.
			break;
		}
		case 0x18: {
			int LFOfreq = data;
			if(YM2151.LFOfreq != LFOfreq){
				smfInsertControl(smf, midiTime, 0, firstTrackOfChip, HardwareLFOfreqMSB, (LFOfreq >> 7)&1); // Midi CC 1 and 33 together form a 14-bit control.
				smfInsertControl(smf, midiTime, 0, firstTrackOfChip, HardwareLFOfreqLSB, LFOfreq & MIDI_CC_MAX);
			}
			YM2151.LFOfreq = LFOfreq;
			break;
		}
		case 0x19: {
			bool PMDenable = (data >> 7) & 1;
			int PMDorAMD = data & 0x7F;
			if (PMDenable == true){
				//if(YM2151.PMD != PMDorAMD){
					smfInsertControl(smf, midiTime, 0, firstTrackOfChip, HardwareLFO_PMD, PMDorAMD);
					//smfInsertControl(smf, midiTime, 0, firstTrackOfChip, HardwareLFO_AMD, 0); // I originally had this here for clarity, but the way I currently have things programmed in Thaumoc means it just overwrites PMD.
					YM2151.PMD = PMDorAMD;
				//}
			} else {
				//if(YM2151.AMD != PMDorAMD){
					smfInsertControl(smf, midiTime, 0, firstTrackOfChip, HardwareLFO_AMD, PMDorAMD);
					//smfInsertControl(smf, midiTime, 0, firstTrackOfChip, HardwareLFO_PMD, 0);
					YM2151.AMD = PMDorAMD;
				//}
			}
			YM2151.PMDenable = PMDenable;
			break;
		}
		case 0x1B: {
			// CT2 will not be implemented. CT1 may be implemented later, but its effect on sound is likely minor.
			int LFOwave = data & 0b11;
			if(YM2151.LFOwave != LFOwave){
				smfInsertControl(smf, midiTime, 0, firstTrackOfChip, HardwareLFOwaveform, registerToCC(LFOwave, 2));
			}
			YM2151.LFOwave = LFOwave;
			break;
		}
		case 0x20:
		case 0x21:
		case 0x22:
		case 0x23:
		case 0x24:
		case 0x25:
		case 0x26:
		case 0x27: {
			int channel = reg - 0x20;
			int pan = (data >> 6) & 0b11;
			int feedback = (data >> 3) & 0b111;
			int conAlgo = data & 0b111;
			if (YM2151.channels[channel].pan != pan) {
				int midiPan = 64;
				if (YM2151.channels[channel].pan == 0)
					smfInsertControl(smf, midiTime, channel, firstTrackOfChip + channel, SMF_CONTROL_VOLUME, MIDI_CC_MAX);
				switch (pan){
					case 0b10:
						midiPan = MIDI_CC_MAX;
						break;
					case 0b01:
						midiPan = MIDI_CC_MIN;
						break;
					case 0b11:
						midiPan = 64;
						break;
					default:
						smfInsertControl(smf, midiTime, channel, firstTrackOfChip + channel, SMF_CONTROL_VOLUME, MIDI_CC_MIN);
						break;
				}
				smfInsertControl(smf, midiTime, channel, firstTrackOfChip + channel, SMF_CONTROL_PANPOT, midiPan);
			}
			if(YM2151.channels[channel].feedback != feedback){
				smfInsertControl(smf, midiTime, channel, firstTrackOfChip + channel, FeedbackLevel, registerToCC(feedback, 3));
			}
			if(YM2151.channels[channel].conAlgo != conAlgo){
				smfInsertControl(smf, midiTime, channel, firstTrackOfChip + channel, Con, registerToCC(conAlgo, 3));
			}
			YM2151.channels[channel].pan = pan;
			YM2151.channels[channel].feedback = feedback;
			YM2151.channels[channel].conAlgo = conAlgo;
			break;
		}
		case 0x28:
		case 0x29:
		case 0x2A:
		case 0x2B:
		case 0x2C:
		case 0x2D:
		case 0x2E:
		case 0x2F: {
			int channel = reg - 0x28;
			//printf("YM2151: channel %d, midiTime %lu, set note.\n", channel, midiTime);
			int octave = (data >> 4) & 0b111;
			int note = data & 0b1111;
			YM2151.channels[channel].octave = octave;
			YM2151.channels[channel].note = note;
			if (YM2151.channels[channel].midiNote != -1) { // if a note is currently playing
				// write a pitch bend.
				// TODO: check if midiPitchBend is out of range here?
				int midiPitchBend = YM2151noteAndKeyFractionToMidiPitchBend(channel);
				if (midiPitchBend == -1) break;
				if (midiPitchBend != YM2151.channels[channel].midiPitchBend) {
					smfInsertPitchBend(smf, midiTime, channel, firstTrackOfChip + channel, midiPitchBend);
					YM2151.channels[channel].midiPitchBend = midiPitchBend;
				}
			}
			break;
		}
		case 0x30:
		case 0x31:
		case 0x32:
		case 0x33:
		case 0x34:
		case 0x35:
		case 0x36:
		case 0x37: {
			int channel = reg - 0x30;
			int keyFraction = (data >> 2) & 0b111111;
			YM2151.channels[channel].keyFraction = keyFraction;
			if (YM2151.channels[channel].midiNote != -1) { // if a note is currently playing
				// write a pitch bend.
				int midiPitchBend = YM2151noteAndKeyFractionToMidiPitchBend(channel);
				if (midiPitchBend == -1) break;
				if (midiPitchBend > MIDI_PITCH_BEND_MAX || midiPitchBend < MIDI_PITCH_BEND_MIN){
					printf("midiPitchBend out of range. writing pitch-change note instead\n");
					int newMidiPitchBend = YM2151.channels[channel].keyFraction * 4;
					if (newMidiPitchBend != YM2151.channels[channel].midiPitchBend) {
						smfInsertPitchBend(smf, midiTime, channel, firstTrackOfChip + channel, midiPitchBend);
						YM2151.channels[channel].midiPitchBend = newMidiPitchBend;
					}
					int newMidiNote = YM2151noteToMidi(channel);
					if (newMidiNote == -1) break;
					smfInsertNoteOff(smf, midiTime + midiTicksPerMillisecond, channel, firstTrackOfChip + channel, YM2151.channels[channel].midiNote, MIDI_CC_MAX); // disable note-off of previous note by adding trailing note end.
					bool result = smfInsertNoteOn(smf, midiTime, channel, firstTrackOfChip + channel, newMidiNote, 43);
					YM2151.channels[channel].midiNote = newMidiNote;
				} else {
					if (midiPitchBend != YM2151.channels[channel].midiPitchBend) {
						smfInsertPitchBend(smf, midiTime, channel, firstTrackOfChip + channel, midiPitchBend);
						YM2151.channels[channel].midiPitchBend = midiPitchBend;
					}
				}
				
			}
			break;
		}
		case 0x38:
		case 0x39:
		case 0x3A:
		case 0x3B:
		case 0x3C:
		case 0x3D:
		case 0x3E:
		case 0x3F: {
			int channel = reg - 0x38;
			int pms = (data >> 4) & 0b111;
			int ams = data & 0b11;
			if(YM2151.channels[channel].PMS != pms){
				smfInsertControl(smf, midiTime, channel, firstTrackOfChip + channel, HardwareLFO_PMS, registerToCC(pms, 3));
			}
			if(YM2151.channels[channel].AMS != ams){
				smfInsertControl(smf, midiTime, channel, firstTrackOfChip + channel, HardwareLFO_AMS, registerToCC(ams, 2));
			}
			YM2151.channels[channel].PMS = pms;
			YM2151.channels[channel].AMS = ams;
			break;
		}
		case 0x40:
		case 0x41:
		case 0x42:
		case 0x43:
		case 0x44:
		case 0x45:
		case 0x46:
		case 0x47: 
		case 0x48:
		case 0x49:
		case 0x4A:
		case 0x4B:
		case 0x4C:
		case 0x4D:
		case 0x4E:
		case 0x4F:
		case 0x50:
		case 0x51:
		case 0x52:
		case 0x53:
		case 0x54:
		case 0x55:
		case 0x56:
		case 0x57: 
		case 0x58:
		case 0x59:
		case 0x5A:
		case 0x5B:
		case 0x5C:
		case 0x5D:
		case 0x5E:
		case 0x5F:
		{
			int channel = (reg - 0x40) % 8;
			int op = (reg - 0x40) / 8;
			// the above math assumes that operators follow a linear order. But they don't. in YM2151 registers, anything involving operators usually goes in the order of op1 -> op3 -> op2 -> op4. op1 is accessed with index 0 throughout this cpp file.
			op = swap2and3(op);
			int detune1 = (data >> 4) & 0b111;
			int multiply = data & 0b1111;
			if(YM2151.channels[channel].operators[op].detune1 != detune1){
				smfInsertControl(smf, midiTime, channel, firstTrackOfChip + channel, DT1_1 + op, registerToCC(detune1, 3));
			}
			if(YM2151.channels[channel].operators[op].multiply != multiply){
				smfInsertControl(smf, midiTime, channel, firstTrackOfChip + channel, MUL1 + op, registerToCC(multiply, 4));
			}
			YM2151.channels[channel].operators[op].detune1 = detune1;
			YM2151.channels[channel].operators[op].multiply = multiply;
			break;
		}
		case 0x60:
		case 0x61:
		case 0x62:
		case 0x63:
		case 0x64:
		case 0x65:
		case 0x66:
		case 0x67: 
		case 0x68:
		case 0x69:
		case 0x6A:
		case 0x6B:
		case 0x6C:
		case 0x6D:
		case 0x6E:
		case 0x6F:
		case 0x70:
		case 0x71:
		case 0x72:
		case 0x73:
		case 0x74:
		case 0x75:
		case 0x76:
		case 0x77: 
		case 0x78:
		case 0x79:
		case 0x7A:
		case 0x7B:
		case 0x7C:
		case 0x7D:
		case 0x7E:
		case 0x7F:
		{
			int channel = (reg - 0x60) % 8;
			int op = (reg - 0x60) / 8;
			op = swap2and3(op);
			int totalLevel = data & 0x7F;
			if(YM2151.channels[channel].operators[op].totalLevel != totalLevel){
#if INVERT_VALUES
				smfInsertControl(smf, midiTime, channel, firstTrackOfChip + channel, TL1 + op, 0x7F - totalLevel); // in register values: 0x7F is quietest. Invert value for midi to make it more intuitive. Supported by VOPMex
#else
				smfInsertControl(smf, midiTime, channel, firstTrackOfChip + channel, TL1 + op, totalLevel);
#endif
				
			}
			YM2151.channels[channel].operators[op].totalLevel = totalLevel;
			break;
		}
		case 0x80:
		case 0x81:
		case 0x82:
		case 0x83:
		case 0x84:
		case 0x85:
		case 0x86:
		case 0x87: 
		case 0x88:
		case 0x89:
		case 0x8A:
		case 0x8B:
		case 0x8C:
		case 0x8D:
		case 0x8E:
		case 0x8F:
		case 0x90:
		case 0x91:
		case 0x92:
		case 0x93:
		case 0x94:
		case 0x95:
		case 0x96:
		case 0x97: 
		case 0x98:
		case 0x99:
		case 0x9A:
		case 0x9B:
		case 0x9C:
		case 0x9D:
		case 0x9E:
		case 0x9F:
		{
			int channel = (reg - 0x80) % 8;
			int op = (reg - 0x80) / 8;
			op = swap2and3(op);
			int keyScale = (data >> 6) & 0b11;
			int attack = data & 0b11111;
			if(YM2151.channels[channel].operators[op].keyScale != keyScale){
				smfInsertControl(smf, midiTime, channel, firstTrackOfChip + channel, KS1 + op, registerToCC(keyScale, 2));
			}
			if(YM2151.channels[channel].operators[op].attack != attack){
#if INVERT_VALUES
				smfInsertControl(smf, midiTime, channel, firstTrackOfChip + channel, AR1 + op, MIDI_CC_MAX - registerToCC(attack, 5));
#else
				smfInsertControl(smf, midiTime, channel, firstTrackOfChip + channel, AR1 + op, registerToCC(attack, 5));
#endif
			}
			YM2151.channels[channel].operators[op].keyScale = keyScale;
			YM2151.channels[channel].operators[op].attack = attack;
			break;
		}
		case 0xA0:
		case 0xA1:
		case 0xA2:
		case 0xA3:
		case 0xA4:
		case 0xA5:
		case 0xA6:
		case 0xA7: 
		case 0xA8:
		case 0xA9:
		case 0xAA:
		case 0xAB:
		case 0xAC:
		case 0xAD:
		case 0xAE:
		case 0xAF:
		case 0xB0:
		case 0xB1:
		case 0xB2:
		case 0xB3:
		case 0xB4:
		case 0xB5:
		case 0xB6:
		case 0xB7: 
		case 0xB8:
		case 0xB9:
		case 0xBA:
		case 0xBB:
		case 0xBC:
		case 0xBD:
		case 0xBE:
		case 0xBF:
		{
			int channel = (reg - 0xA0) % 8;
			int op = (reg - 0xA0) / 8;
			op = swap2and3(op);
			int AMSenable = (data >> 7) & 1;
			int decayRate1 = data & 0b11111;
			if(YM2151.channels[channel].operators[op].AMSenable != AMSenable){
				smfInsertControl(smf, midiTime, channel, firstTrackOfChip + channel, AME1 + op, registerToCC(AMSenable, 1));
			}
			if(YM2151.channels[channel].operators[op].decayRate1 != decayRate1){
#if INVERT_VALUES
				smfInsertControl(smf, midiTime, channel, firstTrackOfChip + channel, D1R1 + op, MIDI_CC_MAX - registerToCC(decayRate1, 5));
#else
				smfInsertControl(smf, midiTime, channel, firstTrackOfChip + channel, D1R1 + op, registerToCC(decayRate1, 5));
#endif
			}
			YM2151.channels[channel].operators[op].AMSenable = AMSenable;
			YM2151.channels[channel].operators[op].decayRate1 = decayRate1;
			break;
		}
		case 0xC0:
		case 0xC1:
		case 0xC2:
		case 0xC3:
		case 0xC4:
		case 0xC5:
		case 0xC6:
		case 0xC7: 
		case 0xC8:
		case 0xC9:
		case 0xCA:
		case 0xCB:
		case 0xCC:
		case 0xCD:
		case 0xCE:
		case 0xCF:
		case 0xD0:
		case 0xD1:
		case 0xD2:
		case 0xD3:
		case 0xD4:
		case 0xD5:
		case 0xD6:
		case 0xD7: 
		case 0xD8:
		case 0xD9:
		case 0xDA:
		case 0xDB:
		case 0xDC:
		case 0xDD:
		case 0xDE:
		case 0xDF: // TODO: make a function for these operator writes, to reduce duplicate code.
		{
			int channel = (reg - 0xC0) % 8;
			int op = (reg - 0xC0) / 8;
			op = swap2and3(op);
			int detune2 = (data >> 6) & 0b11;
			int decayRate2 = data & 0b11111;
			if(YM2151.channels[channel].operators[op].detune2 != detune2){
				smfInsertControl(smf, midiTime, channel, firstTrackOfChip + channel, DT2_1 + op, registerToCC(detune2, 2));
			}
			if(YM2151.channels[channel].operators[op].decayRate2 != decayRate2){
#if INVERT_VALUES
				smfInsertControl(smf, midiTime, channel, firstTrackOfChip + channel, D2R1 + op, MIDI_CC_MAX - registerToCC(decayRate2, 5));
#else
				smfInsertControl(smf, midiTime, channel, firstTrackOfChip + channel, D2R1 + op, registerToCC(decayRate2, 5));
#endif
			}
			YM2151.channels[channel].operators[op].detune2 = detune2;
			YM2151.channels[channel].operators[op].decayRate2 = decayRate2;
			break;
		}
		case 0xE0:
		case 0xE1:
		case 0xE2:
		case 0xE3:
		case 0xE4:
		case 0xE5:
		case 0xE6:
		case 0xE7: 
		case 0xE8:
		case 0xE9:
		case 0xEA:
		case 0xEB:
		case 0xEC:
		case 0xED:
		case 0xEE:
		case 0xEF:
		case 0xF0:
		case 0xF1:
		case 0xF2:
		case 0xF3:
		case 0xF4:
		case 0xF5:
		case 0xF6:
		case 0xF7: 
		case 0xF8:
		case 0xF9:
		case 0xFA:
		case 0xFB:
		case 0xFC:
		case 0xFD:
		case 0xFE:
		case 0xFF:
		{
			int channel = (reg - 0xE0) % 8;
			int op = (reg - 0xE0) / 8;
			op = swap2and3(op);
			int decayLevel = (data >> 4) & 0b1111;
			int releaseRate = data & 0b1111;
#if INVERT_VALUES
			if(YM2151.channels[channel].operators[op].decayLevel != decayLevel){
				smfInsertControl(smf, midiTime, channel, firstTrackOfChip + channel, D1L1 + op, MIDI_CC_MAX - registerToCC(decayLevel, 4));
			}
			if(YM2151.channels[channel].operators[op].releaseRate != releaseRate){
				smfInsertControl(smf, midiTime, channel, firstTrackOfChip + channel, RR1 + op, MIDI_CC_MAX - registerToCC(releaseRate, 4));
			}
#else
			if(YM2151.channels[channel].operators[op].decayLevel != decayLevel){
				smfInsertControl(smf, midiTime, channel, firstTrackOfChip + channel, D1L1 + op, registerToCC(decayLevel, 4));
			}
			if(YM2151.channels[channel].operators[op].releaseRate != releaseRate){
				smfInsertControl(smf, midiTime, channel, firstTrackOfChip + channel, RR1 + op, registerToCC(releaseRate, 4));
			}
#endif
			YM2151.channels[channel].operators[op].decayLevel = decayLevel;
			YM2151.channels[channel].operators[op].releaseRate = releaseRate;
			break;
		}
		default:
			printf("Unknown/unimplemented register 0x%02X\n", reg);
			break;
	}
	return true;
}