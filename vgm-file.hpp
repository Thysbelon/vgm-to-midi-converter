#ifndef __VGMPLAYER_HPP__
#define __VGMPLAYER_HPP__

#include <map>
#include <vector>

using namespace std;

const map<const string, const pair<const int, const int>> headerGeneralSettings = { // {name, {address, size of value in bytes}}
	{"ident", {0x00, 4}},
	{"eofOffset", {0x04, 4}},
	{"version", {0x08, 4}},
	{"GD3offset", {0x14, 4}},
	{"totalSamples", {0x18, 4}},
	{"loopOffset", {0x1C, 4}},
	{"loopSamples", {0x20, 4}},
	{"rate", {0x24, 4}},
	{"vgmDataOffset", {0x34, 4}},
	{"volumeModifier", {0x7C, 1}},
	{"loopBase", {0x7E, 1}},
	{"loopModifier", {0x7F, 1}},
	{"extraHeaderOffset", {0xBC, 4}},
}; 
const int headerChipClocks[] = { // address. The index of each entry corresponds to Chip ID. I think this array will only be used to initially read clocks to figure out what to add to "usedChips". // TODO: should the constant arrays in this file have all caps names?
	0x0C,
	0x10,
	0x2C,
	0x30,
	0x38,
	0x40,
	0x44,
	0x48,
	0x4C,
	0x50,
	0x54,
	0x58,
	0x5C,
	0x60,
	0x64,
	0x68,
	0x6C,
	0x70,
	0x74,
	0x80,
	0x84,
	0x88,
	0x8C,
	0x90,
	0x98,
	0x9C,
	0xA0,
	0xA4,
	0xA8,
	0xAC,
	0xB0,
	0xB4,
	0xB8,
	0xC0,
	0xC4,
	0xC8,
	0xCC,
	0xD0,
	0xD8,
	0xDC,
	0xE0,
	0xE4,
};
const int chipNumVoices[] = { // number of voices (aka channels. Not to be confused with left-right stereo channels) in each chip. Used to determine how many midi tracks to create for each chip, and when indexing through tracks. The index of each entry corresponds to Chip ID. TODO: finish.
	4, // SN76489
	9, // YM2413
	6, // YM2612
	8, // YM2151
	16, // SegaPCM
	8, // RF5C68
	6, // YM2203
	16, // YM2608
	16, // YM2610(B). TODO & WARNING: The YM2610 have YM2610B have different numbers of voices! I need to figure out how to handle this. Maybe I should have the amount of tracks match YM2610B by default, then if the chip is just a YM2610, delete the extra empty tracks after the midi has been fully constructed. YM2610 has 14 voices, YM2610B has 16 voices.
	11, // YM3812
	11, // YM3526
	9+1, // Y8950. FM+ADPCM. NOTE: assuming ADPCM is one voice.
	20, // YMF262. NOTE: There are multiple different settings for amount of voices, and what each of those voices do. 20 is the maximum possible number of voices.
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	1, // MSM6258
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
}; 
const vector<string> chipNames = { // The index of each entry corresponds to Chip ID.
	"SN76489",
	"YM2413",
	"YM2612",
	"YM2151",
	"SegaPCM",
	"RF5C68",
	"YM2203",
	"YM2608",
	"YM2610(B)",
	"YM3812",
	"YM3526",
	"Y8950",
	"YMF262",
	"YMF278B",
	"YMF271",
	"YMZ280B",
	"RF5C164",
	"PWM",
	"AY8910",
	"GB DMG",
	"NES APU",
	"MultiPCM",
	"uPD7759",
	"MSM6258",
	"MSM6295",
	"K051649",
	"K054539",
	"HuC6280",
	"C140",
	"K053260",
	"Pokey",
	"QSound",
	"SCSP",
	"WonderSwan",
	"VSU",
	"SAA1099",
	"ES5503",
	"ES5506",
	"X1-010",
	"C352",
	"GA20",
	"Mikey",
}; 
enum Chip_ID { // chip ID constants
	SN76489,
	YM2413,
	YM2612,
	YM2151,
	SegaPCM,
	RF5C68,
	YM2203,
	YM2608,
	YM2610, // or YM2610B
	YM3812,
	YM3526,
	Y8950,
	YMF262,
	YMF278B,
	YMF271,
	YMZ280B,
	RF5C164,
	PWM,
	AY8910,
	GB_DMG,
	NES_APU,
	MultiPCM,
	uPD7759,
	MSM6258,
	MSM6295,
	K051649,
	K054539,
	HuC6280,
	C140,
	K053260,
	Pokey,
	QSound, // WARNING: this is highlighted blue in my text editor, so it might conflict with a reserved word.
	SCSP,
	WonderSwan,
	VSU,
	SAA1099,
	ES5503,
	ES5506,
	X1_010,
	C352,
	GA20,
	Mikey,
};
const map<const string, const pair<const int, const int>> headerChipSettings = { // {name, {address, size of value in bytes}}
	{"SN76489 feedback", {0x28, 2}},
	{"SN76489 shift register width", {0x2A, 1}},
	{"SN76489 Flags", {0x2B, 1}},
	{"SegaPCM interface register ", {0x3C, 4}},
	{"AY8910 Chip Type", {0x78, 1}},
	{"AY8910 Flags", {0x79, 1}},
	{"YM2203-AY8910 Flags", {0x7A, 1}},
	{"YM2608-AY8910 Flags", {0x7B, 1}},
	{"MSM6258 Flags", {0x94, 1}},
	{"K054539 Flags", {0x95, 1}},
	{"C140 Chip Type", {0x96, 1}},
	{"ES5503 output channel number", {0xD4, 1}},
	{"ES5505/ES5506 amount of output channels", {0xD5, 1}},
	{"C352 clock divider", {0xD6, 1}},
};

