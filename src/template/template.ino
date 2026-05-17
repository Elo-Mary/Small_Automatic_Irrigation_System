#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ========== 硬件适配器 (针对有源蜂鸣器) ==========
// 很多3针模块是低电平触发。如果烧录后依旧常响，请将 ON 改为 LOW, OFF 改为 HIGH
#define BUZ_ON  LOW  
#define BUZ_OFF HIHG   

// ========== 系统状态定义 ==========
#define STATE_OK      0
#define STATE_WARNING 1
#define STATE_ERROR   2

// ========== 引脚定义 ==========
#define PIN_BUTTON_PLANT   2      
#define PIN_BUTTON_STATE   3      
#define PIN_LED_WORK       4      
#define PIN_BUZZER         5      
#define PIN_RELAY          6      

#define PIN_PHOTO          A0     // 修复了换行Bug
#define PIN_HUMIDITY       A1     
#define PIN_LEVEL          A2     

// ========== OLED 配置 ==========
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ========== 阈值与参数 ==========
#define LIGHT_NEED_IRRIGATE  256  
#define HUMIDITY_NEED_IRRIGATE 256 
#define LOW_WATER_LEVEL      256  
#define PUMP_WORK_TIME       2000 
#define IRRIGATE_COOLDOWN    10000
#define DISPLAY_UPDATE_MS    300  
#define DEBOUNCE_DELAY       50

const char* plantName[3] = {"Cactus", "Pothos", "Fern"};
const char* stateName[2] = {"Home", "Trivial"};

// ========== 全局变量 ==========
int systemState = STATE_OK;
int plantMode = 0;
int stateMode = 0;          

bool isIrrigating = false;
unsigned long irrigateStartTime = 0;

int lastBtnPlant = HIGH;
int lastBtnState = HIGH;
// 修复：按键独立的消抖时间戳
unsigned long lastBtnPlantTime = 0; 
unsigned long lastBtnStateTime = 0; 

enum BuzzerTask { BUZZ_OFF, BUZZ_WARNING_SLOW, BUZZ_WARNING_FAST, BUZZ_SHORT_BEEP };
BuzzerTask buzzTask = BUZZ_OFF;
unsigned long buzzTimer = 0;
unsigned long buzzBeepStart = 0;
bool buzzPhase = false;
unsigned long lastDisplayUpdate = 0;

// ========== 系统自检 ==========
int systemSelfCheck(int waterLevel) {
  if (waterLevel == 0) return STATE_ERROR;
  if (waterLevel < LOW_WATER_LEVEL) return STATE_WARNING;
  return STATE_OK;
}

// ========== 非阻塞按钮检测 ==========
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

// ========== 灌溉动作 ==========
void startIrrigate() {
  if (isIrrigating) return;
  isIrrigating = true;
  irrigateStartTime = millis();
  digitalWrite(PIN_RELAY, LOW);     
  digitalWrite(PIN_LED_WORK, HIGH); 
  
  buzzTask = BUZZ_SHORT_BEEP;
  buzzBeepStart = millis();
  digitalWrite(PIN_BUZZER, BUZ_ON); // 修复为电平控制
}

void checkIrrigateStop() {
  if (isIrrigating) {
    if (millis() - irrigateStartTime >= PUMP_WORK_TIME) {
      digitalWrite(PIN_RELAY, HIGH);
      digitalWrite(PIN_LED_WORK, LOW);
      isIrrigating = false;
      buzzTask = BUZZ_SHORT_BEEP;
      buzzBeepStart = millis();
      digitalWrite(PIN_BUZZER, BUZ_ON); // 修复为电平控制
    }
  }
}

// ========== 是否需要浇水 ==========
bool isNeedIrrigate(int illuminance, int humidity) {
  if (systemState == STATE_ERROR) return false;
  if (!isIrrigating && (millis() - irrigateStartTime) < IRRIGATE_COOLDOWN && irrigateStartTime != 0) return false;
  return (illuminance < LIGHT_NEED_IRRIGATE && humidity < HUMIDITY_NEED_IRRIGATE);
}

// ========== 更新板载 LED ==========
void updateStatusLED() {
  static unsigned long ledTimer = 0;
  static bool ledState = false;
  unsigned long interval = 0;
  
  if (systemState == STATE_ERROR)       interval = 200;
  else if (systemState == STATE_WARNING) interval = 800;
  else {
    digitalWrite(LED_BUILTIN, LOW); // 修复：OK时应当熄灭，否则太刺眼
    return;
  }

  if (millis() - ledTimer >= interval) {
    ledTimer = millis();
    ledState = !ledState;
    digitalWrite(LED_BUILTIN, ledState);
  }
}

// ========== 集中管理蜂鸣器 ==========
void handleBuzzer() {
  if (buzzTask == BUZZ_SHORT_BEEP) {
    if (millis() - buzzBeepStart > 200) {
      digitalWrite(PIN_BUZZER, BUZ_OFF); // 修复为电平控制
      buzzTask = BUZZ_OFF;
    }
    return;
  }

  BuzzerTask desiredTask = BUZZ_OFF;
  if (systemState == STATE_ERROR)       desiredTask = BUZZ_WARNING_FAST;
  else if (systemState == STATE_WARNING) desiredTask = BUZZ_WARNING_SLOW;
  if (isIrrigating) desiredTask = BUZZ_OFF;
  
  if (buzzTask != desiredTask) {
    buzzTask = desiredTask;
    buzzTimer = millis();
    buzzPhase = false;
    if (desiredTask == BUZZ_OFF) digitalWrite(PIN_BUZZER, BUZ_OFF);
  }

  if (buzzTask == BUZZ_WARNING_SLOW || buzzTask == BUZZ_WARNING_FAST) {
    unsigned long interval = (buzzTask == BUZZ_WARNING_FAST) ? 300 : 800;
    if (millis() - buzzTimer >= interval) {
      buzzTimer = millis();
      buzzPhase = !buzzPhase;
      if (buzzPhase) {
        digitalWrite(PIN_BUZZER, BUZ_ON); // 修复为电平控制
      } else {
        digitalWrite(PIN_BUZZER, BUZ_OFF); // 修复为电平控制
      }
    }
  }
}

// ========== OLED 显示更新 ==========
void updateDisplay(int illuminance, int humidity, int waterLevel) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print("Plant: "); display.print(plantName[plantMode]);

  display.setCursor(0, 10);
  display.print("Mode: "); display.print(stateName[stateMode]);

  display.setCursor(0, 22);
  display.print("L:"); display.print(illuminance);
  display.print(" H:"); display.print(humidity);

  display.setCursor(0, 34);
  display.print("Water: ");
  if (waterLevel < LOW_WATER_LEVEL) display.print("Low ");
  else display.print("OK  ");

  display.setCursor(0, 46);
  if (isIrrigating) display.print("Watering...");
  else if (systemState == STATE_ERROR) display.print("ERROR: No Water");
  else if (systemState == STATE_WARNING) display.print("WARN: Low Water");
  else display.print("System OK");

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
  digitalWrite(PIN_BUZZER, BUZ_OFF); // 修复：初始化时强制关闭蜂鸣器

  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    while(1) {
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
  int illuminance = analogRead(PIN_PHOTO);
  int humidity = analogRead(PIN_HUMIDITY);
  int waterLevel = analogRead(PIN_LEVEL);
  systemState = systemSelfCheck(waterLevel);

  // 修复：使用各自独立的时间戳
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