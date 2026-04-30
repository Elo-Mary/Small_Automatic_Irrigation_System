#include <LiquidCrystal_I2C.h>

#define STATE int
#define ERROR 1
#define WARNING 2
#define OK 0

#define PIN_LED_POWER LED_BUILTIN
#define PIN_LED_ERROR 2
#define PIN_LED_WARNING 3
#define PIN_LED_WORKING 4

#define PIN_BUTTON_PLANT 6
#define PIN_BUTTON_STATE 7

#define PIN_PHOTO 0
#define PIN_HUMIDITY 1
#define PIN_LEVEL 2

const int plantThreshold[3] = {20, 45, 70};
const char *plantName[3] = {"Cactus", "Pothos", "Fern"};

const int stateThreshold[2] = {20, 45};
const char *stateName[2] = {"Home", "Trivial"};

LiquidCrystal_I2C lcd(0x27, 20, 4);
STATE systemState = OK;

int plantMode = 0;
int stateMode = 0;

STATE systemSelfCheck() {
  int level = analogRead(PIN_LEVEL);
  if (level == 0) {
    return ERROR;
  }

  if (level < 256) {
    return WARNING;
  }

  return OK;
}

void changePlantMode() { plantMode = (plantMode + 1) % 3; }

void changeStateMode() { stateMode = (stateMode + 1) % 2; }

bool isNeedIrrigate(int illuminance, int humidity) {
  return illuminance < 256 && humidity < 256;
}

bool isPressButton(int pin) {
  if (digitalRead(pin) == LOW) {
    delay(50);
    while (digitalRead(pin) == LOW) {
    }
    return true;
  }
  return false;
}

void showState(int illuminance, int humidity) {
  lcd.setCursor(0, 0);
  lcd.print("PLANT: ");
  lcd.print(plantName[plantMode]);

  lcd.setCursor(0, 1);
  lcd.print("STATE: ");
  lcd.print(stateName[stateMode]);

  lcd.setCursor(0, 2);
  lcd.println(illuminance);

  lcd.setCursor(0, 3);
  lcd.println(humidity);
}

void irrigate() {
  digitalWrite(PIN_LED_WORKING, HIGH);
  delay(500);
  digitalWrite(PIN_LED_WORKING, LOW);
}

void setup() {
  lcd.init();
  lcd.backlight();

  pinMode(PIN_BUTTON_PLANT, INPUT_PULLUP);
  pinMode(PIN_BUTTON_STATE, INPUT_PULLUP);

  pinMode(PIN_LED_ERROR, OUTPUT);
  pinMode(PIN_LED_WARNING, OUTPUT);
  pinMode(PIN_LED_WORKING, OUTPUT);
}

void loop() {
  lcd.clear();

  systemState = systemSelfCheck();
  if (systemState == ERROR) {
    digitalWrite(PIN_LED_ERROR, HIGH);
  } else {
    digitalWrite(PIN_LED_ERROR, LOW);
  }

  if (systemState == WARNING) {
    digitalWrite(PIN_LED_WARNING, HIGH);
  } else {
    digitalWrite(PIN_LED_WARNING, LOW);
  }

  if (isPressButton(PIN_BUTTON_PLANT)) {
    changePlantMode();
  }

  if (isPressButton(PIN_BUTTON_STATE)) {
    changeStateMode();
  }

  int illuminance = analogRead(PIN_PHOTO);
  int humidity = analogRead(PIN_HUMIDITY);
  showState(illuminance, humidity);

  if (isNeedIrrigate(illuminance, humidity) && systemState != ERROR) {
    irrigate();
  }

  delay(100);
}
