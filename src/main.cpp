#include <Arduino.h>
#include <U8g2lib.h>
#include <HardwareTimer.h>
#include <STM32FreeRTOS.h>
#include <utility>
#include <vector>
#include <ES_CAN.h>

// Calculate step sizes and frequencies during compilation
constexpr uint32_t samplingFreq = 22050;                  // Hz
constexpr double twelfthRootOfTwo = pow(2.0, 1.0 / 12.0); // 12th root of 2

// Returns frequency for given note
constexpr uint32_t calculateFreq(float semiTone)
{
  return static_cast<uint32_t>(440.00f * std::pow(2.00f, static_cast<float>(semiTone) / 12.0f));
}
// Returns step size from note
constexpr uint32_t calculateStepSize(float frequency)
{
  return static_cast<uint32_t>((pow(2, 32) * frequency) / samplingFreq);
}
// 2 - 8 Octaves of step sizes - super long
constexpr uint32_t stepSizes[] = {
  calculateStepSize(calculateFreq(-33)),// C2
  calculateStepSize(calculateFreq(-32)),// C#2
  calculateStepSize(calculateFreq(-31)),// D2
  calculateStepSize(calculateFreq(-30)),// D#2
  calculateStepSize(calculateFreq(-29)),// E2
  calculateStepSize(calculateFreq(-28)),// F2
  calculateStepSize(calculateFreq(-27)),// F#2
  calculateStepSize(calculateFreq(-26)),// G2
  calculateStepSize(calculateFreq(-25)),// G#2
  calculateStepSize(calculateFreq(-24)),// A2
  calculateStepSize(calculateFreq(-23)),// A#2
  calculateStepSize(calculateFreq(-22)),// B2
  calculateStepSize(calculateFreq(-21)),// C3
  calculateStepSize(calculateFreq(-20)),// C#3
  calculateStepSize(calculateFreq(-19)),// D3
  calculateStepSize(calculateFreq(-18)),// D#3
  calculateStepSize(calculateFreq(-17)),// E3
  calculateStepSize(calculateFreq(-16)),// F3
  calculateStepSize(calculateFreq(-15)),// F#3
  calculateStepSize(calculateFreq(-14)),// G3
  calculateStepSize(calculateFreq(-13)),// G#3
  calculateStepSize(calculateFreq(-12)),// A3
  calculateStepSize(calculateFreq(-11)),// A#3
  calculateStepSize(calculateFreq(-10)),// B3
  calculateStepSize(calculateFreq(-9)), // C4
  calculateStepSize(calculateFreq(-8)), // C#4
  calculateStepSize(calculateFreq(-7)), // D4
  calculateStepSize(calculateFreq(-6)), // D#4
  calculateStepSize(calculateFreq(-5)), // E4
  calculateStepSize(calculateFreq(-4)), // F4
  calculateStepSize(calculateFreq(-3)), // F#4
  calculateStepSize(calculateFreq(-2)), // G4
  calculateStepSize(calculateFreq(-1)), // G#4
  calculateStepSize(calculateFreq(0)), // A4
  calculateStepSize(calculateFreq(1)), // A#4
  calculateStepSize(calculateFreq(2)), // B4
  calculateStepSize(calculateFreq(3)), // C5
  calculateStepSize(calculateFreq(4)), // C#5
  calculateStepSize(calculateFreq(5)), // D5
  calculateStepSize(calculateFreq(6)), // D#5
  calculateStepSize(calculateFreq(7)), // E5
  calculateStepSize(calculateFreq(8)), // F5
  calculateStepSize(calculateFreq(9)), // F#5
  calculateStepSize(calculateFreq(10)), // G5
  calculateStepSize(calculateFreq(11)), // G#5
  calculateStepSize(calculateFreq(12)), // A5
  calculateStepSize(calculateFreq(13)), // A#5
  calculateStepSize(calculateFreq(14)), // B5
  calculateStepSize(calculateFreq(15)), // C6
  calculateStepSize(calculateFreq(16)), // C#6
  calculateStepSize(calculateFreq(17)), // D6
  calculateStepSize(calculateFreq(18)), // D#6
  calculateStepSize(calculateFreq(19)), // E6
  calculateStepSize(calculateFreq(20)), // F6
  calculateStepSize(calculateFreq(21)), // F#6
  calculateStepSize(calculateFreq(22)), // G6
  calculateStepSize(calculateFreq(23)), // G#6
  calculateStepSize(calculateFreq(24)), // A6
  calculateStepSize(calculateFreq(25)), // A#6
  calculateStepSize(calculateFreq(26)), // B6
  calculateStepSize(calculateFreq(27)), // C7
  calculateStepSize(calculateFreq(28)), // C#7
  calculateStepSize(calculateFreq(29)), // D7
  calculateStepSize(calculateFreq(30)), // D#7
  calculateStepSize(calculateFreq(31)), // E7
  calculateStepSize(calculateFreq(32)), // F7
  calculateStepSize(calculateFreq(33)), // F#7
  calculateStepSize(calculateFreq(34)), // G7
  calculateStepSize(calculateFreq(35)), // G#7
  calculateStepSize(calculateFreq(36)), // A7
  calculateStepSize(calculateFreq(37)), // A#7
  calculateStepSize(calculateFreq(38)), // B7
  calculateStepSize(calculateFreq(39)), // C8
  calculateStepSize(calculateFreq(40)), // C#8
  calculateStepSize(calculateFreq(41)), // D8
  calculateStepSize(calculateFreq(42)), // D#8
  calculateStepSize(calculateFreq(43)), // E8
  calculateStepSize(calculateFreq(44)), // F8
  calculateStepSize(calculateFreq(45)), // F#8
  calculateStepSize(calculateFreq(46)), // G8
  calculateStepSize(calculateFreq(47)), // G#8
  calculateStepSize(calculateFreq(48)), // A8
  calculateStepSize(calculateFreq(49)), // A#8
  calculateStepSize(calculateFreq(50))  // B8
};

