#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <stdexcept>
#include <cmath>
#include <memory>
#include <zlib.h>

extern "C" {
	#include "libsmf/libsmfc.h"
	#include "libsmf/libsmfcx.h"
}
#include "vgm-file.hpp"
#include "YM2151.hpp"
#include "data-bank-to-sf2.hpp"

#define UINT32 uint32_t
#define UINT16 uint16_t
#define UINT8 uint8_t

#define DEFAULT_PPQN 0x7FFF

// Read a little-endian 32-bit integer from a byte buffer.
static inline UINT32 Read32LE(const UINT8* p)
{
	return (UINT32)p[0]
		 | ((UINT32)p[1] << 8)
		 | ((UINT32)p[2] << 16)
		 | ((UINT32)p[3] << 24);
}

static uint64_t vgmTime2midiTime(uint64_t vgmTime /*timestamp relative to start of song, aka absolute*/, const uint64_t midiTicksPerSecond){
	double vgmTimeInSeconds = vgmTime / (double)VGMSAMPLERATE;
	uint64_t midiTime = llround(vgmTimeInSeconds * midiTicksPerSecond);
	return midiTime;
}

int selectUsedChipByID(int chipID, vector<unique_ptr<Soundchip>>& usedChips) {
	for (int i = 0; i < usedChips.size(); i++){
		if (usedChips[i]->chipID == chipID)
			return i;
	}
	return -1;
}

