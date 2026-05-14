#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ========== 系统状态定义 ==========
#define STATE_OK      0
#define STATE_WARNING 1
#define STATE_ERROR   2

// ========== 引脚定义（严格按照接口文档） ==========
#define PIN_BUTTON_PLANT   2      // 切换植物种类按钮
#define PIN_BUTTON_STATE   3      // 切换模式按钮
#define PIN_LED_WORK       4      // 浇水指示灯（外部LED）
#define PIN_BUZZER         5      // 蜂鸣器模块 I/O
#define PIN_RELAY          6      // 继电器 IN（控制水泵）

#define PIN_PHOTO          A0     // 光照传感器 AO
#define PIN_HUMIDITY       A1     // 土壤湿度传感器 AUOT
#define PIN_LEVEL          A2     // 水位传感器 S

// ========== OLED 配置 ==========
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ========== 阈值与参数 ==========
#define LIGHT_NEED_IRRIGATE  256  // 光照低于此值允许浇水（配合湿度判断）
#define HUMIDITY_NEED_IRRIGATE 256 // 湿度低于此值允许浇水（template 逻辑）
#define LOW_WATER_LEVEL      256  // 水位传感器低于此值触发 WARNING
#define PUMP_WORK_TIME       2000 // 水泵每次工作时长(ms)
#define IRRIGATE_COOLDOWN    10000// 两次浇水最小间隔(ms)
#define DISPLAY_UPDATE_MS    300  // 屏幕刷新周期(ms)

// ========== 植物与模式名称（与 template 一致） ==========
const char* plantName[3] = {"Cactus", "Pothos", "Fern"};
const char* stateName[2] = {"Home", "Trivial"};

// ========== 全局变量 ==========
int systemState = STATE_OK;
int plantMode = 0;          // 0,1,2
int stateMode = 0;          // 0=Home, 1=Trivial

// 非阻塞灌溉状态机
bool isIrrigating = false;
unsigned long irrigateStartTime = 0;

// 非阻塞按钮相关
int lastBtnPlant = HIGH;
int lastBtnState = HIGH;
unsigned long lastBtnDebounceTime = 0;
#define DEBOUNCE_DELAY 50

// 蜂鸣器集中管理
enum BuzzerTask { BUZZ_OFF, BUZZ_WARNING_SLOW, BUZZ_WARNING_FAST, BUZZ_SHORT_BEEP };
BuzzerTask buzzTask = BUZZ_OFF;
unsigned long buzzTimer = 0;
unsigned long buzzBeepStart = 0;
bool buzzPhase = false;

// 显示刷新计时
unsigned long lastDisplayUpdate = 0;

// ========== 系统自检（与 template 一致） ==========
int systemSelfCheck(int waterLevel) {
  if (waterLevel == 0) {
    return STATE_ERROR;
  }
  if (waterLevel < LOW_WATER_LEVEL) {
    return STATE_WARNING;
  }
  return STATE_OK;
}

// ========== 非阻塞按钮检测（检测按下瞬间） ==========
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

// ========== 灌溉动作（非阻塞状态机） ==========
void startIrrigate() {
  if (isIrrigating) return;
  isIrrigating = true;
  irrigateStartTime = millis();
  digitalWrite(PIN_RELAY, LOW);     // 开启继电器
  digitalWrite(PIN_LED_WORK, HIGH); // 亮起工作指示灯
  buzzTask = BUZZ_SHORT_BEEP;
  buzzBeepStart = millis();
  tone(PIN_BUZZER, 2000);
}

void checkIrrigateStop() {
  if (isIrrigating) {
    if (millis() - irrigateStartTime >= PUMP_WORK_TIME) {
      digitalWrite(PIN_RELAY, HIGH);
      digitalWrite(PIN_LED_WORK, LOW);
      isIrrigating = false;
      buzzTask = BUZZ_SHORT_BEEP;
      buzzBeepStart = millis();
      tone(PIN_BUZZER, 1500);
    }
  }
}

// ========== 是否需要浇水（与 template 条件一致） ==========
bool isNeedIrrigate(int illuminance, int humidity) {
  if (systemState == STATE_ERROR) return false;
  if (!isIrrigating && (millis() - irrigateStartTime) < IRRIGATE_COOLDOWN && irrigateStartTime != 0) return false;
  return (illuminance < LIGHT_NEED_IRRIGATE && humidity < HUMIDITY_NEED_IRRIGATE);
}

