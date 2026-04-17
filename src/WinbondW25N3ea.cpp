#include "WinbondW25N3ea.h"

W25N3EA::W25N3EA(){};


void W25N3EA::sendData(char * buf, uint32_t len){
  SPI.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, SPI_MODE0));
  digitalWrite(_cs, LOW);
  SPI.transfer(buf, len);
  digitalWrite(_cs, HIGH);
  SPI.endTransaction();
}

int W25N3EA::spiBegin(uint8_t cs, uint8_t mosi, uint8_t miso, uint8_t sck){
  if(!SPI.setMISO(miso)) return 1;
  if(!SPI.setMOSI(mosi)) return 1;
  if(!SPI.setSCK(sck)) return 1;
  // Set the SPI settings
  SPI.begin();
  _cs = cs;
  pinMode(_cs, OUTPUT);
  digitalWrite(_cs, HIGH);
  return 0;
}

int W25N3EA::begin(){
  this->reset();

  // JEDEC-ID auslesen: 0x9F senden, dann 4 Bytes lesen
  uint8_t jedec[4] = {0};
  SPI.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, SPI_MODE0));
  digitalWrite(_cs, LOW);
  delayMicroseconds(100); // Ensure CS is low for at least 1us
  SPI.transfer(W25N_JEDEC_ID); // 0x9F senden
  for (int i = 0; i < 4; i++) {
    jedec[i] = SPI.transfer(0x00); // Antwortbytes lesen
  }
  delayMicroseconds(100); // Ensure CS is low for at least 1us
  digitalWrite(_cs, HIGH);
  SPI.endTransaction();


  //Check the JEDEC ID (Hersteller-ID ist Byte 1, Device-ID ist Byte 2+3)
  if(jedec[1] != WINBOND_MAN_ID){
    return 1;
  }

  uint16_t device_id = (jedec[2] << 8) | jedec[3];
  if(device_id == W25N01GV_DEV_ID){
    this->setStatusReg(W25N_PROT_REG, 0x00);
    this->_model = W25N01GV;
    return 0;
  }
  if(device_id == W25M02GV_DEV_ID){
    this->_model = W25M02GV;
    this->dieSelect(0);
    this->setStatusReg(W25N_PROT_REG, 0x00);
    this->dieSelect(1);
    this->setStatusReg(W25N_PROT_REG, 0x00);
    this->dieSelect(0);
    return 0;
  }
  return 1;
}

void W25N3EA::reset(){
  //TODO check WIP in case of reset during write
  char buf[] = {W25N_RESET};
  this->sendData(buf, sizeof(buf));
  delayMicroseconds(600);	//Trst max time 500uS
}

int W25N3EA::dieSelect(char die){
  //TODO add some type of input validation
  char buf[2] = {W25M_DIE_SELECT, die};
  this->sendData(buf, sizeof(buf));
  this->_dieSelect = die;
  return 0;
}

int W25N3EA::dieSelectOnAdd(uint32_t pageAdd){
  if(pageAdd > this->getMaxPage()) return 1;
  return this->dieSelect(pageAdd / W25N01GV_MAX_PAGE);
}

char W25N3EA::getStatusReg(char reg){
  char buf[3] = {W25N_READ_STATUS_REG, reg, 0x00};
  this->sendData(buf, sizeof(buf));
  return buf[2];
}

void W25N3EA::setStatusReg(char reg, char set){
  char buf[3] = {W25N_WRITE_STATUS_REG, reg, set};
  this->sendData(buf, sizeof(buf));
}

uint32_t W25N3EA::getMaxPage(){
  if (_model == W25M02GV) return W25M02GV_MAX_PAGE;
  if (_model == W25N01GV) return W25N01GV_MAX_PAGE;
  return 0;
}

void W25N3EA::writeEnable(){
  char buf[] = {W25N_WRITE_ENABLE};
  this->sendData(buf, sizeof(buf));
}

void W25N3EA::writeDisable(){
  char buf[] = {W25N_WRITE_DISABLE};
  this->sendData(buf, sizeof(buf));
}

int W25N3EA::blockErase(uint32_t pageAdd){
  if(pageAdd > this->getMaxPage()) return 1;
  this->dieSelectOnAdd(pageAdd);
  char pageHigh = (char)((pageAdd & 0xFF00) >> 8);
  char pageLow = (char)(pageAdd);
  char buf[4] = {W25N_BLOCK_ERASE, 0x00, pageHigh, pageLow};
  this->block_WIP();
  this->writeEnable();
  this->sendData(buf, sizeof(buf));
  return 0;
}

int W25N3EA::bulkErase(){
  int error = 0;
  for(uint32_t i = 0; i < this->getMaxPage(); i+=64){
    if((error = this->blockErase(i)) != 0) return error;
  }
  return 0;
}

