#define BLYNK_TEMPLATE_ID "TMPL6jDuSVjPI"
#define BLYNK_TEMPLATE_NAME "PBL5"
#define BLYNK_AUTH_TOKEN "y1uPz6lIRsVM8OVFa0u6EC3UzsNnCQL6"
#define RED_APPLE_PIN V1
#define GREEN_APPLE_PIN V2

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>
#include <Stepper.h>
#include <BlynkSimpleEsp32.h>

// const char* ssid = "Tro 123";
// const char* password = "tudoanpasss*#";

// const char* ssid = "Zone Six Phu";
// const char* ssid = "Zone Six-19PNX-5G";
// const char* password = "19phamnhuxuong";

const char* ssid = "Realmi";
const char* password = "20102002";

const char* serverName = "https://47ac-34-34-102-177.ngrok-free.app/predict";

// Servo servo1;
// Servo servo2;

// const int ledPin = 2;
// const int sv1Pin = 17;
// const int sv1Pin = 13;
// const int sv2Pin = 14;
const int cb1Pin = 34;   //điều khiển servo      
// const int cb2Pin = 35; 
const int cb3Pin = 13;   // điều khiển lấy data

const int steps_per_rev = 200; //Set to 200 for NIMA 17
const int IN1_motor1 = 12;
const int IN2_motor1 = 14;
const int IN3_motor1 = 27;
const int IN4_motor1 = 26;

// const int IN1_motor2 = 25;
// const int IN2_motor2 = 23;
// const int IN3_motor2 = 32;
// const int IN4_motor2 = 35;

const int IN1_motor2 = 15;
const int IN2_motor2 = 2;
const int IN3_motor2 = 0;
const int IN4_motor2 = 4;

Stepper motor1(steps_per_rev, IN1_motor1, IN2_motor1, IN3_motor1, IN4_motor1);
Stepper motor2(steps_per_rev, IN1_motor2, IN2_motor2, IN3_motor2, IN4_motor2);


int redAppleCount = 0;
int greenAppleCount = 0;


long debouncing_time = 2000;
volatile unsigned long last_micros1;
volatile unsigned long last_micros3;


SemaphoreHandle_t cb1Semaphore; // Semaphore cho cảm biến 1
// SemaphoreHandle_t cb2Semaphore; // Semaphore cho cảm biến 2
SemaphoreHandle_t cb3Semaphore; // Semaphore cho cảm biến 3

QueueHandle_t serverQueue;

void IRAM_ATTR cb1ISR() {
  xSemaphoreGiveFromISR(cb1Semaphore, NULL); // Cấp semaphore khi có ngắt từ cb1
  Serial.println("ISR1");
}

// void IRAM_ATTR cb2ISR() {
//   xSemaphoreGiveFromISR(cb2Semaphore, NULL); // Cấp semaphore khi có ngắt từ cb2
// }

void IRAM_ATTR cb3ISR() {
  xSemaphoreGiveFromISR(cb3Semaphore, NULL); // Cấp semaphore khi có ngắt từ cb2
  Serial.println("ISR3");
}

void debounceInterrupt1(){
  if((long)(micros()-last_micros1)>=debouncing_time*1000){
    cb1ISR();
    last_micros1=micros();
  }
}

void debounceInterrupt3(){
  if((long)(micros()-last_micros3)>=debouncing_time*1000){
    cb3ISR();
    last_micros3=micros();
  }
}

void taskControlStepper(void *parameter);
void taskPrint(void *parameter);
String getDetailFromJson(const String& jsonString);


void setup() {
  Serial.begin(115200);
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, password);
  //servo
  // servo1.attach(sv1Pin, 500, 2400);
  // servo2.attach(sv2Pin, 500, 2400);
  //stepper
  motor1.setSpeed(60);
  motor2.setSpeed(60);

  pinMode(cb1Pin, INPUT_PULLUP);
  // pinMode(cb2Pin, INPUT_PULLUP);
  pinMode(cb3Pin, INPUT_PULLUP);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi");

  cb1Semaphore = xSemaphoreCreateBinary();
  // cb2Semaphore = xSemaphoreCreateBinary();
  cb3Semaphore = xSemaphoreCreateBinary();
  serverQueue = xQueueCreate(5, sizeof(String));

  // Thiết lập ngắt cho cảm biến hồng ngoại
  attachInterrupt(digitalPinToInterrupt(cb1Pin), debounceInterrupt1, FALLING);
  // attachInterrupt(digitalPinToInterrupt(cb2Pin), cb2ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(cb3Pin), debounceInterrupt3, FALLING);

  xTaskCreatePinnedToCore(taskFetchData, "Fetch Server Data", 4096, NULL, 3, NULL, 1);
  xTaskCreatePinnedToCore(taskControlStepper, "Control Stepper", 2048, NULL, 4, NULL, 1);
  // xTaskCreatePinnedToCore(controlServo2, "Control Servo2", 2048, NULL, 3, NULL, 1);
  xTaskCreatePinnedToCore(taskPrint, "Print Value", 2048, NULL, 1, NULL, 1);  
  xTaskCreatePinnedToCore(taskSentToBlynk, "sent to blynk", 4096, NULL, 2, NULL, 1); 
  // xTaskCreatePinnedToCore(taskBlynkRun, "Blynk Run", 2048, NULL, 2, NULL, 0);
 
}

void loop() {
  // Không cần sử dụng trong FreeRTOS
  Blynk.run();
}

