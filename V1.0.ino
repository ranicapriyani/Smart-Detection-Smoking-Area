#include <Arduino_FreeRTOS.h>
#include <semphr.h>

SemaphoreHandle_t mutex_v;
SemaphoreHandle_t gasSemaphore;
TaskHandle_t HandleTaskBuzzer;
TaskHandle_t HandleTaskGas;

int buzzer = 3;
int GASA0 = 2;      // Pin untuk sensor gas (gunakan pin interrupt eksternal)
int fanPin = 4;     // Pin untuk mengendalikan kipas
int lightSensor = A5;  // Pin untuk sensor cahaya (gunakan pin analog)
volatile bool gasDetected = false;

void TaskGas(void *pvParameters);
void TaskBuzzer(void *pvParameters);

void setup() {
  Serial.begin(9600);
  pinMode(buzzer, OUTPUT);
  pinMode(fanPin, OUTPUT);
  pinMode(GASA0, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(GASA0), gasInterrupt, CHANGE); // Menghubungkan interrupt ke fungsi gasInterrupt
  mutex_v = xSemaphoreCreateMutex();
  gasSemaphore = xSemaphoreCreateBinary();
  if (mutex_v == NULL || gasSemaphore == NULL ) {
    Serial.println("Mutex or Semaphore can not be created");
  }
  xTaskCreate(TaskGas, "GasSensor", 128, NULL, 1, &HandleTaskGas);
  xTaskCreate(TaskBuzzer, "BuzzerAlarm", 128, NULL, 3, &HandleTaskBuzzer);
}

void loop() {
  
void TaskGas(void *pvParameters) {
  while (1) {
    int gasValue = digitalRead(GASA0);

    if (gasValue == HIGH) {
      xSemaphoreTake(mutex_v, portMAX_DELAY);
      gasDetected = true;
      xSemaphoreGive(mutex_v);
      xSemaphoreGive(gasSemaphore);
      vTaskPrioritySet(NULL, 4); // Dynamic priority change
    } else {
      xSemaphoreTake(mutex_v, portMAX_DELAY);
      gasDetected = false;
      xSemaphoreGive(mutex_v);
      xSemaphoreTake(gasSemaphore, portMAX_DELAY);
      vTaskPrioritySet(NULL, 1); // Dynamic priority change
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}
  
  int lightValue = analogRead(lightSensor);
  
  if (lightValue < 800) {  // Ubah nilai ambang batas sesuai dengan kondisi cahaya di sekitar
    gasDetected = false;
    digitalWrite(fanPin, LOW); // Matikan kipas saat cahaya terang
    noTone(buzzer); // Matikan bunyi buzzer
    Serial.println("Cahaya terang. Kipas dimatikan.");
  } else {
    if (gasDetected) {
      digitalWrite(fanPin, HIGH); // Hidupkan kipas jika gas terdeteksi
      tone(buzzer, 2000, 500); // Bunyikan buzzer jika gas terdeteksi
      Serial.println("Gas terdeteksi! Buzzer dan kipas dihidupkan.");
    }
  }
}

void gasInterrupt() {
  if (digitalRead(GASA0) == HIGH) {
    gasDetected = true;
  } else {
    gasDetected = false;
  }
}
