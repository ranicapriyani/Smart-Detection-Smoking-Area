#include <Arduino_FreeRTOS.h>
#include <semphr.h>

SemaphoreHandle_t mutex_v;
SemaphoreHandle_t gasSemaphore;
SemaphoreHandle_t lightSemaphore;
TaskHandle_t HandleTaskFan;
TaskHandle_t HandleTaskBuzzer;
TaskHandle_t HandleTaskGas;
TaskHandle_t HandleTaskLightSensor;

int buzzer = 3;
int GASA0 = 2;      // Pin untuk sensor gas (gunakan pin interrupt eksternal)
int fanPin = 4;     // Pin untuk mengendalikan kipas
int lightSensor = A5;  // Pin untuk sensor cahaya (gunakan pin analog)
volatile bool gasDetected = false;
volatile bool lightDetected = false;

void TaskGas(void *pvParameters);
void TaskFan(void *pvParameters);
void TaskBuzzer(void *pvParameters);
void TaskLightSensor(void *pvParameters);

void setup() {
  Serial.begin(9600);
  pinMode(buzzer, OUTPUT);
  pinMode(fanPin, OUTPUT);
  pinMode(GASA0, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(GASA0), gasInterrupt, CHANGE); // Menghubungkan interrupt ke fungsi gasInterrupt
  mutex_v = xSemaphoreCreateMutex();
  gasSemaphore = xSemaphoreCreateBinary();
  lightSemaphore = xSemaphoreCreateBinary();
  
  if (mutex_v == NULL || gasSemaphore == NULL ) {
    Serial.println("Mutex or Semaphore can not be created");
  }
  xTaskCreate(TaskGas, "GasSensor", 128, NULL, 1, &HandleTaskGas);
  xTaskCreate(TaskFan, "FanControl", 128, NULL, 2, &HandleTaskFan);
  xTaskCreate(TaskBuzzer, "BuzzerAlarm", 128, NULL, 3, &HandleTaskBuzzer);
  xTaskCreate(TaskLightSensor, "LightSensor", 128, NULL, 1, &HandleTaskLightSensor);
 
  vTaskStartScheduler();
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

  void TaskFan(void *pvParameters) {
  while (1) {
    xSemaphoreTake(mutex_v, portMAX_DELAY);
    if (gasDetected) {
      digitalWrite(fanPin, HIGH); // Hidupkan kipas jika gas terdeteksi
    } else if (lightDetected && analogRead(lightSensor) > 800) {
      digitalWrite(fanPin, LOW); // Matikan kipas jika gas tidak terdeteksi dan cahaya lebih dari 800
    }
    xSemaphoreGive(mutex_v);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void TaskBuzzer(void *pvParameters) {
  while (1) {
    xSemaphoreTake(mutex_v, portMAX_DELAY);
    if (gasDetected) {
      tone(buzzer, 2000, 500);
    } else {
      noTone(buzzer);
    }
    xSemaphoreGive(mutex_v);
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}
  
 void TaskLightSensor(void *pvParameters) {
  while (1) {
    xSemaphoreTake(mutex_v, portMAX_DELAY);
    int lightValue = analogRead(lightSensor);
    if (lightValue > 800) {
      lightDetected = true;
      xSemaphoreGive(lightSemaphore);
    } else {
      lightDetected = false;
      xSemaphoreTake(lightSemaphore, portMAX_DELAY);
    }
    xSemaphoreGive(mutex_v);
    vTaskDelay(100 / portTICK_PERIOD_MS);
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
