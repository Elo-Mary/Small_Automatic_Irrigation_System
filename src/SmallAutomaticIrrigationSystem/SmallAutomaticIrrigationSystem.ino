#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

/*
 * 盆栽自动浇灌系统
 *
 * 传感器读数约定：
 * - 土壤湿度读数越大，表示土壤越干。
 * - 光照读数越大，表示环境越暗。
 * - 水位读数越小，表示水箱水量越少。
 */

// 有源蜂鸣器电平约定；如果实际模块触发电平相反，只需要调整这两个宏。
#define BUZ_ON LOW
#define BUZ_OFF HIGH

// 水箱状态分级，用于 LED、蜂鸣器和 OLED 显示。
#define STATE_OK 0
#define STATE_WARNING 1
#define STATE_ERROR 2

// Arduino 与外设的引脚分配。
#define PIN_BUTTON_PLANT 2
#define PIN_BUTTON_STATE 3
#define PIN_LED_WORK 4
#define PIN_BUZZER 5
#define PIN_RELAY 6

#define PIN_PHOTO A0
#define PIN_HUMIDITY A1
#define PIN_LEVEL A2

// OLED 屏幕参数。
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// 阈值和时间参数集中放置，便于根据传感器实测值重新标定。
#define LIGHT_NEED_IRRIGATE 256
#define HUMIDITY_NEED_IRRIGATE 384
#define NO_WATER_LEVEL 32
#define LOW_WATER_LEVEL 256
#define PUMP_WORK_TIME 2000
#define IRRIGATE_COOLDOWN 10000
#define DISPLAY_UPDATE_MS 300
#define DEBOUNCE_DELAY 50

// OLED 使用英文标签，避免默认字库在部分环境中无法显示中文。
const char *plantName[3] = { "Cactus", "Pothos", "Fern" };
const char *stateName[2] = { "Home", "Trivial" };

// 主循环中共享的系统状态。
int systemState = STATE_OK;
int plantMode = 0;
int stateMode = 0;

int illuminance = 0;
int humidity = 0;
int waterLevel = 0;

bool isIrrigating = false;
unsigned long irrigateStartTime = 0;

int lastBtnPlant = HIGH;
int lastBtnState = HIGH;
unsigned long lastBtnPlantTime = 0;
unsigned long lastBtnStateTime = 0;

// 蜂鸣器任务用于生成短响、慢速告警和快速告警。
enum BuzzerTask {
  BUZZ_OFF,
  BUZZ_WARNING_SLOW,
  BUZZ_WARNING_FAST,
  BUZZ_SHORT_BEEP
};
BuzzerTask buzzTask = BUZZ_OFF;
unsigned long buzzTimer = 0;
unsigned long buzzBeepStart = 0;
bool buzzPhase = false;
unsigned long lastDisplayUpdate = 0;

// 根据水位读数判断水箱是否足够安全。
int systemSelfCheck(int waterLevel) {
  if (waterLevel < NO_WATER_LEVEL)
    return STATE_ERROR;
  if (waterLevel < LOW_WATER_LEVEL)
    return STATE_WARNING;
  return STATE_OK;
}

// 带消抖的按键扫描，按钮按下时读数为 LOW。
bool readButtonPress(int pin, int &lastState, unsigned long &lastDebounce) {
  int reading = digitalRead(pin);
  if (reading != lastState) {
    lastDebounce = millis();
    lastState = reading;
  }
  if ((millis() - lastDebounce) > DEBOUNCE_DELAY) {
    if (reading == LOW) {
      lastDebounce = millis();
      return true;
    }
  }
  return false;
}

// 启动一次非阻塞浇水流程，由继电器控制水泵通断。
void startIrrigate() {
  if (isIrrigating)
    return;
  isIrrigating = true;
  irrigateStartTime = millis();
  digitalWrite(PIN_RELAY, LOW);
  digitalWrite(PIN_LED_WORK, HIGH);

  buzzTask = BUZZ_SHORT_BEEP;
  buzzBeepStart = millis();
  digitalWrite(PIN_BUZZER, BUZ_ON);
}

// 到达设定工作时间后关闭水泵，并给出一次短响提示。
void checkIrrigateStop() {
  if (isIrrigating) {
    if (millis() - irrigateStartTime >= PUMP_WORK_TIME) {
      digitalWrite(PIN_RELAY, HIGH);
      digitalWrite(PIN_LED_WORK, LOW);
      isIrrigating = false;
      buzzTask = BUZZ_SHORT_BEEP;
      buzzBeepStart = millis();
      digitalWrite(PIN_BUZZER, BUZ_ON);
    }
  }
}

// 判断当前传感器状态是否允许触发浇水。
bool isNeedIrrigate(int illuminance, int humidity) {
  if (systemState == STATE_ERROR)
    return false;
  if (!isIrrigating && (millis() - irrigateStartTime) < IRRIGATE_COOLDOWN && irrigateStartTime != 0)
    return false;
  return (illuminance < LIGHT_NEED_IRRIGATE && humidity > HUMIDITY_NEED_IRRIGATE);
}

