/*
    This example reads audio data from the on-board PDM microphones, and prints
    out the samples to the Serial console. The Serial Plotter built into the
    Arduino IDE can be used to plot the audio data (Tools -> Serial Plotter)
    Circuit:
    - Arduino Nicla Vision, or
    - Arduino Nano 33 BLE board, or
    - Arduino Nano RP2040 Connect, or
    - Arduino Portenta H7 board plus Portenta Vision Shield
    This example code is in the public domain.
  */
  // #define PIN_PDM_DIN  11   // example: DATA from mic -> MCU pin 2
  // #define PIN_PDM_CLK  6   // MCU drives CLK -> mic CLK
  // #define PIN_PDM_PWR  -1   // optional: MCU pin to enable mic Vdd

  
  #include <PDM.h>

  // default number of output channels
  static const char channels = 1;

  // default PCM output frequency
  static const int frequency = 16000;

  // Buffer to read samples into, each sample is 16-bits
  short sampleBuffer[512];

  // Number of audio samples read
  volatile int samplesRead;

  // Blinking 
  bool state = false;
  int timeStart = 0;

  int LEDB = 8;
  int TESTPIN = 12;
  // int testpinval = 0;


  void setup() {
    Serial.begin(9600);
    pinMode(LEDB, OUTPUT);
    PDM.setPins(11, 6, -1); //set mic pins
    // pinMode(TESTPIN, OUTPUT);
    // digitalWrite(TESTPIN, HIGH);

    while (!Serial);

    // Configure the data receive callback
    PDM.onReceive(onPDMdata);

    // Optionally set the gain
    // Defaults to 20 on the BLE Sense and 24 on the Portenta Vision Shield
    // PDM.setGain(30);

    // Initialize PDM with:
    // - one channel (mono mode)
    // - a 16 kHz sample rate for the Arduino Nano 33 BLE Sense
    // - a 32 kHz or 64 kHz sample rate for the Arduino Portenta Vision Shield
    if (!PDM.begin(channels, frequency)) {
      Serial.println("Failed to start PDM!");
      while (1);
    }


  }

  void loop() {

    // digitalWrite(TESTPIN, testpinval);
    // testpinval = !testpinval;
    // delay(1000);

    // Wait for samples to be read
    if (samplesRead) {

      // Print samples to the serial monitor or plotter
      for (int i = 0; i < samplesRead; i++) {
        if (channels == 2) {
          Serial.print("L:");
          Serial.print(sampleBuffer[i]);
          Serial.print(" R:");
          i++;
        }
        Serial.println(sampleBuffer[i]);
      }

      // Clear the read count
      samplesRead = 0;

      if (millis() - timeStart > sampleBuffer[2]) {
        digitalWrite(LEDB, state);
        state = !state;
      }
    }
  }

  /**
    Callback function to process the data from the PDM microphone.
    NOTE: This callback is executed as part of an ISR.
    Therefore using `Serial` to print messages inside this function isn't supported.
  * */
  void onPDMdata() {
    // Query the number of available bytes
    int bytesAvailable = PDM.available();

    // Read into the sample buffer
    PDM.read(sampleBuffer, bytesAvailable);

    // 16-bit, 2 bytes per sample
    samplesRead = bytesAvailable / 2;
  }