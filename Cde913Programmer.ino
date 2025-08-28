#include <Wire.h>

#define EEPIP 0x40       // EEPROM programming Status, active high
#define EELOCK 0x20      // Permanently lock EEPROM data, active high
#define EEWRITE 1        // Initiate EEPROM write cycle
#define NumberOfRegs 32  // # of registers in device

char I2cAddress;
char IntelHexData[NumberOfRegs], DeviceRegs[NumberOfRegs];

inline byte ChartoHex(char z) {
  return z <= '9' ? z - '0' : z <= 'F' ? z - 'A' + 10
                                       : z - 'a' + 10;
}

inline byte Doublechar2Hex(char *s) {
  char result;
  result = ChartoHex(s[0]);
  result = result << 4;
  result = result + ChartoHex(s[1]);
  return result;
}

char FindDeviceAddress() {
  char address, error;
  for (address = 1; address < 127; address++) {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.print(address, HEX);
      Serial.println("  !");
      return address;
    } else if (error == 4) {
      Serial.print("Unknow error at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.println(address, HEX);
      return 0;
    }
  }
  Serial.println("No I2C devices found\n");
  return 0;
}

char Read1Byte(char Addr) {
  char c;

  Wire.beginTransmission(I2cAddress);
  Wire.write(Addr | 0x80);  // register address | 0x80 -> i2c byte mode

  Wire.endTransmission(false);  // set to 'false' to prepare for read
  Wire.requestFrom(I2cAddress, 1);
  c = Wire.read();

  return c;
}

char Write1Byte(char Addr, char c) {
  char error;

  Wire.beginTransmission(I2cAddress);
  Wire.write(Addr | 0x80);  // register address | 0x80 -> i2c byte mode

  Wire.write(c);
  error = Wire.endTransmission();  // set to 'false' to prepare for read

  return error;
}

void Write2Device() {
  bool loop_1 = true;
  bool write_data = false;
  uint8_t retval = 0;

  I2cAddress = FindDeviceAddress();

  Serial.print("Writing: ");

  // write config registers
  for (char i = 1; i <= 15; i++) {
    retval += Write1Byte(i, IntelHexData[i]);
  }

  delay(1);

  // write PLL1 registers
  for (char i = 0x1; i <= 6; i++) {
    retval += Write1Byte(i, IntelHexData[i]);
  }

  for (char i = 7; i <= 15; i++) {
    retval += Write1Byte(i, 0);
  }

  for (char i = 0x10; i <= 0x1f; i++) {
    retval += Write1Byte(i, IntelHexData[i]);
  }
//  Serial.print("Retval = ");   Serial.println(retval);

  // check error state
  if (retval == 0) {
    Serial.println("Writing to device completed.");

  } else {
    Serial.println("Write error occured!");
  }
}

void Write2EEPROM() {
  int retval = 0;

  I2cAddress = FindDeviceAddress();

  if ((Read1Byte(0x81) & EELOCK) > 0) {
    Serial.println("EELOCK in register 1 is in high state.");
    Serial.println("New data can no longer be saved to the EEPROM.");
  }
  else {
    // write the EEPROM offset 6 -> 1
    retval += Write1Byte(0x86, IntelHexData[6] | EEWRITE);  // BCOUNT | EEWRITE = 1

    // should technically wait for some time and poll EEPIP, but this program terminates here
    if (retval == 0) {
      Serial.print("Writing to EEPROM started");
      while ((Read1Byte(0x81) & EEPIP) > 0) {
        Serial.print(".");
      }
      Serial.println();
    } else {
      Serial.println("Write Error.");
    }

    Write1Byte(0x86, IntelHexData[6]);  // restore registewr 6 -> clear EEWRITE bit
    Serial.println("Writing to EEPROM completed.");
  }
}

void ReadIntelHEX() {

#define BUFFER_SIZE 16
  char Address, Value;
  char ss[BUFFER_SIZE + 1];

start:

  while (Serial.available() == 0) {}                        //wait for data available
  int size = Serial.readBytesUntil('\r', ss, BUFFER_SIZE);  // read until CR received
  ss[size] = 0;
  //  Serial.println(ss);

  if (strncmp(ss, ":01", 3) == 0) {

    Address = Doublechar2Hex(&ss[5]);

    Value = Doublechar2Hex(&ss[9]);  //

    if (Address < NumberOfRegs) IntelHexData[Address] = Value;

  } else if (strncmp(ss, ":00", 3) == 0) {
    Serial.println("End of IntelHex file reached");

    // overwrite values assigned by  TI Clock Pro
    IntelHexData[1] = IntelHexData[1] & ~EELOCK;  // Disables accidentally permanently locking EEPROM data

    for (char i = 0; i < NumberOfRegs; i++) {
      Serial.print("0x");
      if (i < 16) Serial.print("0");
      Serial.print(i, HEX);
      Serial.print("  0x");
      if (IntelHexData[i] < 16) Serial.print("0");
      Serial.println(IntelHexData[i], HEX);
    }
    return;
  }
  goto start;
}

void ReadDevice() {
  char reg, j;

  I2cAddress = FindDeviceAddress();

  // read registwers
  for (char j = 0; j < NumberOfRegs; j++) {
    DeviceRegs[j] = Read1Byte(j);
  }

  for (char j = 0; j < NumberOfRegs; j++) {
    Serial.print("0x");
    if (j < 16) Serial.print("0");
    Serial.print(j, HEX);
    Serial.print("  0x");
    if (DeviceRegs[j] < 16) Serial.print("0");
    Serial.println(DeviceRegs[j], HEX);
  }

  //  Serial.println("\n\nReady to read IntelHEX file");
}

void ClearScreen() {
  Serial.print("\x1b[H");   // Cursor hom
  Serial.print("\x1b[2J");  // Clear screen
}

void Help() {
  ClearScreen();
  Serial.println("CDCE913 programmer commands:\n");
  Serial.println("F -> Read Intel Hex file containing the register settings");
  Serial.println("W -> Write to Device");
  Serial.println("R -> Read from Device");
  Serial.println("E -> Write to EEPROM");
  Serial.println("H -> Help, shows the available commands");
}

void setup() {
  Wire.begin();
  Serial.begin(9600);
  while (!Serial) {}
}

void loop() {
  //  I2cAddress = FindDeviceAddress();

  if (Serial.available() > 0) {
    switch (Serial.read()) {
      case 'f':
      case 'F':
        //        Serial.println("R");
        Serial.println("\nReady to read IntelHEX file");
        Serial.println("Send intel hex file via serial line to programmer or");
        Serial.println("Copy and paste the contents of it from an editor to the terminal window");
        ReadIntelHEX();
        break;
      case 'w':
      case 'W':
        // Write data to device
        Write2Device();
        break;
      case 'e':
      case 'E':
        // Write data to EEPROM
        Write2EEPROM();
        break;
      case 'h':
      case 'H':
        Help();
        break;
      case 'r':
      case 'R':
        ReadDevice();
        break;
      default:
        Help();
    }
  }
}
