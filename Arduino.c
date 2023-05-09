//Libraries
#include <AccelStepper.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

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

// BUTTONS
#define BUTTON1_PIN 13  //Forces Homing Execution
#define BUTTON2_PIN 12  //Starts Change Character procedure
#define HOME1_PIN 2
#define HOME2_PIN 3

// KEYBOARD PIN and CONF
#define KEY_PIN A5
#define R_REF 10000
#define V_REF 5
#define ADC_RES 1024
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
char keyValue = 0;
char lastKeyValue = 0;
int keyBufferLength = 0;

// DEFINE STRUCT VARIABLES
//State machines
MainState mainState = START_HOME;
UpdateState updateState = SET_TARGETS;

// LCD and motors
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);
AccelStepper stepper1(AccelStepper::HALF4WIRE, STEPPER1_PIN1, STEPPER1_PIN3, STEPPER1_PIN2, STEPPER1_PIN4);
AccelStepper stepper2(AccelStepper::HALF4WIRE, STEPPER2_PIN1, STEPPER2_PIN3, STEPPER2_PIN2, STEPPER2_PIN4);
int target1Pos = 0;
int target2Pos = 0;

// SETUP FUNCTION ////////////////////////////////////////////////////////////
void setup() {
  // SETUP PINS (Buttons, KeyAnalog..)
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  pinMode(HOME1_PIN, INPUT_PULLUP);
  pinMode(HOME2_PIN, INPUT_PULLUP);
#ifdef DEBUG
  // Setup Serial COM
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
  //debounceButton(&home1State, HOME1_PIN, &lastHome1Time, SNS_DEBOUNCE_DELAY);
  //debounceButton(&home2State, HOME2_PIN, &lastHome2Time, SNS_DEBOUNCE_DELAY);
  home1State = digitalRead(HOME1_PIN);
  home2State = digitalRead(HOME2_PIN);
  // Buttons readings
  lastButton1State = button1State;
  lastButton2State = button1State;
  //debounceButton(&button1State, BUTTON1_PIN, &lastButton1Time, BTN_DEBOUNCE_DELAY);
  //debounceButton(&button2State, BUTTON2_PIN, &lastButton2Time, BTN_DEBOUNCE_DELAY);
  button1State = digitalRead(BUTTON1_PIN);
  button2State = digitalRead(BUTTON2_PIN);

  lastKeyValue = keyValue;
  //debounceKey(&keyValue, convertKey(KEY_PIN), &lastKeyTime, READ_CLOCK_DELAY);
  keyValue = convertKey(KEY_PIN);
  // Key_Buffer Update
  // ************************************************* Esplorare il problema delle doppie **************************************************
  if ((lastKeyValue != keyValue) && (keyBufferLength < BUFFER_SIZE) && (keyValue != 0)) {
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
      }
      if (initHome2SyncFlag == finHome2SyncFlag) {
        stepper2.setCurrentPosition((((finHome2Pos + initHome2Pos) / 2) % STEP_PER_ROTATION) + target1Pos);
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
      switch (updateState) {
        case SET_TARGETS:
          int stepper1CurrentPosition = stepper1.currentPosition() % STEP_PER_ROTATION;
          int stepper2CurrentPosition = stepper2.currentPosition() % STEP_PER_ROTATION;
          //bool flag = FALSE;
          if (target1Pos < stepper1CurrentPosition) {
            stepper1.move(STEP_PER_ROTATION - (stepper1CurrentPosition - target1Pos));
          } else {
            stepper1.move(target1Pos - (stepper1CurrentPosition));
          }

          if (target2Pos < stepper2CurrentPosition) {
            stepper2.move(STEP_PER_ROTATION - (stepper2CurrentPosition - target2Pos));
          } else {
            stepper2.move(target2Pos - stepper2CurrentPosition);
          }
          /*
          if (flag) {
            updateState = CHECK_IF_DONE;
            flag = FALSE;
          }
          else flag = TRUE;
          */
          break;

        case CHECK_IF_DONE:
          if ((stepper1.distanceToGo() == 0) && ((stepper2.distanceToGo() == 0))) {
          Serial.print("CAZZOOOOO");
          updateState = SET_TARGETS;
          mainState = UPDATE_COMPLETE;
          }
          break;
      }
      break;
      // END of UPDATE_CELL STATE-MACHINE
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
    lcd.setCursor(7, 1);  // Second Line update
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
  if ((button1State == LOW) && (lastButton1State == HIGH)) {
    mainState = UPDATE_COMPLETE;
    updateState = SET_TARGETS;
    Serial.print("CAZZOOOOO");
  }
  if (printClock) {
    Serial.print(keyValue);
    Serial.print("..");
    Serial.print(analogRead(KEY_PIN));
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

/* const char map[] = "ZXVTRPNLJHFDBACEGIKMOQSUWY";
char myConvertKey(int buttonPin) {
	char value = 0;
	float reading = (analogRead(buttonPin) * V_REF) / ADC_RES;
	float resistor = (reading * R_REF) / (V_REF - reading);
	int numericValue = (resistor + 500) / 1000;

	// *** Inserire controllo per verificare che value sia corretto e sia contenuto nell'array (numericValue < strlen(map)) *** 
	value = map[numericValue];
	return value;

	// *** Si può fare anche con convertKey2 sostituendo 0 a 500 e 4462 a 1000 (da verificare) ***
}
*/

char convertKey(int buttonPin) {
  char value = 0;
  float reading = analogRead(buttonPin);
  if (reading > 1000) {
  } else if (reading > 987) value = 'Y';
  else if (reading > 975) value = 'W';
  else if (reading > 960) value = 'U';
  else if (reading > 945) value = 'S';
  else if (reading > 930) value = 'Q';
  else if (reading > 915) value = 'O';
  else if (reading > 900) value = 'M';
  else if (reading > 865) value = 'K';
  else if (reading > 830) value = 'I';
  else if (reading > 800) value = 'G';
  else if (reading > 765) value = 'E';
  else if (reading > 730) value = 'C';
  else if (reading > 700) value = 'A';
  else if (reading > 650) value = 'B';
  else if (reading > 600) value = 'D';
  else if (reading > 550) value = 'F';
  else if (reading > 500) value = 'H';
  else if (reading > 450) value = 'J';
  else if (reading > 400) value = 'L';
  else if (reading > 350) value = 'N';
  else if (reading > 300) value = 'P';
  else if (reading > 250) value = 'R';
  else if (reading > 200) value = 'T';
  else if (reading > 150) value = 'V';
  else if (reading > 100) value = 'X';
  else if (reading > 0) value = 'Z';
  return value;
}

void debounceButton(int *filteredButtonState, int inputPin, unsigned long *lastButtonTime, int delay) {
  unsigned long currentMillis = millis();
  int reading = digitalRead(inputPin);

  if (*filteredButtonState != reading) {
    *lastButtonTime = currentMillis;
  }
  if (currentMillis - *lastButtonTime > delay) {
    if (*filteredButtonState != reading) {
      *filteredButtonState = reading;
    }
  }
}

void debounceKey(char *filteredKeyState, char keyState, unsigned long *lastButtonTime, int delay) {
  unsigned long currentMillis = millis();

  if (*filteredKeyState != keyState) {
    *lastButtonTime = currentMillis;
  }
  if (currentMillis - *lastButtonTime > delay) {
    if (*filteredKeyState != keyState) {
      *filteredKeyState = keyState;
    }
  }
}
