//Libraries
#include <AccelStepper.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <PS2Keyboard.h> //kayboard that works with PS/2 protocol

// ######################################################################
#define DEBUG
// ######################################################################

#define FALSE false
#define TRUE true

// TARGET POSITIONS mapped in steps each sectors
#define TARGET_POS_0 0
#define TARGET_POS_1 255
#define TARGET_POS_2 510
#define TARGET_POS_3 765
#define TARGET_POS_4 1020
#define TARGET_POS_5 1275
#define TARGET_POS_6 1530
#define TARGET_POS_7 1785

// PERIPHERALS WIRING
// LCD 1602 Freenove
#define LCD_ADDRESS 0x3F
#define LCD_COLUMNS 16
#define LCD_ROWS 2

// BUTTONS and KEYBOARD PIN
#define BUTTON1_PIN 13  //keyboard Data pin
#define BUTTON2_PIN 12  //IRQpin for keyboard
#define HOME1_PIN 2
#define HOME2_PIN 3
#define BUFFER_SIZE 8

// STEPPER 1
#define STEPPER1_PIN1 7
#define STEPPER1_PIN2 6
#define STEPPER1_PIN3 5
#define STEPPER1_PIN4 4

// STEPPER 2
#define STEPPER2_PIN1 11
#define STEPPER2_PIN2 10
#define STEPPER2_PIN3 9
#define STEPPER2_PIN4 8

// CONFIG PARAMETER
// Debouncing times
#define BTN_DEBOUNCE_DELAY 50
#define SNS_DEBOUNCE_DELAY 2
#define PRINT_CLOCK_DELAY 1500
#define READ_CLOCK_DELAY 40

// Motors
#define MAX_SPEED 3000
#define ACCELERATION 5000
#define SPEED 2000
#define STEP_PER_ROTATION 2038

// Define the states of the state machines as an enumeration
enum MainState {
  START_HOME,
  CONTINUE_HOMING,
  UPDATE_HOME_POS,
  READY,
  UPDATE_CELL,
  UPDATE_COMPLETE
};

enum UpdateState {
  SET_TARGETS,
  CHECK_IF_DONE
};

// DEFINE GLOBAL VARIABLES
// Keyboard variable
PS2Keyboard keyboard;

// Button State variables
int button1State = HIGH;      // LOW when HOMING REQUESTED
int lastButton1State = HIGH;  // Used to see only EDGE triggers
int button2State = HIGH;      // LOW when CHANGE_CHAR REQUESTED
int lastButton2State = HIGH;  // Used to see only EDGE triggers
int home1State = HIGH;        // MOTOR 1 HOME_SENSOR
int home2State = HIGH;        // MOTOR 2 HOME_SENSOR
int lastHome1State = home1State;
int lastHome2State = home2State;

//Button debouncing variables
unsigned long lastButton1Time = 0;
unsigned long lastButton2Time = 0;
unsigned long lastHome1Time = 0;
unsigned long lastHome2Time = 0;
unsigned long lastKeyTime = 0;

//Clock variables for prints and keyboard/buttons readings
bool printClock = FALSE;
bool readClock = FALSE;
unsigned long lastReadClockTime = 0;
unsigned long lastPrintClockTime = 0;

//Dynamic homing states variables
int initHome1Pos = 0;
int initHome2Pos = 0;
int finHome1Pos = 0;
int finHome2Pos = 0;
bool initHome1Upd = FALSE;
bool initHome2Upd = FALSE;
bool finHome1Upd = FALSE;
bool finHome2Upd = FALSE;

bool initHome1SyncFlag = FALSE;
bool initHome2SyncFlag = FALSE;
bool finHome1SyncFlag = FALSE;
bool finHome2SyncFlag = FALSE;

//Character buffer variables
//char keyBuffer[BUFFER_SIZE]= {0};
char keyBuffer[BUFFER_SIZE] = "        ";
char keyValue = '';
char lastKeyValue = '';
int keyBufferLength = 0;

// DEFINE STRUCT VARIABLES
//State machines
MainState mainState = START_HOME;
UpdateState updateState = SET_TARGETS;

bool updateFlag = FALSE;
long stepper1CurrentPosition = 0;
long stepper2CurrentPosition = 0;
// LCD and motors
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);
AccelStepper stepper1(AccelStepper::HALF4WIRE, STEPPER1_PIN1, STEPPER1_PIN3, STEPPER1_PIN2, STEPPER1_PIN4);
AccelStepper stepper2(AccelStepper::HALF4WIRE, STEPPER2_PIN1, STEPPER2_PIN3, STEPPER2_PIN2, STEPPER2_PIN4);
int target1Pos = 0;
int target2Pos = 0;