// ========== 更新板载 LED 状态灯 ==========
void updateStatusLED() {
  static unsigned long ledTimer = 0;
  static bool ledState = false;
  unsigned long interval = 0;

  if (systemState == STATE_ERROR)       interval = 200;
  else if (systemState == STATE_WARNING) interval = 800;
  else {
    digitalWrite(LED_BUILTIN, HIGH);
    return;
  }

  if (millis() - ledTimer >= interval) {
    ledTimer = millis();
    ledState = !ledState;
    digitalWrite(LED_BUILTIN, ledState);
  }
}

// ========== 集中管理蜂鸣器（非阻塞） ==========
void handleBuzzer() {
  if (buzzTask == BUZZ_SHORT_BEEP) {
    if (millis() - buzzBeepStart > 200) {
      noTone(PIN_BUZZER);
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
    if (desiredTask == BUZZ_OFF) noTone(PIN_BUZZER);
  }

  if (buzzTask == BUZZ_WARNING_SLOW || buzzTask == BUZZ_WARNING_FAST) {
    unsigned long interval = (buzzTask == BUZZ_WARNING_FAST) ? 300 : 800;
    if (millis() - buzzTimer >= interval) {
      buzzTimer = millis();
      buzzPhase = !buzzPhase;
      if (buzzPhase) {
        tone(PIN_BUZZER, (buzzTask == BUZZ_WARNING_FAST) ? 3000 : 2000);
      } else {
        noTone(PIN_BUZZER);
      }
    }
  }
}

// ========== OLED 显示更新（无闪烁，固定周期刷新） ==========
void updateDisplay(int illuminance, int humidity, int waterLevel) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // 第一行：植物
  display.setCursor(0, 0);
  display.print("Plant: ");
  display.print(plantName[plantMode]);

  // 第二行：模式
  display.setCursor(0, 10);
  display.print("Mode: ");
  display.print(stateName[stateMode]);

  // 第三行：光照和湿度
  display.setCursor(0, 22);
  display.print("L:");
  display.print(illuminance);
  display.print(" H:");
  display.print(humidity);

  // 第四行：水位状态
  display.setCursor(0, 34);
  display.print("Water: ");
  if (waterLevel < LOW_WATER_LEVEL) display.print("Low ");
  else display.print("OK  ");

  // 第五行：系统状态或浇水提示
  display.setCursor(0, 46);
  if (isIrrigating) {
    display.print("Watering...");
  } else if (systemState == STATE_ERROR) {
    display.print("ERROR: No Water");
  } else if (systemState == STATE_WARNING) {
    display.print("WARN: Low Water");
  } else {
    display.print("System OK");
  }

  display.display();
}

// ========== 初始化 ==========
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

  // OLED 初始化
  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    // 如果初始化失败，板载 LED 快闪报错
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

// ========== 主循环 ==========
void loop() {
  int illuminance = analogRead(PIN_PHOTO);
  int humidity = analogRead(PIN_HUMIDITY);
  int waterLevel = analogRead(PIN_LEVEL);

  systemState = systemSelfCheck(waterLevel);

  // 按钮处理
  if (readButtonPress(PIN_BUTTON_PLANT, lastBtnPlant, lastBtnDebounceTime)) {
    plantMode = (plantMode + 1) % 3;
    buzzTask = BUZZ_SHORT_BEEP;
    buzzBeepStart = millis();
    tone(PIN_BUZZER, 1000);
  }
  if (readButtonPress(PIN_BUTTON_STATE, lastBtnState, lastBtnDebounceTime)) {
    stateMode = (stateMode + 1) % 2;
    buzzTask = BUZZ_SHORT_BEEP;
    buzzBeepStart = millis();
    tone(PIN_BUZZER, 1200);
  }

  // 灌溉逻辑
  if (!isIrrigating && isNeedIrrigate(illuminance, humidity)) {
    startIrrigate();
  }
  if (isIrrigating) {
    checkIrrigateStop();
  }

  updateStatusLED();
  handleBuzzer();

  // 定时刷新显示
  if (millis() - lastDisplayUpdate >= DISPLAY_UPDATE_MS) {
    lastDisplayUpdate = millis();
    updateDisplay(illuminance, humidity, waterLevel);
  }

  delay(10);
}
