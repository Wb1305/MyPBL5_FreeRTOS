#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>

// const char* ssid = "Tro 123";
// const char* password = "tudoanpasss*#";

const char* ssid = "Zone Six Phu";
const char* password = "19phamnhuxuong";
const char* serverName = "https://af49-35-245-162-201.ngrok-free.app/predict";

Servo servo1;
Servo servo2;

const int ledPin = 2;
// const int sv1Pin = 17;
const int sv1Pin = 13;
const int sv2Pin = 14;
const int cb1Pin = 34;        
// const int cb2Pin = 35;   
const int cb3Pin = 32;        

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

void controlServo1(void *parameter);
void readAndPrintValues(void *parameter);
String getDetailFromJson(const String& jsonString);


void setup() {
  Serial.begin(115200);
  // pinMode(ledPin, OUTPUT);
  // digitalWrite(ledPin, LOW);

  servo1.attach(sv1Pin, 500, 2400);
  servo2.attach(sv2Pin, 500, 2400);

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

  xTaskCreatePinnedToCore(fetchDataFromServer, "Fetch Server Data", 4096, NULL, 2, NULL, 1);
  // xTaskCreatePinnedToCore(controlSemaphoreAndFetchData, "Control Semaphore And Fetch Data", 4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(controlServo1, "Control Servo1", 2048, NULL, 3, NULL, 1);
  // xTaskCreatePinnedToCore(controlServo2, "Control Servo2", 2048, NULL, 3, NULL, 1);
  // xTaskCreatePinnedToCore(readAndPrintValues, "Read Sensor", 2048, NULL, 1, NULL, 1);  

}

void loop() {
  // Không cần sử dụng trong FreeRTOS
}

// void fetchDataFromServer(void *parameter) {
//   TickType_t xLastWakeTime = xTaskGetTickCount();  // Lưu thời điểm bắt đầu
//   while (true) {
//     Serial.println("task fetchDataFromServer is runing");
//     HTTPClient http;
//     http.begin(serverName);
//     int httpResponseCode = http.GET();
//     if (httpResponseCode > 0) {
//       String payload = http.getString();
//       Serial.println("Payload:"+payload);
//       String data = getDetailFromJson(payload);
//       Serial.println("data tra vê từ server:" + data);
//       xQueueSend(serverQueue, &data, portMAX_DELAY);
//       Serial.println("đã gửi vào hàng đợi");
//     } else {
//       Serial.println("Failed to fetch data from server");
//     }
//     http.end();
//     vTaskDelayUntil(&xLastWakeTime, 5000 / portTICK_PERIOD_MS); 
//   }
// }

void fetchDataFromServer(void *parameter) {
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

// void fetchDataFromServer() {
//   HTTPClient http;
//   http.begin(serverName);
//   int httpResponseCode = http.GET();
  
//   if (httpResponseCode > 0) {
//     String payload = http.getString();
//     Serial.println("Payload: " + payload);
//     String data = getDetailFromJson(payload);
//     Serial.println("Data từ server: " + data);
//     xQueueSend(serverQueue, &data, portMAX_DELAY);
//     Serial.println("Đã gửi vào hàng đợi");
//   } else {
//     Serial.println("Lỗi khi lấy dữ liệu từ server");
//   }
//   http.end();
// }

// // task controlSemaphoreAndFetchData
// void controlSemaphoreAndFetchData(void *parameter) {
//   TickType_t xLastWakeTime = xTaskGetTickCount();
  
//   while (true) {
//     Serial.println("Task controlSemaphoreAndFetchData is running");
//     if (xSemaphoreTake(cb3Semaphore, portMAX_DELAY) == pdTRUE) {  // Chờ cb3Semaphore
//       fetchDataFromServer();  // Gọi hàm fetchDataFromServer khi semaphore được cấp
//     }
//     vTaskDelayUntil(&xLastWakeTime, 5000 / portTICK_PERIOD_MS); 
//   }
// }


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
void controlServo1(void *parameter) {
  String serverData;
  Serial.println("task controlServo1 is runing");
  while (true) {
    if (xSemaphoreTake(cb1Semaphore, portMAX_DELAY) == pdTRUE) { // Chờ đến khi cb1Semaphore được cấp
      if (xQueueReceive(serverQueue, &serverData, portMAX_DELAY) == pdTRUE) {
        // if (serverData == "onservo1") {
        //   servo1.write(90);      // Quay servo1 đến góc 90 độ
        //   vTaskDelay(2000 / portTICK_PERIOD_MS);
        //   servo1.write(0);       // Trả về góc 0 độ
        // }
        if (serverData == "RedApple") { //servo1
            // Serial.println(serverData);  
            Serial.println("Bật servo 1");  // In ra câu "bật servo"
            // servo1.write(120);      // Quay servo2 đến góc 120 độ
            
            // delay(2000);
            // for(int i = 0; i< 2000;i++){}
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            
            Serial.println("Đóng servo 1");  // In ra câu "đóng servo"
            // servo1.write(240);       // Trả về góc 0 độ
        } else if (serverData == "GreenApple"){
            // Serial.println(serverData);  
            Serial.println("Bật servo 2");  // In ra câu "bật servo"
            // servo2.write(60);      // Quay servo2 đến góc 120 độ
            // delay(2000);
            // for(int i = 0; i< 2000;i++){}
            vTaskDelay(5000 / portTICK_PERIOD_MS);

            Serial.println("Đóng servo 2");  // In ra câu "đóng servo"
            // servo2.write(0); 
        } else{
            Serial.println("Chưa dự đoán được");  
            // servo1.write(90);      // Quay servo2 đến góc 120 độ
            // Serial.println("Bật servo 1");  // In ra câu "bật servo"
            // // Chờ 5 giây trước khi trả về vị trí ban đầu
            // delay(5000); 

            // Serial.println("Đóng servo 1");  // In ra câu "đóng servo"
            // servo1.write(0);  
        }
        // vTaskDelay(5000 / portTICK_PERIOD_MS);
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
void readAndPrintValues(void *parameter) {
  while(true){
    Serial.println("task readAndPrintValues is runing");
    // Đọc giá trị từ các cảm biến
    int sensorValue1 = digitalRead(cb1Pin);
    // int sensorValue2 = digitalRead(cb2Pin);
    int sensorValue3 = digitalRead(cb3Pin);

    // Hiển thị trạng thái cảm biến hồng ngoại 1
    Serial.print("Cảm biến 1: ");
    if (sensorValue1 == 0) {
      Serial.println("Phát hiện vật thể");
      Serial.println("Giá trị:" + String(sensorValue1));
    } else {
      Serial.println("Không phát hiện vật thể");
      Serial.println("Giá trị:" + String(sensorValue1));
    }
    
    // Hiển thị trạng thái cảm biến hồng ngoại 2
    // Serial.print("Cảm biến 2: ");
    // if (sensorValue2 == 0) {
    //   Serial.println("Phát hiện vật thể");
    //   Serial.println("Giá trị:" + String(sensorValue2));
    // } else {
    //   Serial.println("Không phát hiện vật thể");
    //   Serial.println("Giá trị:" + String(sensorValue2));
    // }

    Serial.print("Cảm biến 3: ");
    if (sensorValue3 == 0) {
      Serial.println("Phát hiện vật thể");
      Serial.println("Giá trị:" + String(sensorValue3));
    } else {
      Serial.println("Không phát hiện vật thể");
      Serial.println("Giá trị:" + String(sensorValue3));
    }

    // fetchDataFromServer();
    Serial.println("----------------------");  // Dòng ngăn cách giữa các lần đọc
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
}

