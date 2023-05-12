#include <PS2Keyboard.h>

const int DataPin = 8;
const int IRQpin =  2;

PS2Keyboard keyboard;

void setup() {
delay(1000);
keyboard.begin(DataPin, IRQpin);
}

void loop() {
if (keyboard.available()) {

char c = keyboard.read();

if (c == ‘a’) {
p();l();
} else if (c == ‘b’) {
l();p();p();p();
} else if (c == ‘c’) {
l();p();l();p();
} else if (c == ‘d’) {
l();p();p();
} else if (c == ‘e’) {
p();
} else if (c == ‘f’) {
p();p();l();p();
} else if (c == ‘g’) {
l();l();p();
} else if (c == ‘h’) {
p();p();p();p();
} else if (c == ‘i’) {
p();p();
} else if (c == ‘j’) {
p();l();l();l();
} else if (c == ‘k’) {
l();p();l();
} else if (c == ‘l’) {
p();l();p();p();
} else if (c == ‘m’) {
l();l();
} else if (c == ‘n’) {
l();p();
} else if (c == ‘o’) {
l();l();l();
} else if (c == ‘p’) {
p();l();l();p();
} else if (c == ‘q’) {
l();l();p();l();
} else if (c == ‘r’) {
p();l();p();
} else if (c == ‘s’) {
p();p();p();
} else if (c == ‘t’) {
l();
} else if (c == ‘u’) {
p();p();l();
} else if (c == ‘v’) {
p();p();p();l();
} else if (c == ‘w’) {
p();l();l();
} else if (c == ‘x’) {
l();p();p();l();
} else if (c == ‘y’) {
l();p();l();l();
} else if (c == ‘z’) {
l();l();p();p();
} else if (c == ‘1’) {
p();l();l();l();l();
} else if (c == ‘2’) {
p();p();l();l();l();
} else if (c == ‘3’) {
p();p();p();l();l();
} else if (c == ‘4’) {
p();p();p();p();l();
} else if (c == ‘5’) {
p();p();p();p();p();
} else if (c == ‘6’) {
l();p();p();p();p();
} else if (c == ‘7’) {
l();l();p();p();p();
} else if (c == ‘8’) {
l();l();l();p();p();
} else if (c == ‘9’) {
l();l();l();l();p();
} else if (c == ‘0’) {
l();l();l();l();l();
} else if (c == ‘ ‘) {
pausa();
} else {
tone(4, 300, 250); //error tone
}
}
}
