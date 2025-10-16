#include <Adafruit_SPIFlash.h>
#include <Adafruit_TinyUSB.h>

Adafruit_FlashTransport_QSPI flashTransport;
Adafruit_SPIFlash flash(&flashTransport);

bool writeFlashBlocking(uint32_t, char *, size_t);

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  //initialize the flash chip 
  bool flashResult = flash.begin();
  
  if (!flashResult)
  {
    Serial.println("problem with initialization of flash chip");
    while (1);
  }

  uint32_t addr = 0x000000; 
  char data[] = "hello world";
  size_t length = sizeof(data);

  flashResult = writeFlashBlocking(addr, data, length);

  char buffer[length] = {0};
  flash.readBuffer(addr, (uint8_t*)buffer, length);
  Serial.println(buffer);
}

bool writeFlashBlocking(uint32_t addr, char * data, size_t len)
{
    //flash needs to be erased before written
    bool flashResult = flash.eraseSector(addr / 4096); 
    flash.waitUntilReady();
    Serial.println("erased a flash sector of 4KB");

    if (!flashResult)
    {
        return false;
    }

    //write
    flashResult = flash.writeBuffer(addr, (uint8_t*)data, len);
    flash.waitUntilReady();
    Serial.println("done writing data to flash");

    return flashResult;
}

void loop() {
}