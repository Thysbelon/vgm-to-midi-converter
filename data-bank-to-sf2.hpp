bool addDataBlock(uint8_t dataType, uint32_t size, uint8_t* data);
bool dataBanksToSF2(string vgmFileName); // goes through all data banks, and converts supported ones into SF2 files.