// Audio definitions
struct Node {
  uint32_t data = 0;
  Node* next = nullptr;
};

struct LinkedList{
  Node* head = nullptr;
  Node* tail = nullptr;
};

const uint32_t interval = 100; // Display update interval
volatile LinkedList currentStepSizes;

const char *notes[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
const char *keys[12] = {};
const char *waves[4] = {"Saw", "Square", "Triangle", "Sine"};
const char *effects[4] = {"Clean", "Vibrato", "Octave", "None"};
const char *canModes[3] = {"Master", "Send 1", "Send 2"};

volatile int32_t sample = 0;
const int TABLE_SIZE = 1028;
float sinTable[TABLE_SIZE];

// Key Matrix
volatile uint8_t keyArray[4];
SemaphoreHandle_t keyArrayMutex;
volatile int pressedKeys;

// CAN Message
volatile uint32_t prev_message[2] = {0, 0};
volatile uint32_t cur_message[2] = {0, 0};
volatile int octaveRX[2] = {0, 0};

// Knob Variables
volatile int volume = 6;
volatile int waveform = 0;
volatile int effect = 0;
volatile int canMode = 0;
volatile bool showCAN = 0;

// Octave Settings
volatile int octaveSelect = 4;
const int MIN_OCT = 2;
const int MAX_OCT = 8;
volatile bool OctToggle = false;

// Pitch Bend
volatile float pitchBend = 1;
float calZero = 0;
volatile float vibrato = 0;
volatile bool vibToggle = false;

// Pin definitions
// Row select and enable
const int RA0_PIN = D3;
const int RA1_PIN = D6;
const int RA2_PIN = D12;
const int REN_PIN = A5;

// Matrix input and output
const int C0_PIN = A2;
const int C1_PIN = D9;
const int C2_PIN = A6;
const int C3_PIN = D1;
const int OUT_PIN = D11;

// Audio analogue out
const int OUTL_PIN = A4;
const int OUTR_PIN = A3;

// Joystick analogue in
const int JOYY_PIN = A0;
const int JOYX_PIN = A1;

// Output multiplexer bits
const int DEN_BIT = 3;
const int DRST_BIT = 4;
const int HKOW_BIT = 5;
const int HKOE_BIT = 6;

// Display driver object
U8G2_SSD1305_128X32_NONAME_F_HW_I2C u8g2(U8G2_R0);

// CAN Variables
QueueHandle_t msgInQ;
QueueHandle_t msgOutQ;
uint8_t RX_Message[8] = {0};
uint8_t TX_Message[8] = {0};
uint8_t prevTX[8] = {0};
SemaphoreHandle_t CAN_TX_Semaphore;

// Function to set outputs using key matrix
void setOutMuxBit(const uint8_t bitIdx, const bool value)
{
  digitalWrite(REN_PIN, LOW);
  digitalWrite(RA0_PIN, bitIdx & 0x01);
  digitalWrite(RA1_PIN, bitIdx & 0x02);
  digitalWrite(RA2_PIN, bitIdx & 0x04);
  digitalWrite(OUT_PIN, value);
  digitalWrite(REN_PIN, HIGH);
  delayMicroseconds(2);
  digitalWrite(REN_PIN, LOW);
}

uint8_t readCols()
{
  uint8_t colVals = 0;
  // Read column values
  colVals |= (digitalRead(C0_PIN) << 0);
  colVals |= (digitalRead(C1_PIN) << 1);
  colVals |= (digitalRead(C2_PIN) << 2);
  colVals |= (digitalRead(C3_PIN) << 3);

  return colVals;
}

void setRow(uint8_t rowIdx)
{
  // Disable row select enable
  digitalWrite(REN_PIN, LOW);
  // Set row select pins
  digitalWrite(RA0_PIN, rowIdx & 0x01);
  digitalWrite(RA1_PIN, (rowIdx >> 1) & 0x01);
  digitalWrite(RA2_PIN, (rowIdx >> 2) & 0x01);
  // Enable row select enable
  digitalWrite(REN_PIN, HIGH);
}

// Prints the contents of a linked list
void printList(volatile LinkedList* list) {
    Node* current = list->head;
    while (current != nullptr) {
        Serial.print(current->data);
        Serial.print(" | ");
        current = current->next;
    }
    Serial.println();
}

void sampleISR()
{
  static uint32_t phase_accs[84] = {};
  switch(waveform) {
    case 0:
      // SAW
      {
        int32_t Vout = 0;
        sample = 0;
        Node* current = currentStepSizes.head;
        int i = 0;
        while (current != nullptr){
          uint32_t stepSize = current->data;
          phase_accs[i] += stepSize;
          Vout = (phase_accs[i] >> 24) - 128;
          sample += Vout;
          current = current->next;
          i += 1;
        }
        sample = sample >> (8 - volume);
        analogWrite(OUTR_PIN, sample/i + 128);
      }
      break;
    case 1:
      // SQUARE
      {
        int32_t Vout = 0;
        sample = 0;
        Node* current = currentStepSizes.head;
        int i = 0;

        while (current != nullptr){
          uint32_t stepSize = current->data;
          phase_accs[i] += stepSize;
          if (phase_accs[i] < UINT32_MAX / 2) {
            Vout = 63;
          }
          else{
            Vout = -64;
          }
        sample += Vout;
        current = current->next;
        i += 1;
        }
        sample = sample * volume / 8;
        analogWrite(OUTR_PIN, sample/i + 64);
      }
      break;
    case 2:
      // TRIANGLE
      {
      int32_t Vout = 0;
      sample = 0;
      Node* current = currentStepSizes.head;
      int i = 0;

      while (current != nullptr){
        uint32_t stepSize = current->data;
        phase_accs[i] += stepSize;
        if (phase_accs[i] < UINT32_MAX / 2) {
          Vout = (phase_accs[i] >> 24);
        }
        else{
          Vout = (-phase_accs[i] >> 24);
        }
      sample += Vout;
      current = current->next;
      i += 1;
      }
      sample = sample * volume / 8;
      analogWrite(OUTR_PIN, sample/i + 128);
    }
      break;
    case 3:
  // SINE
  {
    int32_t Vout = 0;
    sample = 0;
    Node* current = currentStepSizes.head;
    int i = 0;
    while (current != nullptr) {
      uint32_t stepSize = current->data;
      phase_accs[i] += stepSize;
      int index = 1027 * ((float)phase_accs[i]/(float)UINT32_MAX);
      sample += (int32_t)sinTable[index];
      current = current->next;
      i += 1;
    }
    sample = sample * volume / 8;
    analogWrite(OUTR_PIN, sample/i + 128);
  }
  break;
  }
}

class Knob
{
public:
  Knob(int minValue, int maxValue, volatile int *target)
      : m_minValue(minValue), m_maxValue(maxValue), m_target(target) {}

  void increment()
  {
    __atomic_store_n(m_target, min(__atomic_load_n(m_target, __ATOMIC_RELAXED) + 1, m_maxValue), __ATOMIC_RELAXED);
  }

  void decrement()
  {
    __atomic_store_n(m_target, max(__atomic_load_n(m_target, __ATOMIC_RELAXED) - 1, m_minValue), __ATOMIC_RELAXED);
  }

  void update(int keyVal)
  {
    // Increment Value State
    if ((keyVal == 0b10 && m_prevAB == 0b11) ||
        (keyVal == 0b01 && m_prevAB == 0b00))
    {
      increment();
      m_prevTransition = 1;
    }
    // Decrement Value State
    else if ((keyVal == 0b11 && m_prevAB == 0b10) ||
             (keyVal == 0b00 && m_prevAB == 0b01))
    {
      decrement();
      m_prevTransition = -1;
    }
    // Best-Guess States
    else if ((keyVal == 0b00 && m_prevAB == 0b11) ||
             (keyVal == 0b11 && m_prevAB == 0b00) ||
             (keyVal == 0b10 && m_prevAB == 0b01) ||
             (keyVal == 0b01 && m_prevAB == 0b10))
    {
      if (m_prevTransition == 1)
      {
        increment();
        m_prevTransition = 1;
      }
      else if (m_prevTransition == -1)
      {
        decrement();
        m_prevTransition = -1;
      }
    }
    m_prevAB = keyVal;
  }

private:
  int m_minValue;
  int m_maxValue;
  volatile int *m_target;
  int m_prevAB = 0;
  int m_prevTransition = 0;
};

void addNode(LinkedList* list, const int data) {
  struct Node* newNode = new Node;
  newNode->data = data;
  newNode->next = nullptr;
  if (list->head == nullptr){
    list->head = newNode;
    list->tail = newNode;
    return;
  }
  list->tail->next = newNode;
  list->tail = newNode;
}

void deleteLinkedList(LinkedList* list) {
    Node* current = list->head;
    Node* next;

    while (current != nullptr) {
        next = current->next;
        delete current;
        current = next;
    }

    list->head = nullptr;
    list->tail = nullptr;
}

void scanKeysTask(void *pvParameters)
{
  const TickType_t xFrequency = 20 / portTICK_PERIOD_MS;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  LinkedList allKeysPressed;
  
  while (1)
  {
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
    deleteLinkedList(&allKeysPressed);
    xSemaphoreTake(keyArrayMutex, portMAX_DELAY);
    pressedKeys = 0;
  
    // Read keys
    for (size_t row = 0; row < 3; row++) {
      setRow(row);
      delayMicroseconds(3);
      pressedKeys |= readCols() << (4*row);
    }
    pressedKeys = ~pressedKeys & 0x0FFF;

    // Add key step sizes to linked list  (polyphony)
    // LOCAL KEYS
    if (canMode == 0) {
      for (int i = 0; i < 12; i++) {
        if (pressedKeys & (1 << i)) { 
          addNode(&allKeysPressed, (uint32_t)((float)stepSizes[12*(octaveSelect-2)+i] * pitchBend));
          if (effect == 2){
            addNode(&allKeysPressed, (uint32_t)((float)stepSizes[12*(octaveSelect-3)+i] * pitchBend));
            addNode(&allKeysPressed, (uint32_t)((float)stepSizes[12*(octaveSelect-1)+i] * pitchBend));
          }
          keys[i] = notes[i];  
        }
        else{
          keys[i] = 0;
        }
      }
    // CAN RECEIVE KEYS
      for (int j = 0; j < 2; j++) {
        if (prev_message[j] != cur_message[j]){     //  NEW KEY STATE
          for (int i = 0; i < 12; i++) {
            if (cur_message[j] & (1 << i)) { 
              addNode(&allKeysPressed, (uint32_t)((float)stepSizes[12*(octaveRX[j]-2)+i] * pitchBend));
              keys[i] = notes[i];  
              if (effect == 2){
                addNode(&allKeysPressed, (uint32_t)((float)stepSizes[12*(octaveSelect-3)+i] * pitchBend));
                addNode(&allKeysPressed, (uint32_t)((float)stepSizes[12*(octaveSelect-1)+i] * pitchBend));
              }
            }
          }
          prev_message[j] = cur_message[j];
        }
        else{                                 //  REPEAT OLD KEY STATE
          for (int i = 0; i < 12; i++){
            if (prev_message[j] & (1 << i)) { 
              addNode(&allKeysPressed, (uint32_t)((float)stepSizes[12*(octaveRX[j]-2)+i] * pitchBend));
              keys[i] = notes[i];
              if (effect == 2){
                addNode(&allKeysPressed, (uint32_t)((float)stepSizes[12*(octaveSelect-3)+i] * pitchBend));
                addNode(&allKeysPressed, (uint32_t)((float)stepSizes[12*(octaveSelect-1)+i] * pitchBend));
              }  
            }
          }
        }
      }
    }

    else {
      // Send message
      TX_Message[0] = canMode - 1;
      TX_Message[1] = (pressedKeys >> 8) & 0xF; // keys 9-12
      TX_Message[2] = (pressedKeys >> 4) & 0xF; // keys 5-8
      TX_Message[3] = pressedKeys & 0xF;        // keys 1-4
      TX_Message[4] = octaveSelect;

      bool arraysAreEqual = std::equal(std::begin(TX_Message), std::end(TX_Message), std::begin(prevTX));
      if (!arraysAreEqual){
        Serial.println("[RX Send]");
        xQueueSend(msgOutQ, TX_Message, portMAX_DELAY);
        for (int i = 0; i < 5; i++){
          prevTX[i] = TX_Message[i];
        }
      }     
    }
    
    // Read knobs
    for (size_t row = 3; row < 7; row++) {
      setRow(row);
      delayMicroseconds(3);
      keyArray[row-3] = readCols();
    }

    xSemaphoreGive(keyArrayMutex);

    // Send keys to sampler
    __atomic_store_n(&currentStepSizes.head, allKeysPressed.head, __ATOMIC_RELAXED);
    __atomic_store_n(&currentStepSizes.tail, allKeysPressed.tail, __ATOMIC_RELAXED);
    //printList(&allKeysPressed);
  }
}

void octaveControl(){
  // Read joystick (octaves)
    float joyX = analogRead(A1);
    float joyXscale = (joyX / 1023) * 100;

    if (joyXscale > 80 && OctToggle == false){
      __atomic_store_n(&octaveSelect, max(__atomic_load_n(&octaveSelect, __ATOMIC_RELAXED) - 1, MIN_OCT), __ATOMIC_RELAXED);
      OctToggle = true;
    }
    else if (joyXscale < 15 && OctToggle == false){
      __atomic_store_n(&octaveSelect, min(__atomic_load_n(&octaveSelect, __ATOMIC_RELAXED) + 1, MAX_OCT), __ATOMIC_RELAXED);
      OctToggle = true;
    }
    else if (joyXscale >= 15 && joyXscale <= 80){
      OctToggle = false;
    }
}

void pitchControl(){
  // Read joystick (stepsize)
  float joyY = analogRead(A0);
  float joyYscale = (joyY / 1023);
  pitchBend = 1.00f;

  if (joyYscale < calZero - 0.05) {
    pitchBend = 1 + (calZero - joyYscale) * 0.5;
  }
  else if (joyYscale > calZero + 0.05) {
    pitchBend = 1 - (joyYscale - calZero) * 0.5;
  }

  // Vibrato
  if (effect == 1 && pressedKeys != 0){
    if (vibrato < 0.8 && vibToggle == false){
      vibrato += 0.04;
      pitchBend = 1 + vibrato;
      if (vibrato >= 0.1){
        vibToggle = true;
      }
      
    }
    if (vibToggle == true){
      vibrato -= 0.02;
      pitchBend = 1 + vibrato;
      if (vibrato <= 0) {
        vibToggle = false;
      }
    }
    // Serial.println(pitchBend);
  }

}

void readControlsTask(void *pvParameters)
{
  const TickType_t xFrequency = 20 / portTICK_PERIOD_MS;
  TickType_t xLastWakeTime = xTaskGetTickCount();

  // Knob Constructors
  Knob volumeKnob(0, 8, &volume);
  Knob functionKnob(0, 3, &waveform);
  Knob effectKnob(0, 3, &effect);
  Knob canKnob(0, 2, &canMode);

  // Calculate the zero error (stick drift)
  float initialY = analogRead(A1);
  calZero = (initialY / 1023);

  while (1)
  {
    vTaskDelayUntil(&xLastWakeTime, xFrequency);

    functionKnob.update(keyArray[1] & 0x03);// KNOB 0       ( 0 )    ( 1 )    ( 2 )    ( 3 )
    effectKnob.update(keyArray[0] >> 2);    // KNOB 1      [4]>>2  [4]&0x03  [3]>>2  [3]&0x03

    // Update Knobs
    // Not pressed
    if (keyArray[3] & 0x01 == 1) {
      if (canMode != 0){
        showCAN = 1;
      }
      else {
        volumeKnob.update(keyArray[1] >> 2);
        showCAN = 0;
      }
    }
    // Press Down
    else{
      canKnob.update(keyArray[1] >> 2);
      showCAN = 1;
    }

    octaveControl(); 
    pitchControl();
  }

}

void displayKeysTask(void *pvParameters)
{
  const TickType_t xFrequency = 100 / portTICK_PERIOD_MS;
  TickType_t xLastWakeTime = xTaskGetTickCount();

  while (1)
  {
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
    u8g2.setFont(u8g2_font_profont10_tf);
    u8g2.clearBuffer();

    if (showCAN == 0) {
      u8g2.setCursor(100, 10);
      u8g2.print("Vol:");
      u8g2.print(volume);

      u8g2.setCursor(100, 20);
      u8g2.print("Oct:");
      u8g2.print(octaveSelect);

      u8g2.setCursor(2, 20);
      u8g2.print("WAVE:");
      u8g2.print(waves[waveform]);

      u8g2.setCursor(2, 30);
      u8g2.print("FX:");
      u8g2.print(effects[effect]);

      u8g2.setCursor(2, 10);
      u8g2.print("KEY: ");
      for (size_t i = 0; i < 12; i++){
        u8g2.print(keys[i]);
      }
      u8g2.sendBuffer();
    }
    else{
      u8g2.setFont(u8g2_font_tenthinguys_t_all);
      u8g2.setCursor(30, 15);
      u8g2.print("CAN MODE");
      u8g2.setCursor(38, 30);
      u8g2.print(canModes[canMode]);
      u8g2.setFont(u8g2_font_profont10_tf);
      u8g2.setCursor(100, 30);
      u8g2.print("Oct:");
      u8g2.print(octaveSelect);
      u8g2.sendBuffer();
    }
    digitalToggle(LED_BUILTIN);
  }
}

// CAN functions
void CAN_RX_ISR(void)
{
  uint8_t RX_Message_ISR[8];
  uint32_t ID;
  CAN_RX(ID, RX_Message_ISR);
  xQueueSendFromISR(msgInQ, RX_Message_ISR, NULL);
}

void CAN_TX_Task(void *pvParameters)
{
  uint8_t msgOut[8];
  while (1)
  {
    xQueueReceive(msgOutQ, msgOut, portMAX_DELAY);
    xSemaphoreTake(CAN_TX_Semaphore, portMAX_DELAY);
    CAN_TX(0x123, msgOut);
  }
}

void CAN_TX_ISR(void)
{
  xSemaphoreGiveFromISR(CAN_TX_Semaphore, NULL);
}

void decodeTask(void *pVparameters)
{
  while (1)
  {
    xQueueReceive(msgInQ, RX_Message, portMAX_DELAY); // wait for message

    cur_message[RX_Message[0]] = 0;
    // Create the  keyboard array
    cur_message[RX_Message[0]] |= RX_Message[1];
    cur_message[RX_Message[0]] <<= 4;   
    cur_message[RX_Message[0]] |= RX_Message[2];
    cur_message[RX_Message[0]] <<= 4;   
    cur_message[RX_Message[0]] |= RX_Message[3];
    // Set the keyboard octave
    octaveRX[RX_Message[0]] = RX_Message[4];
    Serial.println(cur_message[0]);
  }
}

void setup()
{
  // Set pin directions
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

  // Initialise display
  setOutMuxBit(DRST_BIT, LOW); // Assert display logic reset
  delayMicroseconds(2);
  setOutMuxBit(DRST_BIT, HIGH); // Release display logic reset
  u8g2.begin();
  setOutMuxBit(DEN_BIT, HIGH); // Enable display power supply

  // Initalise SINE
  for (int i = 0; i < TABLE_SIZE; i++) {
    sinTable[i] = 127 * sin((float) i / (float) TABLE_SIZE * 2.0 * PI);
  }

  // Initialise UART
  Serial.begin(9600);

  // Create the mutex and assign its handle
  keyArrayMutex = xSemaphoreCreateMutex();

  // CAN bus
  CAN_Init(false);
  setCANFilter(0x123, 0x7ff);
  CAN_RegisterRX_ISR(CAN_RX_ISR);
  CAN_RegisterTX_ISR(CAN_TX_ISR);
  CAN_Start();
  msgInQ = xQueueCreate(36, 8);                      // create queue for received messages
  msgOutQ = xQueueCreate(36, 8);                     // create queue for transmitted messages
  CAN_TX_Semaphore = xSemaphoreCreateCounting(3, 3); // 3 slots for outgoing messages, start with 3 slots available. Max count = 3 so a 4th attempt is blocked
  
  // Create timer for audio
  TIM_TypeDef *Instance = TIM1;
  HardwareTimer *sampleTimer = new HardwareTimer(Instance);
  sampleTimer->setOverflow(22050, HERTZ_FORMAT);
  sampleTimer->attachInterrupt(sampleISR);
  sampleTimer->resume();

  // Runs the key detection and display on separate tasks
  TaskHandle_t scanKeysHandle = NULL;
  xTaskCreate(scanKeysTask, "scanKeys", 64, NULL, 2, &scanKeysHandle);
  TaskHandle_t displayKeysHandle = NULL;
  xTaskCreate(displayKeysTask, "displayKeys", 256, NULL, 1, &displayKeysHandle);
  TaskHandle_t readControlsHandle = NULL;
  xTaskCreate(readControlsTask, "readControls", 256, NULL, 1, &readControlsHandle);

  // setup threading for decoding messages
  TaskHandle_t decodeTaskHandle = NULL;
  xTaskCreate(
      decodeTask,         /* Function that implements the task */
      "decode",           /* Text name for the task */
      256,                /* Stack size in words, not bytes */
      NULL,               /* Parameter passed into the task */
      4,                  /* Task priority */
      &decodeTaskHandle); /* Pointer to store the task handle */

   // setup threading for sending messages
  TaskHandle_t CAN_TX_TaskHandle = NULL;
  xTaskCreate(
      CAN_TX_Task,         /* Function that implements the task */
      "CAN_TX",            /* Text name for the task */
      256,                 /* Stack size in words, not bytes */
      NULL,                /* Parameter passed into the task */
      3,                   /* Task priority */
      &CAN_TX_TaskHandle); /* Pointer to store the task handle */

  vTaskStartScheduler();
}

void loop()
{
  delay(100);
}