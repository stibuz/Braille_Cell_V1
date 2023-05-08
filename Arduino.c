//Libraries
#include <AccelStepper.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define FALSE 			false
#define TRUE 			true

// TARGET POSITIONS mapped in steps each sectors
#define TARGET_STEP 		255
#define TARGET_POS_0 		0
#define TARGET_POS_1 		TARGET_POS_0 + TARGET_STEP
#define TARGET_POS_2 		TARGET_POS_1 + TARGET_STEP
#define TARGET_POS_3		TARGET_POS_2 + TARGET_STEP
#define TARGET_POS_4 		TARGET_POS_3 + TARGET_STEP
#define TARGET_POS_5 		TARGET_POS_4 + TARGET_STEP
#define TARGET_POS_6 		TARGET_POS_5 + TARGET_STEP
#define TARGET_POS_7 		TARGET_POS_6 + TARGET_STEP

// PERIPHERALS WIRING
// LCD 1602 Freenove
#define LCD_ADDRESS 		0x3F
#define LCD_COLUMNS 		16
#define LCD_ROWS 		2

// BUTTONS
#define BUTTON1_PIN 		13  //Forces Homing Execution
#define BUTTON2_PIN 		12  //Starts Change Character procedure
#define HOME1_PIN 		2
#define HOME2_PIN 		3

// KEYBOARD PIN and CONF
#define KEY_PIN 		A5
#define R_REF 			10000
#define V_REF 			5
#define ADC_RES 		1024
#define BUFFER_SIZE 		8

// STEPPER 1
#define STEPPER1_PIN1 		7
#define STEPPER1_PIN2 		6
#define STEPPER1_PIN3 		5
#define STEPPER1_PIN4 		4

// STEPPER 2
#define STEPPER2_PIN1 		11
#define STEPPER2_PIN2 		10
#define STEPPER2_PIN3 		9
#define STEPPER2_PIN4 		8

// CONFIG PARAMETER
// Debouncing times
#define BTN_DEBOUNCE_DELAY 	50
#define SNS_DEBOUNCE_DELAY 	20
#define PRINT_CLOCK_DELAY 	100

// Motors
#define MAX_SPEED 		3000
#define ACCELERATION 		1000
#define SPEED			500
#define STEP_PER_ROTATION 	2038

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
	CHECK_HOME_NEEDS,
	EXEC_HOME,
	CONTINUE_HOME,
	UPDATE_HOME,
	SET_TARGETS,
	CHECK_IF_DONE
};

// DEFINE GLOBAL VARIABLES
// Button State variables
int button1State = HIGH;  	// LOW when HOMING REQUESTED
int lastButton1State = HIGH; 	// Used to see only EDGE_NEG triggers
int button2State = HIGH;  	// LOW when CHANGE_CHAR REQUESTED
int home1State = HIGH;    	// MOTOR 1 HOME_SENSOR
int home2State = HIGH;    	// MOTOR 2 HOME_SENSOR
			  
//Button debouncing variables
unsigned long lastButton1Time = 0;
unsigned long lastButton2Time = 0;
unsigned long lastHome1Time = 0;
unsigned long lastHome2Time = 0;
unsigned long lastKeyTime = 0;

//Clock variables for prints and keyboard readings
bool printClock = FALSE;
unsigned long lastClockTime = 0;

//Motor homing states variables
int initHome1Pos = 0;
int initHome2Pos = 0;
int finHome1Pos = 0;
int finHome2Pos = 0;
bool initHome1Upd = FALSE;
bool initHome2Upd = FALSE;
bool finHome1Upd = FALSE;
bool finHome2Upd = FALSE;

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
	// Setup Serial COM
	Serial.begin(9600);
	// SETUP LCD
	lcd.init();
	lcd.backlight();
	lcd.setCursor(0, 0);  // (COLUMN, ROW)
	lcd.print("Buffer: ");//restart from (8, 0)
	lcd.setCursor(0, 1);
	lcd.print("State: ");	//restart from (7, 1)
	
	// SETUP MOTORS
	stepper1.setMaxSpeed(MAX_SPEED);
	stepper1.setAcceleration(ACCELERATION);
	stepper1.setSpeed(SPEED);

	stepper2.setMaxSpeed(MAX_SPEED);
	stepper2.setAcceleration(ACCELERATION);
	stepper2.setSpeed(SPEED);
	
	// START FIRST HOMING
	stepper1.move(2200);
	stepper1.setSpeed(200);
	//stepper2.move(100);
	keyBuffer[0] = 'Q';
	keyBuffer[1] = 'B';
	keyBuffer[2] = 'B';
	keyBufferLength = 3;
	//stepper1.setMinPulseWidth(50);
	//stepper2.setMinPulseWidth(50);
}