// SETUP FUNCTION ////////////////////////////////////////////////////////////
void setup() {
  // SETUP PINS (Buttons, KeyAnalog..)
  //pinMode(BUTTON1_PIN, INPUT_PULLUP);
  //pinMode(BUTTON2_PIN, INPUT_PULLUP);
  pinMode(HOME1_PIN, INPUT_PULLUP);
  pinMode(HOME2_PIN, INPUT_PULLUP);
#ifdef DEBUG
  // Setup Serial COM
  keyboard.begin(BUTTON1_PIN, BUTTON2_PIN);
  Serial.begin(9600);
#endif
  // SETUP LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);    // (COLUMN, ROW)
  lcd.print("Buffer: ");  //restart from (8, 0)
  lcd.setCursor(0, 1);
  lcd.print("State:");  //restart from (6, 1)

  // SETUP MOTORS
  stepper1.setMaxSpeed(MAX_SPEED);
  stepper1.setAcceleration(ACCELERATION);
  stepper1.setSpeed(SPEED);

  stepper2.setMaxSpeed(MAX_SPEED);
  stepper2.setAcceleration(ACCELERATION);
  stepper2.setSpeed(SPEED);

  keyBuffer[0] = 'F';
  keyBuffer[1] = 'I';
  keyBuffer[2] = 'R';
  keyBuffer[3] = 'E';
  keyBuffer[4] = 'F';
  keyBuffer[5] = 'L';
  keyBuffer[6] = 'Y';
  keyBufferLength = 7;
  //stepper1.setMinPulseWidth(50);
  //stepper2.setMinPulseWidth(50);
}

