/* 
SƠ ĐỒ CHÂN PHẦN CỨNG (ESP32 DevKit V1 30-pin):
- Relay (Bơm) : GPIO 25
- Nút bấm 1    : GPIO 27 (Bật/tắt Bơm + Sáng LCD + Chuyển MANUAL)
- Nút bấm 2    : GPIO 14 (Chỉ sáng LCD)
- Cảm biến Đất : GPIO 34 (Analog)
- Cảm biến DHT : GPIO 4  (DHT11)
- Màn hình LCD : GPIO 21 (SDA), GPIO 22 (SCL)

SƠ ĐỒ CHÂN ẢO BLYNK (VIRTUAL PINS):
- V0  : Nhiệt độ không khí (°C)
- V1  : Độ ẩm không khí (%)
- V2  : Công tắc chuyển chế độ AUTO / MANUAL (Switch)
- V3  : Độ ẩm đất (%)
- V12 : Nút bấm bật/tắt Bơm trên App
*/

#define BLYNK_TEMPLATE_ID "TMPL6sBsqMyuI"
#define BLYNK_TEMPLATE_NAME "VuonThongMinh"
#define BLYNK_PRINT Serial

#include <Wire.h>
#include <IskakINO_LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>

// Cấu hình mạng & Blynk
char auth[] = "i4Q8S5HmKz0Wa1PazOG5XA9WbIq2U04-";  
char ssid[] = "";   
char pass[] = "";   

#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define soil           34          
#define RELAY_PIN_1    25
#define PUSH_BUTTON_1  27
#define PUSH_BUTTON_2  14
#define VPIN_BUTTON_1  V12
#define VPIN_AUTO_MODE V2

LiquidCrystal_I2C lcd(16, 2); 

// Ngưỡng cài đặt độ ẩm đất cho chế độ AUTO
const int SOIL_THRESHOLD_LOW  = 30; // Độ ẩm < 30%: Tự động BẬT bơm
const int SOIL_THRESHOLD_HIGH = 60; // Độ ẩm > 60%: Tự động TẮT bơm

// Biến quản lý trạng thái
int relay1State = LOW;
int pushButton1State = HIGH;
int pushButton2State = HIGH;

bool isAutoMode = true; // Mặc định bật chế độ AUTO khi khởi động

unsigned long lastBacklightTime = 0;
const long backlightTimeout = 10000; // Tắt đèn nền LCD sau 10 giây
bool isBacklightOn = true;

// Task Handle cho FreeRTOS
TaskHandle_t TaskBlynkHandle;
TaskHandle_t TaskSensorsHandle;

// Bật đèn nền LCD và đếm lại 10s
void wakeUpLCD() {
  lcd.backlight();
  isBacklightOn = true;
  lastBacklightTime = millis();
}

// Tự động tắt đèn nền LCD khi hết 10s
void checkBacklightTimeout() {
  if (isBacklightOn && (millis() - lastBacklightTime > backlightTimeout)) {
    lcd.noBacklight();
    isBacklightOn = false;
  }
}

// Cập nhật trạng thái W:ON / W:OFF trên LCD
void updateWaterStatusLCD() {
  lcd.setCursor(11, 1);
  if (relay1State == HIGH) {
    lcd.print("W:ON ");
  } else {
    lcd.print("W:OFF");
  }
}

// Đọc cảm biến nhiệt độ & độ ẩm không khí
void DHT11sensor() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) return;

  Blynk.virtualWrite(V0, t);
  Blynk.virtualWrite(V1, h);

  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print((int)t);
  lcd.print((char)223); 
  lcd.print("C ");

  lcd.setCursor(8, 0);
  lcd.print("H:");
  lcd.print((int)h);
  lcd.print("%  ");
}

// Đọc cảm biến độ ẩm đất & xử lý tưới tự động
void soilMoistureSensor() {
  int value = analogRead(soil);               
  // Ánh xạ 2900 -> 1550 giúp map trực tiếp từ khô (0%) sang ướt (100%)
  value = map(value, 2900, 1550, 0, 100); 

  Blynk.virtualWrite(V3, value);

  lcd.setCursor(0, 1);
  lcd.print("S:");
  lcd.print(value);
  lcd.print("%   ");

  // LOGIC TƯỚI TỰ ĐỘNG (Chỉ hoạt động khi đang ở chế độ AUTO)
  if (isAutoMode) {
    // Đất khô (< 30%) và bơm đang TẮT -> Tự động BẬT bơm
    if (value < SOIL_THRESHOLD_LOW && relay1State == LOW) {
      relay1State = HIGH;
      digitalWrite(RELAY_PIN_1, relay1State);

      Blynk.virtualWrite(VPIN_BUTTON_1, relay1State);
      updateWaterStatusLCD();
      wakeUpLCD();
    } 
    // Đất đủ ẩm (> 60%) và bơm đang BẬT -> Tự động TẮT bơm
    else if (value > SOIL_THRESHOLD_HIGH && relay1State == HIGH) {
      relay1State = LOW;
      digitalWrite(RELAY_PIN_1, relay1State);

      Blynk.virtualWrite(VPIN_BUTTON_1, relay1State);
      updateWaterStatusLCD();
    }
  }
}