// 根据水箱状态闪烁板载 LED。
void updateStatusLED() {
  static unsigned long ledTimer = 0;
  static bool ledState = false;
  unsigned long interval = 0;

  if (systemState == STATE_ERROR)
    interval = 200;
  else if (systemState == STATE_WARNING)
    interval = 800;
  else {
    digitalWrite(LED_BUILTIN, LOW);
    return;
  }

  if (millis() - ledTimer >= interval) {
    ledTimer = millis();
    ledState = !ledState;
    digitalWrite(LED_BUILTIN, ledState);
  }
}

// 统一驱动蜂鸣器，避免告警逻辑阻塞主循环。
void handleBuzzer() {
  if (illuminance > LIGHT_NEED_IRRIGATE) {
    buzzTask = BUZZ_OFF;
    digitalWrite(PIN_BUZZER, BUZ_OFF);
  }

  if (buzzTask == BUZZ_SHORT_BEEP) {
    if (millis() - buzzBeepStart > 200) {
      digitalWrite(PIN_BUZZER, BUZ_OFF);
      buzzTask = BUZZ_OFF;
    }
    return;
  }

  BuzzerTask desiredTask = BUZZ_OFF;
  if (systemState == STATE_ERROR)
    desiredTask = BUZZ_WARNING_FAST;
  else if (systemState == STATE_WARNING)
    desiredTask = BUZZ_WARNING_SLOW;
  if (isIrrigating)
    desiredTask = BUZZ_OFF;

  if (buzzTask != desiredTask) {
    buzzTask = desiredTask;
    buzzTimer = millis();
    buzzPhase = false;
    if (desiredTask == BUZZ_OFF)
      digitalWrite(PIN_BUZZER, BUZ_OFF);
  }

  if (buzzTask == BUZZ_WARNING_SLOW || buzzTask == BUZZ_WARNING_FAST) {
    unsigned long interval = (buzzTask == BUZZ_WARNING_FAST) ? 300 : 800;
    if (millis() - buzzTimer >= interval) {
      buzzTimer = millis();
      buzzPhase = !buzzPhase;
      if (buzzPhase) {
        digitalWrite(PIN_BUZZER, BUZ_ON);
      } else {
        digitalWrite(PIN_BUZZER, BUZ_OFF);
      }
    }
  }
}

// 刷新 OLED 状态页。
void updateDisplay(int illuminance, int humidity, int waterLevel) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print("Plant: ");
  display.print(plantName[plantMode]);

  display.setCursor(0, 10);
  display.print("Mode: ");
  display.print(stateName[stateMode]);

  display.setCursor(0, 22);
  display.print("L:");
  display.print(illuminance);
  display.print(" H:");
  display.print(humidity);

  display.setCursor(0, 34);
  display.print("Water: ");
  if (waterLevel < LOW_WATER_LEVEL)
    display.print("Low ");
  else
    display.print("OK  ");

  display.setCursor(0, 46);
  if (isIrrigating)
    display.print("Watering...");
  else if (systemState == STATE_ERROR)
    display.print("ERROR: No Water");
  else if (systemState == STATE_WARNING)
    display.print("WARN: Low Water");
  else
    display.print("System OK");

  display.display();
}

void setup() {
  pinMode(PIN_BUTTON_PLANT, INPUT_PULLUP);
  pinMode(PIN_BUTTON_STATE, INPUT_PULLUP);
  pinMode(PIN_LED_WORK, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  digitalWrite(PIN_RELAY, HIGH);
  digitalWrite(PIN_LED_WORK, LOW);
  digitalWrite(LED_BUILTIN, LOW);
  digitalWrite(PIN_BUZZER, BUZ_OFF);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    while (1) {
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      delay(100);
    }
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Auto Watering...");
  display.display();
  delay(1500);
}

void loop() {
  illuminance = analogRead(PIN_PHOTO);
  humidity = analogRead(PIN_HUMIDITY);
  waterLevel = analogRead(PIN_LEVEL);
  systemState = systemSelfCheck(waterLevel);

  if (readButtonPress(PIN_BUTTON_PLANT, lastBtnPlant, lastBtnPlantTime)) {
    plantMode = (plantMode + 1) % 3;
    buzzTask = BUZZ_SHORT_BEEP;
    buzzBeepStart = millis();
    digitalWrite(PIN_BUZZER, BUZ_ON);
  }
  if (readButtonPress(PIN_BUTTON_STATE, lastBtnState, lastBtnStateTime)) {
    stateMode = (stateMode + 1) % 2;
    buzzTask = BUZZ_SHORT_BEEP;
    buzzBeepStart = millis();
    digitalWrite(PIN_BUZZER, BUZ_ON);
  }

  if (!isIrrigating && isNeedIrrigate(illuminance, humidity)) {
    startIrrigate();
  }
  if (isIrrigating) {
    checkIrrigateStop();
  }

  updateStatusLED();
  handleBuzzer();

  if (millis() - lastDisplayUpdate >= DISPLAY_UPDATE_MS) {
    lastDisplayUpdate = millis();
    updateDisplay(illuminance, humidity, waterLevel);
  }

  delay(10);
}