static void vgm2midi(const char* filePath, int inPPQN = DEFAULT_PPQN){

	// ---- load the whole file into memory ----
	gzFile f = gzopen(filePath, "rb");
	if (!f) { perror("fopen"); return; }
	long fileSize;
	std::vector<UINT8> buf;
	{
		const size_t CHUNK_SIZE = 65536;
		size_t totalRead = 0;
		int bytesRead;
		do {
			buf.resize(totalRead + CHUNK_SIZE);
			bytesRead = gzread(f, buf.data() + totalRead, CHUNK_SIZE);
			if (bytesRead < 0) {
				int errnum = 0;
				fprintf(stderr, "gzread error: %s\n", gzerror(f, &errnum));
				gzclose(f);
				return;
			}
			totalRead += bytesRead;
		} while (bytesRead == CHUNK_SIZE);
		fileSize = (long)totalRead;
	}
	buf.resize(fileSize);
	gzclose(f);

	if (fileSize < 0x40) { printf("fileSize too small (less than 0x40). fileSize: %ld\n", fileSize); return; }

	// ---- validate magic ----
	if (memcmp(buf.data(), "Vgm ", 4) != 0) {
		puts("Not a VGM file."); return;
	}

	// ---- locate the command stream ----
	// VGM ≥ 1.50: vgmDataOffset is at 0x34, relative to that field.
	// VGM  < 1.50: data starts at 0x40.
	UINT32 version = Read32LE(buf.data() + 0x08);
	UINT32 dataStart;
	UINT32 loopStart;
	if (version >= 0x150) {
		UINT32 relOffset = Read32LE(buf.data() + 0x34);
		dataStart = (relOffset == 0 || relOffset == 0x0000000C) ? 0x40 : (0x34 + relOffset);
	} else {
		dataStart = 0x40;
	}
	bool loopStartMarked = false;
	{
		UINT32 relOffset = Read32LE(buf.data() + 0x1C);
		loopStart = relOffset + 0x1C;
		if (loopStart >= fileSize || relOffset == 0)
			loopStart = 0; // no loop
	}
	printf("VGM version  : 0x%08X\n", version);
	printf("Data starts  : 0x%08X\n", dataStart);
	
	// ---- Create Midi File ----
	const int MIDI_BPM=120;
	const int SECONDS_IN_A_MINUTE=60;
	const int MIDI_PPQN = inPPQN;
	Smf* smf = smfCreate();
	if(smf){
		smfSetTimebase(smf, MIDI_PPQN);
	} else {
		printf("Failed to create midi file\n");
		//return 1;
	}
	int vgmTime=0; // number of samples that have elapsed since the start of the song.
	// Channels from the same chip will be in adjacent tracks.
	// each channel from each chip will get its own track.
	// Tracks will be named for their chip and channel.
	// Read the vgm header to figure out what chips are used in a song, then create tracks and track names based on that.
	const uint64_t midiTicksPerSecond = (float)MIDI_PPQN * ((float)MIDI_BPM / SECONDS_IN_A_MINUTE);
	
	// ---- Identify used chips ----
	// TODO: finish
	// TODO: read extra header.
	int totalTracks = 1; // total Midi tracks. track 0 must be the tempo map to be compliant with midi spec. Some DAWs are lenient with this if track 0 has note data.
	vector<unique_ptr<Soundchip>> usedChips; // read all the chip clock values in the header, in the order of when they appear in the header, to figure out which chips are used. Each time a used chip is discovered, a SOUNDCHIP is added to this vector.
	// When tracks are added to the Midi, they are added in the order that SOUNDCHIPs appear in the usedChips vector.
	// a SOUNDCHIP includes chip ID, clock rate, and any chip settings as defined in the header.
	// SOUNDCHIP will likely be a base class that is then inherited by other chip classes. I think specific chip classes should only be defined for chips that have unique settings.
	for (int i = 0; i < CHIPLISTSIZE; i++){
		// i is Chip ID.
		if (headerChipClocks[i] >= dataStart)
			break; // "If the VGM data starts at an offset that is lower than 0x100, all overlapping header bytes have to be handled as they were zero."
		UINT32 curChipClock = Read32LE(buf.data() + headerChipClocks[i]); // headerChipClocks is an array of offsets to the chip clock fields in the header.
		if (curChipClock != 0){ // if the chip clock field is not 0, that means the chip is used in the song; add the chip to usedChips.
			printf("Found chip %s, clock: %u, address: 0x%04X\n", chipNames[i].c_str(), curChipClock, headerChipClocks[i]);
			switch(i){
				case AY8910:
					usedChips.push_back(make_unique<AY8910_Soundchip>(i, curChipClock, totalTracks)); // chips with unique settings in the header file will be created as a specialized child class object.
					break;
				default:
					usedChips.push_back(make_unique<Soundchip>(i, curChipClock, totalTracks)); // all chips without unique settings will be created as a generic Soundchip object.
					break;
			}
			switch(i){
				case YM2151:
					initYM2151(smf, totalTracks, curChipClock);
					break;
				default:
					break;
			}
			totalTracks += chipNumVoices[i];
		}
	}
	
	// add tracks to Midi file, with appropriate names. (track name example: "YM2151 Channel 1"). TODO: a lot of chips have some FM channels, some Square wave channels, some ADPCM channels, etc (YM2608 is an example). Midi tracks should also be named with sound type (eg "YM2608 Channel 12 (ADPCM)"). Other chips like this: YM2610(B), YMF262. This is an ambitious feature that will be added very late in development, or may be delayed indefinitely.
	for (int curTrack = 1, curChip = 0; curTrack < totalTracks; curTrack++){
		if ( curTrack >= (usedChips[curChip]->firstMidiTrack + chipNumVoices[usedChips[curChip]->chipID]) )
			curChip++;
		string curTrackName = chipNames[usedChips[curChip]->chipID] + " Channel " + to_string((curTrack - usedChips[curChip]->firstMidiTrack) + 1);
		// string curTrackName = chipNames[usedChips[curChip]->chipID] + " Ch. " + to_string((curTrack - usedChips[curChip]->firstMidiTrack) + 1); // The word "Channel" might not fit in the space that the DAW displays the track name.
		bool result = smfInsertMetaText(smf, 0, curTrack, SMF_META_TRACKNAME, curTrackName.c_str());
	}
	vgmTime += VGMSAMPLERATE / 4; // add space at the beginning of the song for midi commands that will initialize the chip.
	
	//vector<DAC_Stream> runningDACstreams;
	DAC_Stream runningDACstreams[0x100]; // index corresponds to streamID.

	// ---- walk commands ----
	UINT32 pos	 = dataStart;
	UINT32 cmdIdx  = 0;
	bool   running = true;

	while (running && pos < (UINT32)fileSize) {
		UINT8 cmd = buf[pos]; 
		if (pos >= loopStart && loopStart != 0 && loopStartMarked == false){
			std::string text = "Loop Start";
			smfInsertMetaText(smf, vgmTime2midiTime(vgmTime, midiTicksPerSecond), 1, SMF_META_TEXT, text.c_str()); // TODO: change this from a text event to a marker
			loopStartMarked = true;
		}
		pos++;

		// Uncomment the printf below to dump every command:
		// printf("[%6u] pos=0x%06X  cmd=0x%02X\n", cmdIdx, pos-1, cmd);

		switch (cmd) {

		// --- single-byte wait / end commands ---
		case 0x66:   // End of sound data
			printf("Command 0x66: End of data (after %u commands).\n", cmdIdx);
			running = false;
			break;

		case 0x70: case 0x71: case 0x72: case 0x73:
		case 0x74: case 0x75: case 0x76: case 0x77:
		case 0x78: case 0x79: case 0x7A: case 0x7B:
		case 0x7C: case 0x7D: case 0x7E: case 0x7F: {
			// Wait (cmd & 0x0F)+1 samples
			//printf("address %08X: 0x%02X Wait command\n", pos-1, cmd);
			vgmTime += (cmd & 0x0F)+1;
			break;
		}
		case 0x61: {  // Wait N samples (2-byte operand)
			if (pos + 2 > (UINT32)fileSize) { running = false; break; }
			UINT16 samples = (UINT16)buf[pos] | ((UINT16)buf[pos+1] << 8);
			pos += 2;
			// use 'samples' for timing calculations
			vgmTime += samples;
			break;
		}
		case 0x62: { // Wait 735 samples (1/60 s)
			vgmTime += 735;
			break;
		}
		case 0x63: { // Wait 882 samples (1/50 s)
			vgmTime += 882;
			break;
		}

		// --- 1-operand write commmands (data) for sound chips that only have one register, or just the stereo mask of a sound chip --- // TODO: skip reserved commands. make a common function that runs before this main switch ladder that checks the number of operands in the current command, and breaks the byte step loop if there aren't enough bytes left in the file; like "if (pos + 1 > (UINT32)fileSize) { running = false; break; }" below
		// AY8910 stereo mask
		case 0x31: {
			if (pos + 1 > (UINT32)fileSize) { running = false; break; }
			UINT8 data = buf[pos]; pos++;
			(void)data;
			break;
		}
		// Game Gear PSG stereo
		case 0x4F: {
			if (pos + 1 > (UINT32)fileSize) { running = false; break; }
			UINT8 data = buf[pos]; pos++;
			(void)data;
			break;
		}
		// SN76489
		case 0x50: {
			if (pos + 1 > (UINT32)fileSize) { running = false; break; }
			UINT8 data = buf[pos]; pos++;
			(void)data;
			break;
		}

		// --- 2-operand write commands (reg, data) ---
		// YM2413
		case 0x51:
		// YM2612 port 0
		case 0x52:
		// YM2612 port 1
		case 0x53: {
			UINT32 address = pos-1;
			if (pos + 2 > (UINT32)fileSize) { running = false; break; }
			UINT8 reg  = buf[pos]; pos++;
			UINT8 data = buf[pos]; pos++;
			//printf("address %08X: 0x%02X Sound Chip Register Write: reg=0x%02X data=0x%02X\n", address, cmd, reg, data);
			break;
		}
		// YM2151
		case 0x54: {
			UINT32 address = pos-1;
			if (pos + 2 > (UINT32)fileSize) { running = false; break; }
			UINT8 reg  = buf[pos]; pos++;
			UINT8 data = buf[pos]; pos++;
			//printf("address %08X: 0x%02X YM2151 Register Write: reg=0x%02X data=0x%02X\n", address, cmd, reg, data);
			int curChipIndex = selectUsedChipByID(YM2151, usedChips);
			handleYM2151regWrite(reg, data, smf, vgmTime2midiTime(vgmTime, midiTicksPerSecond), usedChips[curChipIndex]->firstMidiTrack, midiTicksPerSecond / 1000);
			break;
		}
		// YM2203
		case 0x55:
		// YM2608 port 0
		case 0x56:
		// YM2608 port 1
		case 0x57:
		// YM2610 port 0
		case 0x58:
		// YM2610 port 1
		case 0x59:
		// YM3812
		case 0x5A:
		// YM3526
		case 0x5B:
		// Y8950
		case 0x5C:
		// YMZ280B
		case 0x5D:
		// YM2608 port 2 (alias)
		case 0x5E:
		// YM2608 port 3 (alias)
		case 0x5F: {
			UINT32 address = pos-1;
			if (pos + 2 > (UINT32)fileSize) { running = false; break; }
			UINT8 reg  = buf[pos]; pos++;
			UINT8 data = buf[pos]; pos++;
			//printf("address %08X: 0x%02X Sound Chip Register Write: reg=0x%02X data=0x%02X\n", address, cmd, reg, data);
			break;
		}

		// --- data blocks ---
		case 0x67: {
			UINT32 address = pos-1;
			// 0x67 0x66 <type> <size:4>  <data…>
			if (pos + 6 > (UINT32)fileSize) { running = false; break; }
			// UINT8 compat  = buf[pos++]; // always 0x66
			pos++;
			UINT8  blkType = buf[pos]; pos++;
			UINT32 blkSize = Read32LE(buf.data() + pos); pos += 4;
			//printf("address %08X: 0x67 Data block: type=0x%02X  size=%u bytes\n", address, blkType, blkSize & 0x7FFFFFFF);
			addDataBlock(blkType, blkSize, buf.data() + pos);
			pos += (blkSize & 0x7FFFFFFF);   // skip payload
			break;
		}

		// --- PCM seek ---
		case 0xE0: {
			if (pos + 4 > (UINT32)fileSize) { running = false; break; }
			UINT32 offset = Read32LE(buf.data() + pos); pos += 4;
			(void)offset;
			break;
		}
		
		// --- DAC Stream Control Write ---
		case 0x90: {
			UINT32 address = pos-1;
			UINT8  streamID = buf[pos]; pos++;
			UINT8  chipType = buf[pos]; pos++;
			UINT8  writeToPort = buf[pos]; pos++;
			UINT8  writeThisCommandOrRegister = buf[pos]; pos++;
			//printf("address %08X: 0x90 Setup Stream Control: streamID=0x%02X  chipType=0x%02X  writeToPort=%u writeThisCommandOrRegister=0x%02X\n", address, streamID, chipType, writeToPort, writeThisCommandOrRegister);
			// chipType 0x17 is MSM6258
			DAC_Stream newStream(streamID, chipType, writeToPort, writeThisCommandOrRegister);
			runningDACstreams[streamID] = newStream;
			break;
		}
		
		case 0x91: {
			UINT32 address = pos-1;
			UINT8  streamID = buf[pos]; pos++;
			UINT8  dataBankID = buf[pos]; pos++;
			UINT8  stepSize = buf[pos]; pos++;
			UINT8  stepBase = buf[pos]; pos++;
			//printf("address %08X: 0x91 Set Stream Data: streamID=0x%02X  dataBankID=0x%02X  stepSize=%u stepBase=0x%02X\n", address, streamID, dataBankID, stepSize, stepBase);
			// a "data bank" is a group of all data blocks of the same type.
			// which data block to use is selected in the "Start Stream" command (and its fast variant). Start Stream: data block is selected with Data Start offset (note: this makes it possible to begin playback in the middle of a block). Start Stream (fast call): data block is selected with Block ID.
			runningDACstreams[streamID].dataBankID = dataBankID;
			break;
		}
		
		case 0x92: {
			UINT32 address = pos-1;
			UINT8  streamID = buf[pos]; pos++;
			UINT32 frequency = Read32LE(buf.data() + pos); pos += 4; // sample rate, in Hz.
			//printf("address %08X: 0x92 Set Stream Frequency: streamID=0x%02X  frequency=%u\n", address, streamID, frequency);
			runningDACstreams[streamID].frequency = frequency;
			break;
		}
		
		case 0x93: {
			UINT32 address = pos-1;
			UINT8  streamID = buf[pos]; pos++;
			UINT32 dataStartOffset = Read32LE(buf.data() + pos); pos += 4; // Data Start offset in data bank (byte offset in data bank)
			UINT8  lengthMode = buf[pos]; pos++;
			UINT32 dataLength = Read32LE(buf.data() + pos); pos += 4;
			//printf("address %08X: 0x93 Start Stream: streamID=0x%02X  dataStartOffset=0x%08X lengthMode=0x%02X dataLength=%u\n", address, streamID, dataStartOffset, lengthMode, dataLength);
			break;
		}
		
		case 0x94: {
			UINT32 address = pos-1;
			//UINT8  streamID = buf[pos++]; // reads buf[pos], THEN increments pos by 1.
			UINT8  streamID = buf[pos]; pos++;
			//printf("address %08X: 0x94 Stop Stream: streamID=0x%02X\n", address, streamID);
			if (runningDACstreams[streamID].started == true) {
				// handle the stream stopping
				if (runningDACstreams[streamID].chipID == MSM6258 && runningDACstreams[streamID].dataBankID == 0x04){ // Temporary.
					int targetTrack = 0;
					int curChipIndex = selectUsedChipByID(runningDACstreams[streamID].chipID, usedChips);
					// if (curChipIndex == -1) return 1;
					targetTrack = usedChips[curChipIndex]->firstMidiTrack;
					smfInsertNoteOff(smf, vgmTime2midiTime(vgmTime, midiTicksPerSecond), 0, targetTrack, runningDACstreams[streamID].blockID, 127);
				}
				
				//runningDACstreams[streamID] = DAC_Stream(); // delete stream. // This line causes no notes to appear. And I suppose I don't need it anyway since the vgm should overwrite that streamID after it has been stopped?
			}
			break;
		}
		
		case 0x95: {
			UINT32 address = pos-1;
			UINT8  streamID = buf[pos]; pos++;
			UINT16 blockID = (UINT16)buf[pos] | ((UINT16)buf[pos+1] << 8); pos+=2;
			UINT8  flags = buf[pos]; pos++;
			//printf("address %08X: 0x95 Start Stream (fast call): streamID=0x%02X  blockID=0x%04X  flags=0x%02X\n", address, streamID, blockID, flags);
			runningDACstreams[streamID].started = true;
			runningDACstreams[streamID].blockID = blockID;
			if (runningDACstreams[streamID].chipID == MSM6258 && runningDACstreams[streamID].dataBankID == 0x04){ // Temporary. TODO: finish
				int targetTrack = 0;
				int curChipIndex = selectUsedChipByID(runningDACstreams[streamID].chipID, usedChips);
				// if (curChipIndex == -1) return 1;
				targetTrack = usedChips[curChipIndex]->firstMidiTrack;
				smfInsertNoteOn(smf, vgmTime2midiTime(vgmTime, midiTicksPerSecond), 0, targetTrack, blockID, 127);
				//smfInsertNote(smf, vgmTime2midiTime(vgmTime, midiTicksPerSecond), 0, targetTrack, blockID, 127, MIDI_PPQN / 2);
			}
			break;
		}

		// --- extended / two-chip write (0xA0–0xBF range, 3 bytes) ---
		// Many of these follow the pattern: cmd, addr, data
		default:
			if (cmd >= 0xA0 && cmd <= 0xBF) {
				if (pos + 2 > (UINT32)fileSize) { running = false; break; }
				pos += 2;   // skip addr + data
			} else if (cmd >= 0xC0 && cmd <= 0xDF) {
				if (pos + 3 > (UINT32)fileSize) { running = false; break; }
				pos += 3;   // skip chip-offset + addr + data
			} else if (cmd >= 0xE1 && cmd <= 0xFF) {
				if (pos + 4 > (UINT32)fileSize) { running = false; break; }
				pos += 4;
			} else {
				fprintf(stderr, "Unknown command 0x%02X at offset 0x%X – stopping.\n",
						cmd, pos - 1);
				running = false;
			}
			break;
		}

		++cmdIdx;
	}

	printf("Processed %u commands.\n", cmdIdx);
	for (int curTrack = 0; curTrack < totalTracks; curTrack++){
		bool result = smfSetEndTimingOfTrack(smf, curTrack, vgmTime);
	}
	string outFileName = string(filePath) + ".mid";
	smfWriteFile(smf, outFileName.c_str());
	
	dataBanksToSF2(string(filePath));
}

int main(int argc, char* argv[])
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <file.vgm> [Midi_PPQN]\n", argv[0]);
		return 1;
	}

	const char* path = argv[1];
	int inPPQN = DEFAULT_PPQN;
	if (argc >= 3) {
		inPPQN = atoi(argv[2]);
	}

	vgm2midi(path, inPPQN);

	return 0;
}
