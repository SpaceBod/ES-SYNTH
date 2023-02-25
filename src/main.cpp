#include <Arduino.h>
#include <U8g2lib.h>
#include <HardwareTimer.h>

constexpr uint32_t samplingFreq = 22050; // Hz
constexpr double twelfthRootOfTwo = pow(2.0, 1.0/12.0); // 12th root of 2
constexpr uint32_t calculateStepSize(float frequency) {
    return static_cast<uint32_t>((pow(2, 32) * frequency) / samplingFreq);
}
constexpr uint32_t stepSizes[] = {
    calculateStepSize(261.63f), // C
    calculateStepSize(277.18f), // C#
    calculateStepSize(293.66f), // D
    calculateStepSize(311.13f), // D#
    calculateStepSize(329.63f), // E
    calculateStepSize(349.23f), // F
    calculateStepSize(369.99f), // F#
    calculateStepSize(392.00f), // G
    calculateStepSize(415.30f), // G#
    calculateStepSize(440.00f), // A
    calculateStepSize(466.16f), // A#
    calculateStepSize(493.88f)  // B
};

//SOUND CONSTANTS
  int volume = 100;

//Constants
  const uint32_t interval = 100; //Display update interval
  volatile uint32_t currentStepSize;
//Pin definitions
  //Row select and enable
  const int RA0_PIN = D3;
  const int RA1_PIN = D6;
  const int RA2_PIN = D12;
  const int REN_PIN = A5;

  //Matrix input and output
  const int C0_PIN = A2;
  const int C1_PIN = D9;
  const int C2_PIN = A6;
  const int C3_PIN = D1;
  const int OUT_PIN = D11;

  //Audio analogue out
  const int OUTL_PIN = A4;
  const int OUTR_PIN = A3;
  
  //Joystick analogue in
  const int JOYY_PIN = A0;
  const int JOYX_PIN = A1;

  //Output multiplexer bits
  const int DEN_BIT = 3;
  const int DRST_BIT = 4;
  const int HKOW_BIT = 5;
  const int HKOE_BIT = 6;

//Display driver object
U8G2_SSD1305_128X32_NONAME_F_HW_I2C u8g2(U8G2_R0);

//Function to set outputs using key matrix
void setOutMuxBit(const uint8_t bitIdx, const bool value) {
      digitalWrite(REN_PIN,LOW);
      digitalWrite(RA0_PIN, bitIdx & 0x01);
      digitalWrite(RA1_PIN, bitIdx & 0x02);
      digitalWrite(RA2_PIN, bitIdx & 0x04);
      digitalWrite(OUT_PIN,value);
      digitalWrite(REN_PIN,HIGH);
      delayMicroseconds(2);
      digitalWrite(REN_PIN,LOW);
}

uint8_t readCols(){
  uint8_t colVals = 0;
  // Read column values
  colVals |= (digitalRead(C0_PIN) << 0);
  colVals |= (digitalRead(C1_PIN) << 1);
  colVals |= (digitalRead(C2_PIN) << 2);
  colVals |= (digitalRead(C3_PIN) << 3);
  // Serial.println(colVals, BIN);
  return colVals;
}

void setRow(uint8_t rowIdx){
  // Disable row select enable
  digitalWrite(REN_PIN, LOW);

  // Set row select pins
  digitalWrite(RA0_PIN, rowIdx & 0x01);
  digitalWrite(RA1_PIN, (rowIdx >> 1) & 0x01);
  digitalWrite(RA2_PIN, (rowIdx >> 2) & 0x01);

  // Enable row select enable
  digitalWrite(REN_PIN, HIGH);
}

// SAW
// void sampleISR() {
//   static uint32_t phaseAcc = 0;
//   phaseAcc += currentStepSize;
//   int32_t Vout = (phaseAcc >> 24) - 128;
//   Vout = (Vout * volume) / 100;
//   analogWrite(OUTR_PIN, Vout + 128);
// }

// TRIANGLE
void sampleISR() {
  // Update the phase accumulator by the current step size
  static uint32_t phaseAcc = 0;
  phaseAcc += currentStepSize;
  // Calculate the output voltage based on the current phase
  int32_t Vout;
  if (phaseAcc < UINT32_MAX / 2) {
    Vout = (phaseAcc >> 23) - 256; // Increasing slope
  } else {
    Vout = ((UINT32_MAX - phaseAcc) >> 23) - 256; // Decreasing slope
  }
  Vout = (Vout * volume) / 100;
  analogWrite(OUTR_PIN, Vout + 128);
}

void setup() {
  //Set pin directions
  pinMode(RA0_PIN, OUTPUT);
  pinMode(RA1_PIN, OUTPUT);
  pinMode(RA2_PIN, OUTPUT);
  pinMode(REN_PIN, OUTPUT);
  pinMode(OUT_PIN, OUTPUT);
  pinMode(OUTL_PIN, OUTPUT);
  pinMode(OUTR_PIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  pinMode(C0_PIN, INPUT);
  pinMode(C1_PIN, INPUT);
  pinMode(C2_PIN, INPUT);
  pinMode(C3_PIN, INPUT);
  pinMode(JOYX_PIN, INPUT);
  pinMode(JOYY_PIN, INPUT);

  //Initialise display
  setOutMuxBit(DRST_BIT, LOW);  //Assert display logic reset
  delayMicroseconds(2);
  setOutMuxBit(DRST_BIT, HIGH);  //Release display logic reset
  u8g2.begin();
  setOutMuxBit(DEN_BIT, HIGH);  //Enable display power supply

  //Initialise UART
  Serial.begin(9600);
  Serial.println("Hello World");
  
  for (int i = 0; i < 12; i++) {
  Serial.print(stepSizes[i]);
  Serial.print(" ");
  }
  // Create timer and configure
  TIM_TypeDef *Instance = TIM1;
  HardwareTimer *sampleTimer = new HardwareTimer(Instance);
  sampleTimer->setOverflow(22050, HERTZ_FORMAT);
  sampleTimer->attachInterrupt(sampleISR);
  sampleTimer->resume();
}

void loop() {
  const char* notes[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#",  "A", "A#", "B"};
  std::string keyLabel;
  uint8_t keyArray[7];
  int key;

  // Loop over rows 0 to 2
  key = -1;
  for (uint8_t row = 0; row < 3; row++) {
    setRow(row);
    delayMicroseconds(3); // wait for column lines to settle
    keyArray[row] = readCols();
    if (keyArray[row] == 0b1110) {
      key = (4*row);
    }
    else if (keyArray[row] == 0b1101) {
      key = (4*row)+1;
    }
    else if (keyArray[row] == 0b1011) {
      key = (4*row)+2;
    }
    else if (keyArray[row] == 0b0111) {
      key = (4*row)+3;
    }
  }

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.setCursor(2,20);
  if (key == -1){
    u8g2.print("-");
    u8g2.setCursor(2,30);
    currentStepSize = 0;
    u8g2.sendBuffer(); 
  }
  else {
    u8g2.print(notes[key]);
    u8g2.setCursor(2,30);
    u8g2.print(stepSizes[key]);
    currentStepSize = stepSizes[key];
    u8g2.sendBuffer(); 
  }
  
  delay(100);
}