// LOOP FUNCTION /////////////////////////////////////////////////////////////
void loop() {
  // Clock for delayed routines
  computeClock(&printClock, &lastPrintClockTime, PRINT_CLOCK_DELAY);
  // INPUT BRIDGE: update buttons state, keyboard state and so on...
  // Sensors readings
  lastHome1State = home1State;
  lastHome2State = home2State;
  home1State = digitalRead(HOME1_PIN);
  home2State = digitalRead(HOME2_PIN);
  // Buttons readings
  lastButton1State = button1State;
  lastButton2State = button1State;
  lastKeyValue = keyValue;
  if (keyboard.available()) {
    keyValue = keyboard.read();
    if (keyValue ==  PS2_ENTER) {
      button1State = HIGH;
      keyValue = ''; 
    }
    else {
      button1State = LOW;
    }
    if (keyValue ==  PS2_TAB) {
      button2State = HIGH;
      keyValue = ''; 
    }
    else {
      button2State = LOW;
    }

  }
  
  // Key_Buffer Update
  // ************************************************* Esplorare il problema delle doppie **************************************************
  if ((lastKeyValue != keyValue) && (keyBufferLength < BUFFER_SIZE) && (keyValue != '')) {
    keyBuffer[keyBufferLength] = keyValue;
    keyBufferLength++;
  }
  // ERROR HANDLER: no home trigger after 1 full revolution and some (check current pos) or if 2 buttons pressed together
  // DYNAMIC_HOMING HANDLER
  if (lastHome1State != home1State) {
    if (home1State == HIGH) {  // home1State = HIGH 	lastHome1State = LOW	// Rising phase 1
      finHome1Pos = stepper1.currentPosition();
      finHome1SyncFlag = changeFlag(finHome1SyncFlag);
    } else {  // home1State = LOW 	lastHome1State = HIGH	// Falling phase 1
      initHome1Pos = stepper1.currentPosition();
      initHome1SyncFlag = changeFlag(initHome1SyncFlag);
    }
  }

  if (lastHome2State != home2State) {
    if (home2State == HIGH) {  // home2State = HIGH 	lastHome2State = LOW	// Rising phase 2
      finHome2Pos = stepper2.currentPosition();
      finHome2SyncFlag = changeFlag(finHome2SyncFlag);
    } else {  // home2State = LOW	lastHome2State = HIGH	// Falling phase 2
      initHome2Pos = stepper2.currentPosition();
      initHome2SyncFlag = changeFlag(initHome2SyncFlag);
    }
  }

  // MAIN STATE MACHINE:
  switch (mainState) {
    case START_HOME:
      if (home1State == HIGH) {
        if (stepper1.distanceToGo() == 0) stepper1.move(STEP_PER_ROTATION / 2);
      } else if (initHome1Upd == FALSE) {
        initHome1Pos = stepper1.currentPosition();
        initHome1Upd = TRUE;
        stepper1.stop();
      }
      if (home2State == HIGH) {
        if (stepper2.distanceToGo() == 0) stepper2.move(STEP_PER_ROTATION / 2);
      } else if (initHome2Upd == FALSE) {
        initHome2Pos = stepper2.currentPosition();
        initHome2Upd = TRUE;
        stepper2.stop();
      }
      if ((home1State == LOW) && (home2State == LOW)) {
        initHome1Upd = FALSE;
        initHome2Upd = FALSE;
        mainState = CONTINUE_HOMING;
        stepper1.move(STEP_PER_ROTATION / 4);
        stepper2.move(STEP_PER_ROTATION / 4);
      }
      break;
    case CONTINUE_HOMING:
      if (home1State == LOW) {
        if (stepper1.distanceToGo() == 0) stepper1.move(STEP_PER_ROTATION / 4);
      } else if (finHome1Upd == FALSE) {
        finHome1Pos = stepper1.currentPosition();
        finHome1Upd = TRUE;
        stepper1.stop();
      }
      if (home2State == LOW) {
        if (stepper2.distanceToGo() == 0) stepper2.move(STEP_PER_ROTATION / 4);
      } else if (finHome2Upd == FALSE) {
        finHome2Pos = stepper2.currentPosition();
        finHome2Upd = TRUE;
        stepper2.stop();
      }
      if ((home1State == HIGH) && (home2State == HIGH)) {
        finHome1Upd = FALSE;
        finHome2Upd = FALSE;
        stepper1.setCurrentPosition(finHome1Pos - initHome1Pos);
        stepper2.setCurrentPosition(finHome2Pos - initHome2Pos);
        mainState = READY;
      }
      break;
    case UPDATE_HOME_POS:
      /* CODICE CALCOLO NUOVO VALORE ATTUALE POSIZIONE MOTORI */
      if (initHome1SyncFlag == finHome1SyncFlag) {
        stepper1.setCurrentPosition((((finHome1Pos + initHome1Pos) / 2) % STEP_PER_ROTATION) + target1Pos);
        Serial.print("Stepper 1 Position Updated \n");
      }
      if (initHome2SyncFlag == finHome2SyncFlag) {
        stepper2.setCurrentPosition((((finHome2Pos + initHome2Pos) / 2) % STEP_PER_ROTATION) + target1Pos);
        Serial.print("Stepper 2 Position Updated \n");
      }
      mainState = READY;
      break;

    case READY:
      if ((button2State == LOW) && (keyBufferLength > 0)) {
        setTargets(&target1Pos, &target2Pos, keyBuffer[0]);
        mainState = UPDATE_CELL;
      }
      if ((button1State == LOW) && (lastButton1State == HIGH)) {
        mainState = START_HOME;
      }
      break;

    // START of UPDATE_CELL STATE-MACHINE
    case UPDATE_CELL:
      if (updateFlag) {
        if ((stepper1.distanceToGo() == 0) && ((stepper2.distanceToGo() == 0))) {
          updateFlag = FALSE;
          mainState = UPDATE_COMPLETE;
        }
      }
      else {
        stepper1CurrentPosition = stepper1.currentPosition() % STEP_PER_ROTATION;
        stepper2CurrentPosition = stepper2.currentPosition() % STEP_PER_ROTATION;
        if (target1Pos < stepper1CurrentPosition) {
          stepper1.move(STEP_PER_ROTATION - (stepper1CurrentPosition - target1Pos));
        } 
        else {
          stepper1.move(target1Pos - (stepper1CurrentPosition));
        }

        if (target2Pos < stepper2CurrentPosition) {
          stepper2.move(STEP_PER_ROTATION - (stepper2CurrentPosition - target2Pos));
        } 
        else {
          stepper2.move(target2Pos - stepper2CurrentPosition);
        }
        updateFlag = TRUE;
      }
    break;
    case UPDATE_COMPLETE:  // SHIFT BUFFER, REMOVE-1 from keyBufferLength, write the last one to null.
      for (int i = 0; i < BUFFER_SIZE - 1; i++) {
        keyBuffer[i] = keyBuffer[i + 1];
      }
      keyBuffer[BUFFER_SIZE - 1] = ' ';
      keyBufferLength--;
      mainState = UPDATE_HOME_POS;
      break;
  }

  // OUTPUT BRIDGE-CALLS: call the run function, update lcd, clean buffer and so on..
  // Run calls for motors
  stepper1.run();
  stepper2.run();
  // LCD print out
  if (printClock) {
    lcd.setCursor(8, 0);  // First Line update
    lcd.print(keyBuffer);
    lcd.setCursor(6, 1);  // Second Line update
    switch (mainState) {
      case START_HOME:
        lcd.print("START_HOME");
        break;
      case CONTINUE_HOMING:
        lcd.print(" CONT_HOME");
        break;
      case UPDATE_HOME_POS:
        lcd.print("UPD_HM_PS");
        break;
      case READY:
        lcd.print("   READY  ");
        break;
      case UPDATE_CELL:
        lcd.print("UPDAT_CELL");
        break;
      case UPDATE_COMPLETE:
        lcd.print("UPD_COMPLT");
        break;
    }
  }
// DEBUGGING
#ifdef DEBUG
  if (printClock) {
    Serial.print(keyValue);
    Serial.print("..");
    Serial.print("..");
    Serial.print(updateState);
    Serial.print("..");
    Serial.print(home1State);
    Serial.print(home2State);
    Serial.print("..");
    Serial.print(stepper1.distanceToGo());
    Serial.print("-");
    Serial.print(stepper2.distanceToGo());
    Serial.print("\n");
  }
#endif
}  //CLOSE loop()


