#include "sf2cute/include/sf2cute.hpp"
#include "vgm-file.hpp" // for dataBankID enum.
extern "C" {
	#include "adpcm/adpcm.h" // from mdxtools
}
#include <iostream> // to write sf2
#include <fstream>

using namespace sf2cute;

class DataBlock {
public:
	uint8_t dataType = 0xFF; // this should match the data bank ID
	uint32_t size = 0; // the size of "data", in bytes.
	//vector<uint8_t> data = vector<uint8_t>(1,0xFF);
	vector<uint8_t> data;
	DataBlock(){}
	DataBlock(uint8_t in_dataType){
		dataType = in_dataType;
	}
	DataBlock(uint8_t in_dataType, uint32_t in_size){
		dataType = in_dataType;
		size = in_size;
	}
	DataBlock(uint8_t in_dataType, uint32_t in_size, vector<uint8_t> in_data){
		dataType = in_dataType;
		size = in_size;
		data = in_data;
	}
	DataBlock(uint8_t in_dataType, uint32_t in_size, uint8_t* in_data){
		dataType = in_dataType;
		size = in_size;
		data = vector<uint8_t>(in_data, in_data + size);
	}
};

class DataBank { // gather data blocks found in the vgm file. Then assemble them into an SF2 after the entire vgm file has been read.
public:
	uint8_t dataBankID = 0xFF; // identifies which system the Data Bank is for. Should match its index position in dataBankCollection.
	//vector<DataBlock> blocks = vector<DataBlock>(1,DataBlock());
	vector<DataBlock> blocks;
	DataBank(){}
	DataBank(uint8_t in_dataBankID){
		dataBankID = in_dataBankID;
	}
	DataBank(uint8_t in_dataBankID, vector<DataBlock> in_blocks){
		dataBankID = in_dataBankID;
		blocks = in_blocks;
	}
};

//vector<DataBank> dataBankCollection;
DataBank dataBankCollection[0xE2]; // current highest valid data bank ID is 0xE1.

vector<int16_t> decodeMSM6258_ADPCMsample(vector<uint8_t> MSM6258_ADPCM_data){ // from mdxtools
	//f->samples[i].decoded_data = cur_sample;
	//int curSample = 0;
	//f->samples[i].num_samples  = f->samples[i].len * 2;
	const int inputSize = MSM6258_ADPCM_data.size();
	vector<int16_t> outputData(inputSize * 2, 0);
	struct adpcm_status st;
	adpcm_init(&st);
	for(int i = 0; i < inputSize; i++) {
		uint8_t c  = MSM6258_ADPCM_data[i];
		int16_t d1 = adpcm_decode(c & 0x0f, &st);
		int16_t d2 = adpcm_decode(c >> 4, &st);
		//*(cur_sample++) = d1;
		//*(cur_sample++) = d2;
		outputData[i * 2] = ((int16_t)d1) << 4; // convert from sint12 sample to sint16 sample.
		outputData[i * 2 + 1] = ((int16_t)d2) << 4; // convert from sint12 sample to sint16 sample.
		// TODO: figure out if there's a way to tell if an MSM6258 sample is in sint12 or some other format.
	}
	return outputData;
}

bool addDataBlock(uint8_t dataType, uint32_t size, uint8_t* data){ // takes data block info as input. uses that info to construct a data block object. If the right type of data bank already exists, add that block to the bank. If not, create the right type of data bank, then add the block to it.
	DataBlock newBlock(dataType, size, data);
	dataBankCollection[dataType].dataBankID = dataType; // TODO: would it be more or less performant to put this under an "if" statement?
	dataBankCollection[dataType].blocks.push_back(newBlock);
	return true;
}

bool MSM6258_to_SF2(string vgmFileName){
	const int BASE_NOTE = 0;
	DataBank MSM6258_bank = dataBankCollection[0x4];
	// create SF2. create a single instrument for the sf2.
	SoundFont sf2;
	sf2.set_sound_engine("EMU8000"); // ?
	std::shared_ptr<SFInstrument> MSM6258_instrument = sf2.NewInstrument("MSM6258 Instrument");
	// for (DataBlock curBlock : MSM6258_bank.blocks) {
	for (int i = 0; i < MSM6258_bank.blocks.size(); i++) {
		// structure this like pdx2sf2. the current block is converted to a sample, then that sample is added to the single instrument.
		std::vector<int16_t> converted_sample_data = decodeMSM6258_ADPCMsample(MSM6258_bank.blocks[i].data);
		std::shared_ptr<SFSample> my_sample_pointer = sf2.NewSample(
			"Sample " + to_string(i + 1), // name
			converted_sample_data, // sample data
			0, // start loop
			uint32_t( converted_sample_data.size() ), // end loop
			15600, // sample rate
			60, // root key
			0, // microtuning
			std::weak_ptr<SFSample>(), // pointer to other sample in a left-right stereo sample pair. NULL here because these are all mono samples.
			SFSampleLink::kMonoSample);

		SFInstrumentZone instrument_zone(my_sample_pointer);
		instrument_zone.SetGenerator(SFGeneratorItem(SFGenerator::kKeyRange, RangesType(BASE_NOTE + i, BASE_NOTE + i)));
		instrument_zone.SetGenerator(SFGeneratorItem(SFGenerator::kOverridingRootKey, BASE_NOTE + i));
		// add zone to instrument here.
		MSM6258_instrument->AddZone(instrument_zone);
	}
	// create a single preset. Add the instrument to the preset.
	std::shared_ptr<SFPreset> preset_50 = sf2.NewPreset(
		"MSM6258 Preset", 0, 0,
		std::vector<SFPresetZone>{
			SFPresetZone(MSM6258_instrument)
		});
	try {
		std::ofstream ofs(vgmFileName + ".sf2", std::ios::binary);
		sf2.Write(ofs);
		return 0;
	}
	catch (const std::fstream::failure & e) {
		// File output error.
		std::cerr << e.what() << std::endl;
		return 1;
	}
	catch (const std::exception & e) {
		// Other errors.
		// For example: Too many samples.
		std::cerr << e.what() << std::endl;
		return 1;
	}
}

bool dataBanksToSF2(string vgmFileName){
	if (dataBankCollection[0x4].dataBankID == 0x4)
		MSM6258_to_SF2(vgmFileName);
	return true;
}