class Soundchip {
public:
	virtual ~Soundchip() = default;
	uint8_t chipID = 0xFF; // TODO: set type as Chip_ID enum?
	uint32_t clockRate = 0xFFFFFFFF;
	uint32_t firstMidiTrack = 0;
	Soundchip(){}
	Soundchip(uint8_t in_chipID, uint32_t in_clockRate){
		chipID = in_chipID;
		clockRate = in_clockRate;
	}
	Soundchip(uint8_t in_chipID, uint32_t in_clockRate, uint32_t in_firstMidiTrack){
		chipID = in_chipID;
		clockRate = in_clockRate;
		firstMidiTrack = in_firstMidiTrack;
	}
};
class AY8910_Soundchip : public Soundchip {
public:
	uint8_t AYrevType = 0; // TODO: make an enum for possible AY subtypes? Use enum type to avoid conflict with Chip_ID enum.
	uint8_t flags = 0;
	uint8_t YM2203subChipFlags = 0;
	uint8_t YM2608subChipFlags = 0;
	using Soundchip::Soundchip;
};

class DAC_Stream {
public:
	// Setup Stream Control
	uint8_t streamID = 0xFF;
	uint8_t chipID = 0xFF;
	uint8_t targetPort = 0xFF;
	uint8_t command = 0xFF;
	// Set Stream Data
	uint8_t dataBankID = 0xFF; // TODO: make an enum for this.
	// Set Stream Frequency
	uint32_t frequency = 0xFF; // in Hz
	// Start Stream
	bool started = false; // I've noticed that vgm files like to run "Stop Stream" before the stream has even started. This bool is here to make sure that a stream only gets stopped after it has begun.
	uint16_t blockID = 0xFFFF;
	DAC_Stream(){}
	DAC_Stream(uint8_t in_streamID, uint8_t in_chipID){
		streamID = in_streamID;
		chipID = in_chipID;
	}
	DAC_Stream(uint8_t in_streamID, uint8_t in_chipID, uint8_t in_targetPort){
		streamID = in_streamID;
		chipID = in_chipID;
		targetPort = in_targetPort;
	}
	DAC_Stream(uint8_t in_streamID, uint8_t in_chipID, uint8_t in_targetPort, uint8_t in_command){
		streamID = in_streamID;
		chipID = in_chipID;
		targetPort = in_targetPort;
		command = in_command;
	}
};

#define VGMSAMPLERATE 44100
#define CHIPLISTSIZE 42

#endif	// __VGMPLAYER_HPP__