int W25N3EA::loadProgData(uint16_t columnAdd, char* buf, uint32_t dataLen){
  if(columnAdd > (uint32_t)W25N_MAX_COLUMN) return 1;
  if(dataLen > (uint32_t)W25N_MAX_COLUMN - columnAdd) return 1;
  char columnHigh = (columnAdd & 0xFF00) >> 8;
  char columnLow = columnAdd & 0xff;
  char cmdbuf[3] = {W25N_PROG_DATA_LOAD, columnHigh, columnLow};
  this->block_WIP();
  this->writeEnable();
  SPI.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, SPI_MODE0));
  digitalWrite(_cs, LOW);
  SPI.transfer(cmdbuf, sizeof(cmdbuf));
  SPI.transfer(buf, dataLen);
  digitalWrite(_cs, HIGH);
  SPI.endTransaction();
  return 0;
}

int W25N3EA::loadProgData(uint16_t columnAdd, char* buf, uint32_t dataLen, uint32_t pageAdd){
  if(this->dieSelectOnAdd(pageAdd)) return 1;
  return this->loadProgData(columnAdd, buf, dataLen);
}

int W25N3EA::loadRandProgData(uint16_t columnAdd, char* buf, uint32_t dataLen){
  if(columnAdd > (uint32_t)W25N_MAX_COLUMN) return 1;
  if(dataLen > (uint32_t)W25N_MAX_COLUMN - columnAdd) return 1;
  char columnHigh = (columnAdd & 0xFF00) >> 8;
  char columnLow = columnAdd & 0xff;
  char cmdbuf[3] = {W25N_RAND_PROG_DATA_LOAD, columnHigh, columnLow};
  this->block_WIP();
  this->writeEnable();
  SPI.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, SPI_MODE0));
  digitalWrite(_cs, LOW);
  SPI.transfer(cmdbuf, sizeof(cmdbuf));
  SPI.transfer(buf, dataLen);
  digitalWrite(_cs, HIGH);
  SPI.endTransaction();
  return 0;
}

int W25N3EA::loadRandProgData(uint16_t columnAdd, char* buf, uint32_t dataLen, uint32_t pageAdd){
  if(this->dieSelectOnAdd(pageAdd)) return 1;
  return this->loadRandProgData(columnAdd, buf, dataLen);
}

int W25N3EA::ProgramExecute(uint32_t pageAdd){
  if(pageAdd > this->getMaxPage()) return 1;
  this->dieSelectOnAdd(pageAdd);
  char pageHigh = (char)((pageAdd & 0xFF00) >> 8);
  char pageLow = (char)(pageAdd);
  this->writeEnable();
  char buf[4] = {W25N_PROG_EXECUTE, 0x00, pageHigh, pageLow};
  this->sendData(buf, sizeof(buf));
  return 0;
}

int W25N3EA::pageDataRead(uint32_t pageAdd){
  if(pageAdd > this->getMaxPage()) return 1;
  this->dieSelectOnAdd(pageAdd);
  char pageHigh = (char)((pageAdd & 0xFF00) >> 8);
  char pageLow = (char)(pageAdd);
  char buf[4] = {W25N_PAGE_DATA_READ, 0x00, pageHigh, pageLow};
  this->block_WIP();
  this->sendData(buf, sizeof(buf));
  return 0;

}

int W25N3EA::read(uint16_t columnAdd, char* buf, uint32_t dataLen){
  if(columnAdd > (uint32_t)W25N_MAX_COLUMN) return 1;
  if(dataLen > (uint32_t)W25N_MAX_COLUMN - columnAdd) return 1;
  char columnHigh = (columnAdd & 0xFF00) >> 8;
  char columnLow = columnAdd & 0xff;
  char cmdbuf[4] = {W25N_READ, columnHigh, columnLow, 0x00};
  this->block_WIP();
  SPI.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, SPI_MODE0));
  digitalWrite(_cs, LOW);
  SPI.transfer(cmdbuf, sizeof(cmdbuf));
  SPI.transfer(buf, dataLen);
  digitalWrite(_cs, HIGH);
  SPI.endTransaction();
  return 0;
}
//Returns the Write In Progress bit from flash.
int W25N3EA::check_WIP(){
  char status = this->getStatusReg(W25N_STAT_REG);
  if(status & 0x01){
    return 1;
  }
  return 0;
}

int W25N3EA::block_WIP(){
  //Max WIP time is 10ms for block erase so 15 should be a max.
  unsigned long tstamp = millis();
  while(this->check_WIP()){
    delay(1);
    if (millis() > tstamp + 15) return 1;
  }
  return 0;
}

int W25N3EA::check_status(){
  return(this->getStatusReg(W25N_STAT_REG));
}
