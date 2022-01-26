/*
  // TLV6510 DAC#1 Pinout;
  1.  Dgnd - GND                  20. Dvdd -> 3.3V
  2.  Din -> SPI MOSI(11) Dac#1   19. Dout  -> NC / Multiple; Din Dac#2
  3.  SCK -> SPI SCK (13)         18. LDAC -> GND
  4.  FS -> CS (9)                17. Mode -> 3.3V (uC mode)
  5.  Pre -> 3.3V                 16. Ref -> 3.3V
  6.  OutE                        15. OutD
  7.  OutF                        14. OutC
  8.  OutG                        13. OutB
  9.  OutH                        12. OutA
  10. AGnd -> Gnd                 11. AVdd -> 3.3V

  DAC#2;
  Dac#1 Dout(19) -> Dac#2 Din(2)
  
  https://github.com/thomasvanta/TLV5610/
  https://www.ti.com/product/TLV5610

  LDAC == LOW : Asynchronous mode, not possible to write all registers at once
  PRE == HIGH : Preset register disabled

*/

#include <SPI.h>
#define SPI_SPEED 45000000 // dual dac setup allows for max 45Mhz on breadboard, single dac does 70Mhz. Officially; 30Mhz

#define CS  9 // Use any digital pin for !CS (chip select) -> Din of Dac#1

#define LOOPS 500.0
#define DACMAX 4095
#define DAC_CHANNEL_COUNT 16 // 8 is max currently
#define DAC_NUM_DEVICES 2

#define DAC_DOUT_ENABLE 0x8008 // 1000000000001000 CTRL0; [15-12] 1000 [11 - 5] 0000000 [4-0] 01000
#define DAC_DOUT_DISABLE 0x8000 // 1000000000000000 CTRL0; [15-12] 1000 [11 - 5] 0000000 [4-0] 00000

#define DAC_FAST 0x900F // CTRL1; Fast; [15-12] 1001 [11-8]  0000 [7-4] 0000 [3-0] 1111 (2.89 ms)
#define DAC_SLOW 0x9000 // Slow ; [15-12] 1001 [11-0] 0000; (2.87ms)


// SINE STUFF
unsigned long timeOld = 0;
unsigned long totalLoops = 0;
int count = 0;
const word N = 512; //number of samples per period
word sinvalues[N]; // lookup table
const float pi = 3.141592653589793;
word icount = 0;

void setup() {
  Serial.begin(115200);
  delay(10);
  SPI.begin();
  SPISettings settingsA(SPI_SPEED, MSBFIRST, SPI_MODE3); // spi speed max : 30Mhz (50Mhz seems to work also)
  SPI.beginTransaction(settingsA);
  pinMode(CS, OUTPUT);
  digitalWrite(CS, HIGH);

  // create sine table
  for (word i = 0; i < N; i++) {
    sinvalues[i] = floor((DACMAX / 2) * (1 - cos(i * 2 * pi / N))); //my sine has to start from zero value to 4096 value (12-bit range)
  }


  setDACctrl(DAC_DOUT_ENABLE); // Enable allows for multiple DAC's daisy chained
  setDACctrl(DAC_SLOW);
  Serial.println("Dac configured" );
  timeOld = micros();
}

void loop() {
  count = sinvalues[icount];

  uint16_t values[DAC_CHANNEL_COUNT];
  for (int i = 0; i < DAC_CHANNEL_COUNT; i++) {
    values[i] = count;
  }
  updateMultipleDevices(values);
  if (count == DACMAX / 2) {
    totalLoops ++;

    if (totalLoops == LOOPS) {
      totalLoops = 0;
      unsigned long timeElapsed = millis() - timeOld;
      timeOld = micros();
      Serial.print("frequency; " );
      Serial.print(3.0 / (float(timeElapsed / LOOPS) / 1000000000.0), 1); // print loopspeed in Hz
      Serial.println(" Hz");
    }
  }
  icount = (icount + 1) & N - 1;
}

void updateMultipleDevices(uint16_t values[]) {
  int dev;
  int channelsPerDevice = DAC_CHANNEL_COUNT / DAC_NUM_DEVICES;

  for (int i = 0; i < channelsPerDevice; i++) {
    digitalWriteFast (CS, LOW);
    for (dev = DAC_NUM_DEVICES - 1; dev >= 0; --dev) { //
      uint16_t dataWord = (i << 12 ) | values[(channelsPerDevice * dev) + i];

      SPI.transfer(highByte(dataWord)); // upper 8 bits
      SPI.transfer(lowByte (dataWord) ); // lower 8 bits
    }
    digitalWriteFast(CS, HIGH); //
    delayMicroseconds(1);
  }
}

void setDACctrl(uint16_t _cmd) {
  int dev;
  uint16_t dataWord = _cmd;
  digitalWrite (CS, LOW);//
  for (dev = DAC_NUM_DEVICES - 1; dev >= 0; --dev) { // TODO: exclusion of Data out enable in case of last IC
    SPI.transfer(highByte(dataWord)); // upper 8 bits
    SPI.transfer (lowByte (dataWord) ); // lower 8 bits
  }
  digitalWriteFast(CS, HIGH); //
  delayMicroseconds(1);
}