void taskFetchData(void *parameter) {
  Serial.println("task fetchDataFromServer is running");
  TickType_t xLastWakeTime = xTaskGetTickCount();
  while (true) {
    if (xSemaphoreTake(cb3Semaphore, portMAX_DELAY) == pdTRUE) {  // Wait for cb3Semaphore
      Serial.println("task fetchDataFromServer is runing");
      HTTPClient http;
      http.begin(serverName);
      int httpResponseCode = http.GET();
      if (httpResponseCode > 0) {
        String payload = http.getString();
        Serial.println("Payload:"+payload);
        String data = getDetailFromJson(payload);
        Serial.println("data tra vê từ server:" + data);
        xQueueSend(serverQueue, &data, portMAX_DELAY);
        Serial.println("đã gửi vào hàng đợi");
      } else {
        Serial.println("Failed to fetch data from server");
      }
      http.end();
      // vTaskDelayUntil(&xLastWakeTime, 5000 / portTICK_PERIOD_MS); 
    }
  }
}


String getDetailFromJson(const String& jsonString) {
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, jsonString);
  if (error) {
    Serial.print("JSON parsing failed: ");
    Serial.println(error.f_str());
    return "";
  }
  return doc["message"].as<String>();
  // return doc["detail"].as<String>();
}

//task controlServo1
void taskControlStepper(void *parameter) {
  String serverData;
  Serial.println("task controlServo1 is runing");
  while (true) {
    if (xSemaphoreTake(cb1Semaphore, portMAX_DELAY) == pdTRUE) { // Chờ đến khi cb1Semaphore được cấp
      if (xQueueReceive(serverQueue, &serverData, portMAX_DELAY) == pdTRUE) {
        if (serverData == "RedApple") { //servo1
            // Serial.println(serverData);  
            Serial.println("Bật stepper 1");  // In ra câu "bật servo"
            // servo1.write(120);      // Quay servo2 đến góc 120 độ
            motor1.step(steps_per_rev);
            
            delay(2000);
            // vTaskDelay(5000 / portTICK_PERIOD_MS);
            
            Serial.println("Đóng stepper 1");  // In ra câu "đóng servo"
            motor1.step(-steps_per_rev);

            // servo1.write(0);       // Trả về góc 0 độ

            redAppleCount++;  // Tăng số lượng táo đỏ
            Serial.print("Số lượng táo đỏ: ");
            Serial.println(redAppleCount);
        } else if (serverData == "GreenApple"){
            // Serial.println(serverData);  
            Serial.println("Bật stepper 2");  // In ra câu "bật servo"
            // servo2.write(60);      // Quay servo2 đến góc 120 độ
            motor2.step(steps_per_rev);

            // vTaskDelay(5000 / portTICK_PERIOD_MS);
            delay(2000);

            Serial.println("Đóng stepper 2");  // In ra câu "đóng servo"
            motor2.step(-steps_per_rev);
            // servo2.write(0); 

            greenAppleCount++;  // Tăng số lượng táo xanh
            Serial.print("Số lượng táo xanh: ");
            Serial.println(greenAppleCount);
        } else{
            Serial.println("Chưa dự đoán được");  
        }
      }
    }
  }
}

//task controlServo2
// void controlServo2(void *parameter) {
//   String serverData;

//   while (true) {
//     if (xSemaphoreTake(cb2Semaphore, portMAX_DELAY) == pdTRUE) { // Chờ đến khi cb2Semaphore được cấp
//       if (xQueueReceive(serverQueue, &serverData, portMAX_DELAY) == pdTRUE) {
//         if (serverData == "GreenApple") { // servo 2
//           Serial.println(serverData);  
//           Serial.println("Bật servo 2");  
//           servo2.write(120);      // Quay servo2 đến góc 90 độ
//           vTaskDelay(5000 / portTICK_PERIOD_MS);
//           Serial.println("Bật servo 2");  
//           servo2.write(0);       // Trả về góc 0 độ
//         } else{
//           Serial.println("No image found");  
          
//         }
//       }
//     }
//   }
// }

// Hàm đọc giá trị từ cảm biến và hiển thị ra Serial Monitor
//task readAndPrintValues
void taskPrint(void *parameter) {
  while(true){
    Serial.println("task readAndPrintValues is runing");

    // In ra số lượng táo xanh và táo đỏ
    Serial.print("Số lượng táo đỏ: ");
    Serial.println(redAppleCount);

    Serial.print("Số lượng táo xanh: ");
    Serial.println(greenAppleCount);

    Serial.println("----------------------");  // Dòng ngăn cách giữa các lần đọc
    vTaskDelay(3000 / portTICK_PERIOD_MS);
  }
}

// void taskBlynkRun(void *parameter) {
//   while (true) {
//     Blynk.run();
//     vTaskDelay(10 / portTICK_PERIOD_MS);
//   }
// }


void taskSentToBlynk(void *parameter) {
  while (true) {
    if (Blynk.connected()) {
      Serial.println("Gửi dữ liệu");
      Blynk.virtualWrite(RED_APPLE_PIN, redAppleCount);
      Blynk.virtualWrite(GREEN_APPLE_PIN, greenAppleCount);
      Serial.println("Đã gửi lên Blynk");
    } else {
      Serial.println("Blynk không kết nối");
    }
    vTaskDelay(3000 / portTICK_PERIOD_MS);  // Đợi 3 giây
  }
}