// Đọc 2 nút bấm vật lý
void checkPhysicalButton() {
  // Nút 1: Bật/Tắt Bơm + Mở đèn LCD + Chuyển sang chế độ MANUAL
  if (digitalRead(PUSH_BUTTON_1) == LOW) {
    if (pushButton1State != LOW) {
      relay1State = !relay1State;
      digitalWrite(RELAY_PIN_1, relay1State);

      // Tắt AUTO, chuyển sang MANUAL khi người dùng can thiệp bằng nút bấm
      isAutoMode = false;
      Blynk.virtualWrite(VPIN_AUTO_MODE, 0);

      Blynk.virtualWrite(VPIN_BUTTON_1, relay1State);
      updateWaterStatusLCD();
      wakeUpLCD();
    }
    pushButton1State = LOW;
  } else {
    pushButton1State = HIGH;
  }

  // Nút 2: Chỉ mở đèn LCD
  if (digitalRead(PUSH_BUTTON_2) == LOW) {
    if (pushButton2State != LOW) {
      wakeUpLCD();
    }
    pushButton2State = LOW;
  } else {
    pushButton2State = HIGH;
  }
}

// Đồng bộ trạng thái từ ứng dụng Blynk khi vừa kết nối thành công
BLYNK_CONNECTED() {
  Blynk.syncVirtual(VPIN_BUTTON_1);
  Blynk.syncVirtual(VPIN_AUTO_MODE);
}

// Nhận lệnh bật/tắt Bơm từ App Blynk
BLYNK_WRITE(VPIN_BUTTON_1) {
  relay1State = param.asInt();
  digitalWrite(RELAY_PIN_1, relay1State);

  // Tắt AUTO, chuyển sang MANUAL khi người dùng điều khiển qua App
  isAutoMode = false;
  Blynk.virtualWrite(VPIN_AUTO_MODE, 0);

  updateWaterStatusLCD();
}

// Bật/Tắt chế độ AUTO từ App Blynk (Gán vào Virtual Pin V2 dạng Switch)
BLYNK_WRITE(VPIN_AUTO_MODE) {
  isAutoMode = param.asInt(); // 1 = Bật AUTO, 0 = Bật MANUAL
}

// =================================================================
// FREERTOS TASKS
// =================================================================

// TASK 1 (Chạy trên Core 0): Chuyên giao tiếp Wi-Fi & Blynk Cloud
void TaskBlynk(void *pvParameters) {
  for (;;) {
    Blynk.run();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// TASK 2 (Chạy trên Core 1): Đọc cảm biến, xử lý nút bấm & màn hình LCD
void TaskSensors(void *pvParameters) {
  unsigned long lastDHTTime = 0;
  unsigned long lastSoilTime = 0;

  for (;;) {
    unsigned long currentMillis = millis();

    // Quét nút bấm vật lý nhanh (mỗi 50ms)
    checkPhysicalButton();

    // Kiểm tra tắt đèn LCD (mỗi 1s)
    checkBacklightTimeout();

    // Đọc độ ẩm đất & tự động tưới mỗi 1000ms (1s)
    if (currentMillis - lastSoilTime >= 1000) {
      soilMoistureSensor();
      lastSoilTime = currentMillis;
    }

    // Đọc cảm biến DHT11 mỗi 2000ms (2s)
    if (currentMillis - lastDHTTime >= 2000) {
      DHT11sensor();
      lastDHTTime = currentMillis;
    }

    vTaskDelay(20 / portTICK_PERIOD_MS);
  }
}

// =================================================================
// SETUP & LOOP
// =================================================================

void setup() {
  Serial.begin(115200);

  Wire.begin(21, 22);
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lastBacklightTime = millis();

  pinMode(RELAY_PIN_1, OUTPUT);
  digitalWrite(RELAY_PIN_1, relay1State);
  
  pinMode(PUSH_BUTTON_1, INPUT_PULLUP);
  pinMode(PUSH_BUTTON_2, INPUT_PULLUP);

  Blynk.begin(auth, ssid, pass, "blynk.cloud", 80);
  dht.begin();
  
  updateWaterStatusLCD();

  // Tạo Task 1 (Blynk) - Gắn vào CORE 0
  xTaskCreatePinnedToCore(
    TaskBlynk,
    "TaskBlynk",
    4096,
    NULL,
    2,
    &TaskBlynkHandle,
    0
  );

  // Tạo Task 2 (Sensors & Buttons) - Gắn vào CORE 1
  xTaskCreatePinnedToCore(
    TaskSensors,
    "TaskSensors",
    4096,
    NULL,
    1,
    &TaskSensorsHandle,
    1
  );
}

void loop() {
  vTaskDelete(NULL); // Xóa task loop mặc định để giải phóng RAM
}