// LOOP FUNCTION /////////////////////////////////////////////////////////////
void loop() {
	// Clock for delayed routines
	unsigned long currentMillis = millis();
	if (printClock) {
		printClock = FALSE; 
	} 
	if (currentMillis - lastClockTime > PRINT_CLOCK_DELAY) {
		if (printClock == FALSE) {
			printClock = TRUE;  
		}
		lastClockTime = currentMillis;			
	}
	
	// INPUT BRIDGE: update buttons state, keyboard state and so on...
	// Sensors readings
	home1State = digitalRead(HOME1_PIN);
	home2State = digitalRead(HOME2_PIN);
	// Buttons readings
	lastButton1State = button1State;
	button1State = digitalRead(BUTTON1_PIN);
	button2State = digitalRead(BUTTON2_PIN);

	if (printClock) {  
		lastKeyValue = keyValue;
		keyValue = convertKey(KEY_PIN);
	}

	// Key_Buffer Update
	// *** Esplorare il problema delle doppie ***
	if ((lastKeyValue != keyValue) && (keyBufferLength < BUFFER_SIZE) && (keyValue != 0)) {
		keyBuffer[keyBufferLength] = keyValue;
		keyBufferLength++;
	}

	// ERROR HANDLER: no home trigger after 1 full revolution and some (check current pos) or if 2 buttons pressed together
	// DEBUGGING
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
		Serial.print(stepper2.distanceToGo());
		Serial.print("\n");
	}

	// MAIN STATE MACHINE:
	switch (mainState) {
	
		case START_HOME: 	// OVERRIDE: MANUAL HOME MOTOR 1
			if ((button1State == LOW) && (lastButton1State == HIGH)) {
				stepper1.stop();
				stepper1.setCurrentPosition(0);
				mainState = CONTINUE_HOMING;
				stepper2.move(2200);
				stepper2.setSpeed(200);
			}
			else if (stepper1.distanceToGo() == 0) {
				stepper1.move(2200);
				stepper1.setSpeed(200);
			}
			break;

		case CONTINUE_HOMING: // OVERRIDE: MANUAL HOME MOTOR 2
			if ((button1State == LOW) && (lastButton1State == HIGH)) {
				stepper2.stop();
				stepper2.setCurrentPosition(0);
				mainState = UPDATE_HOME_POS;      
			}
			else if (stepper2.distanceToGo() == 0) {
				stepper2.move(2200);
				stepper2.setSpeed(200);
			}
			break;
	
		case UPDATE_HOME_POS:
			mainState = READY;
			break;
	
		case READY:
			if ((button2State == LOW) && (keyBufferLength > 0)) {
				setTargets(&target1Pos, &target2Pos, keyBuffer[0]);
				mainState = UPDATE_CELL;
			}
			if ((button1State == LOW) && (lastButton1State == HIGH)) {
				stepper1.move(2200);
				stepper1.setSpeed(200);
				mainState = START_HOME;
			}
			break;

		// START of UPDATE_CELL STATE-MACHINE
		case UPDATE_CELL:
			switch (updateState) {
				/*case CHECK_HOME_NEEDS:
					if ((stepper1.currentPosition() > target1Pos) || (stepper2.currentPosition() > target2Pos)) {
						updateState = EXEC_HOME;
						stepper1.move(1000);
						stepper2.move(1000);
					} 
					else updateState = SET_TARGETS;
					break;
	
				case EXEC_HOME:  // AS START_HOMING
					if ((home1State == HIGH) && (stepper1.distanceToGo() == 0)) {
						stepper1.move(1000);
					} 
					else if (initHome1Upd == FALSE) {
						initHome1Pos = stepper1.currentPosition();
						initHome1Upd = TRUE;
						stepper1.stop();
					}
	
					if ((home2State == HIGH) && (stepper2.distanceToGo() == 0)) {
				       		stepper2.move(1000);
					} 
					else if (initHome2Upd == FALSE) {
						initHome2Pos = stepper2.currentPosition();
						initHome2Upd = TRUE;
						stepper2.stop();
					}

					if ((home1State == LOW) && (home2State == LOW)) {
						initHome1Upd = FALSE;
						initHome2Upd = FALSE;
						updateState = CONTINUE_HOME;
						stepper1.move(500);
						stepper2.move(500);
					}
					break;

				case CONTINUE_HOME:  // AS CONTINUE_HOMING
					if ((home1State == LOW) && (stepper1.distanceToGo() == 0)) {
						stepper1.move(500);
					} 
					else if (finHome1Upd == FALSE) {
						finHome1Pos = stepper1.currentPosition();
						finHome1Upd = TRUE;
						stepper1.stop();
					}
	
					if ((home2State == LOW) && (stepper2.distanceToGo() == 0)) {
				       		stepper2.move(500);
					} 
					else if (finHome2Upd == FALSE) {
						finHome2Pos = stepper2.currentPosition();
						finHome2Upd = TRUE;
						stepper2.stop();
					}
	
					if ((home1State == HIGH) && (home2State == HIGH)) {
						finHome1Upd = FALSE;
						finHome2Upd = FALSE;
						updateState = UPDATE_HOME;
					}
					break;
				case UPDATE_HOME:  // AS UPDATE_HOME_POS
					stepper1.setCurrentPosition(finHome1Pos - initHome1Pos);
					stepper2.setCurrentPosition(finHome2Pos - initHome2Pos);
					updateState = SET_TARGETS;
					break;*/
				case SET_TARGETS:
					int stepper1CurrentPosition = stepper1.currentPosition() % STEP_PER_ROTATION;
					int stepper2CurrentPosition = stepper2.currentPosition() % STEP_PER_ROTATION;

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
	
					updateState = CHECK_IF_DONE;
					break;
	
				case CHECK_IF_DONE:
					if ((stepper1.distanceToGo() == 0) && ((stepper2.distanceToGo() == 0))) {
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
			mainState = READY;
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
		lcd.setCursor(7, 1);   // Second Line update
		switch(mainState) {
			case START_HOME:
				lcd.print("START_HOM");
				break;
			case CONTINUE_HOMING:
				lcd.print("CONT_HOME");
				break;
			case UPDATE_HOME_POS:
				lcd.print("UPD_HM_PS");
				break;
			case READY:
				lcd.print("  READY  ");
				break;
			case UPDATE_CELL:
				lcd.print("UPDT_CELL");
				break;
			case UPDATE_COMPLETE:
				lcd.print("UPD_CMPLT");
				break;
		}
	}	
}


// USER FUNCTION //////////////////////////////////////////////////////////
void setTargets(int* target1, int* target2, char character) {
	/*
	 * *target1 = TARGET_POS_0;
	 * *target2 = TARGET_POS_0;
 	 */
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

const char map[] = "ZXVTRPNLJHFDBACEGIKMOQSUWY";
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

char convertKey(int buttonPin) {
	char value = 0;
	float reading = (analogRead(buttonPin) * V_REF) / ADC_RES;  // VOLT LETTI
	float resistor = (reading * R_REF) / (V_REF - reading);
	if (resistor < 500) value = 'Z';
	else if (resistor < 1500) value = 'X';
	else if (resistor < 2500) value = 'V';
	else if (resistor < 3500) value = 'T';
	else if (resistor < 4500) value = 'R';
	else if (resistor < 5500) value = 'P';
	else if (resistor < 6500) value = 'N';
	else if (resistor < 7500) value = 'L';
	else if (resistor < 8500) value = 'J';
	else if (resistor < 9500) value = 'H';
	else if (resistor < 10500) value = 'F';
	else if (resistor < 11500) value = 'D';
	else if (resistor < 12500) value = 'B';
	else if (resistor < 13500) value = 'A';
	else if (resistor < 14500) value = 'C';
	else if (resistor < 15500) value = 'E';
	else if (resistor < 16500) value = 'G';
	else if (resistor < 17500) value = 'I';
	else if (resistor < 18500) value = 'K';
	else if (resistor < 19500) value = 'M';
	else if (resistor < 20500) value = 'O';
	else if (resistor < 21500) value = 'Q';
	else if (resistor < 22500) value = 'S';
	else if (resistor < 23500) value = 'U';
	else if (resistor < 24500) value = 'W';
	else if (resistor < 25500) value = 'Y';
	return value;
}

char convertKey2(int buttonPin) {
	char value = 0;
	int reading = analogRead(buttonPin);
	float resistor = ((R_REF * reading) / (ADC_RES - reading));
	if (resistor < 4461) value = 'Z';
	else if (resistor < 8923) value = 'X';
	else if (resistor < 13385) value = 'V';
	else if (resistor < 17845) value = 'T';
	else if (resistor < 22310) value = 'R';
	else if (resistor < 26770) value = 'P';
	else if (resistor < 31230) value = 'N';
	else if (resistor < 35690) value = 'L';
	else if (resistor < 40155) value = 'J';
	else if (resistor < 44615) value = 'H';
	else if (resistor < 49080) value = 'F';
	else if (resistor < 53540) value = 'D';
	else if (resistor < 58000) value = 'B';
	else if (resistor < 62460) value = 'A';
	else if (resistor < 66920) value = 'C';
	else if (resistor < 71385) value = 'E';
	else if (resistor < 75850) value = 'G';
	else if (resistor < 80310) value = 'I';
	else if (resistor < 84770) value = 'K';
	else if (resistor < 89230) value = 'M';
	else if (resistor < 93690) value = 'O';
	else if (resistor < 98150) value = 'Q';
	else if (resistor < 102615) value = 'S';
	else if (resistor < 107080) value = 'U';
	else if (resistor < 111540) value = 'W';
	else if (resistor < 116000) value = 'Y';
i	return value;
}

void debounceButton(int buttonPin, int* buttonState, unsigned long* lastButtonTime) {
	unsigned long currentMillis = millis();

	if (currentMillis - *lastButtonTime > BTN_DEBOUNCE_DELAY) {
		int reading = digitalRead(buttonPin);
	
		if (reading != *buttonState) {
			*lastButtonTime = currentMillis;
		}

		// inutile: la condizione di questo else è !(reading != *buttonState), quindi reading == buttonState
		/*
		 * else {
		 * 	*buttonState = reading;
		 * }
		 */
	}
}

void debounceKey(char buttonPin, int* buttonState, unsigned long* lastButtonTime) {
	unsigned long currentMillis = millis();

	if (currentMillis - *lastButtonTime > BTN_DEBOUNCE_DELAY) {
		int reading = digitalRead(buttonPin);
		
		if (reading != *buttonState) {
			*lastButtonTime = currentMillis;
		}

		// inutile: la condizione di questo else è !(reading != *buttonState), quindi reading == buttonState 
		/* 
		 * else {
		 * 	*buttonState = reading;
		 * }
		 */
	}
}

/*

  typedef struct {
	int value; 	// Ohm value
	char lcd; 	// LCD character
	int leftPos; 	// Left motor position
	int rightPos; 	// Right motor position
  } braille_char;

  braille_char braille_char_factory(int value, char lcd, int leftPos, int rightPos);

  braille_char braille_char_factory(int value, char lcd, int leftPos, int rightPos) {
	braille_char c;
	c.value = value;
	c.lcd = lcd;
	c.leftPos = leftPos;
	c.rightPos = rightPos;
  } braille_char;

  // *** Da completare con i valori e da inserire in SETUP ***
  braille_char char_a = braille_char_factory(182, 'A', pos_100, pos_000);
  braille_char char_b = braille_char_factory(482, 'B', pos_110, pos_000);
  braille_char char_c = braille_char_factory(982, 'C', pos_100, pos_100);
  braille_char char_d = braille_char_factory(1023, 'D', pos_100, pos_110);
  braille_char char_e = braille_char_factory(*, 'E', pos_100, pos_010);
  braille_char char_f = braille_char_factory(*, 'F', pos_110, pos_100);
  braille_char char_g = braille_char_factory(*, 'G', pos_110, pos_110);
  braille_char char_h = braille_char_factory(*, 'H', pos_110, pos_010);
  braille_char char_i = braille_char_factory(*, 'I', pos_010, pos_100);
  braille_char char_j = braille_char_factory(*, 'J', pos_010, pos_110);
  braille_char char_k = braille_char_factory(*, 'K', pos_101, pos_000);
  braille_char char_l = braille_char_factory(*, 'L', pos_111, pos_000);
  braille_char char_m = braille_char_factory(*, 'M', pos_101, pos_100);
  braille_char char_n = braille_char_factory(*, 'N', pos_101, pos_110);
  braille_char char_o = braille_char_factory(*, 'O', pos_101, pos_010);
  braille_char char_p = braille_char_factory(*, 'P', pos_111, pos_100);
  braille_char char_q = braille_char_factory(*, 'Q', pos_111, pos_110);
  braille_char char_r = braille_char_factory(*, 'R', pos_111, pos_010);
  braille_char char_s = braille_char_factory(*, 'S', pos_011, pos_100);
  braille_char char_t = braille_char_factory(*, 'T', pos_011, pos_100);
  braille_char char_u = braille_char_factory(*, 'U', pos_101, pos_001);
  braille_char char_v = braille_char_factory(*, 'V', pos_111, pos_001);
  braille_char char_w = braille_char_factory(*, 'W', pos_010, pos_111);
  braille_char char_x = braille_char_factory(*, 'X', pos_101, pos_101);
  braille_char char_y = braille_char_factory(*, 'Y', pos_101, pos_111);
  braille_char char_z = braille_char_factory(*, 'Z', pos_101, pos_011);
*/ 