// USER FUNCTION //////////////////////////////////////////////////////////
void computeClock(bool *clockState, unsigned long *lastClockTime, int delay) {
  unsigned long currentMillis = millis();
  if (*clockState == TRUE) {
    *clockState = FALSE;
    *lastClockTime = currentMillis;
  } else if (currentMillis - *lastClockTime > delay) {
    *clockState = TRUE;
  }
}

bool changeFlag(bool flag) {
  if (flag == 0) flag = 1;
  else flag = 0;
  return flag;
}

void setTargets(int *target1, int *target2, char character) {
  switch (character) {
    case 'A':
      *target1 = TARGET_POS_1;
      *target2 = TARGET_POS_0;
      break;
    case 'B':
      *target1 = TARGET_POS_6;
      *target2 = TARGET_POS_0;
      break;
    case 'C':
      *target1 = TARGET_POS_1;
      *target2 = TARGET_POS_4;
      break;
    case 'D':
      *target1 = TARGET_POS_1;
      *target2 = TARGET_POS_3;
      break;
    case 'E':
      *target1 = TARGET_POS_1;
      *target2 = TARGET_POS_7;
      break;
    case 'F':
      *target1 = TARGET_POS_6;
      *target2 = TARGET_POS_4;
      break;
    case 'G':
      *target1 = TARGET_POS_6;
      *target2 = TARGET_POS_3;
      break;
    case 'H':
      *target1 = TARGET_POS_6;
      *target2 = TARGET_POS_7;
      break;
    case 'I':
      *target1 = TARGET_POS_7;
      *target2 = TARGET_POS_4;
      break;
    case 'J':
      *target1 = TARGET_POS_7;
      *target2 = TARGET_POS_3;
      break;
    case 'K':
      *target1 = TARGET_POS_5;
      *target2 = TARGET_POS_0;
      break;
    case 'L':
      *target1 = TARGET_POS_2;
      *target2 = TARGET_POS_0;
      break;
    case 'M':
      *target1 = TARGET_POS_5;
      *target2 = TARGET_POS_4;
      break;
    case 'N':
      *target1 = TARGET_POS_5;
      *target2 = TARGET_POS_3;
      break;
    case 'O':
      *target1 = TARGET_POS_5;
      *target2 = TARGET_POS_7;
      break;
    case 'P':
      *target1 = TARGET_POS_2;
      *target2 = TARGET_POS_4;
      break;
    case 'Q':
      *target1 = TARGET_POS_2;
      *target2 = TARGET_POS_3;
      break;
    case 'R':
      *target1 = TARGET_POS_2;
      *target2 = TARGET_POS_7;
      break;
    case 'S':
      *target1 = TARGET_POS_3;
      *target2 = TARGET_POS_4;
      break;
    case 'T':
      *target1 = TARGET_POS_3;
      *target2 = TARGET_POS_3;
      break;
    case 'U':
      *target1 = TARGET_POS_5;
      *target2 = TARGET_POS_1;
      break;
    case 'V':
      *target1 = TARGET_POS_2;
      *target2 = TARGET_POS_1;
      break;
    case 'W':
      *target1 = TARGET_POS_7;
      *target2 = TARGET_POS_2;
      break;
    case 'X':
      *target1 = TARGET_POS_5;
      *target2 = TARGET_POS_5;
      break;
    case 'Y':
      *target1 = TARGET_POS_5;
      *target2 = TARGET_POS_2;
      break;
    case 'Z':
      *target1 = TARGET_POS_5;
      *target2 = TARGET_POS_6;
      break;
  }
}


