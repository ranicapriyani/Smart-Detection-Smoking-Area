#include <Arduino_FreeRTOS.h>
#include <semphr.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

SemaphoreHandle_t mutex_v;
SemaphoreHandle_t gasSemaphore;
SemaphoreHandle_t lightSemaphore;
TaskHandle_t HandleTaskFan;
TaskHandle_t HandleTaskBuzzer;
TaskHandle_t HandleTaskGas;
TaskHandle_t HandleTaskLightSensor;

int buzzer = 3;
int GASA0 = A0;       // Pin untuk sensor gas (gunakan pin interrupt eksternal)
int fanPin = 4;      // Pin untuk mengendalikan kipas
int lightSensor = A5; // Pin untuk sensor cahaya (gunakan pin analog)
volatile bool gasDetected = false;
volatile bool lightDetected = false;

LiquidCrystal_I2C lcd(0x27, 16, 2); // Adjust the I2C address if needed
const int pushButtonPin = 6;          // Adjust the pin number as needed
volatile bool safeCondition = false;
volatile int gasLevel = 0;

const int gasThreshold = 500;  // Set the gas threshold level

QueueHandle_t gasLevelQueue;  // Queue to pass gas level between tasks

void TaskGas(void *pvParameters);
void TaskFan(void *pvParameters);
void TaskBuzzer(void *pvParameters);
void TaskLightSensor(void *pvParameters);
void gasInterrupt();
void buttonInterrupt();

void setup() {
  Serial.begin(9600);
  pinMode(buzzer, OUTPUT);
  pinMode(fanPin, OUTPUT);
  pinMode(GASA0, INPUT_PULLUP);
  pinMode(pushButtonPin, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(GASA0), gasInterrupt, CHANGE);
  attachInterrupt(digitalPinToInterrupt(pushButtonPin), buttonInterrupt, FALLING);

  mutex_v = xSemaphoreCreateMutex();
  gasSemaphore = xSemaphoreCreateBinary();
  lightSemaphore = xSemaphoreCreateBinary();
  gasLevelQueue = xQueueCreate(1, sizeof(int)); // Create a queue with a depth of 1

  if (mutex_v == NULL || gasSemaphore == NULL || gasLevelQueue == NULL) {
    Serial.println("Mutex or Semaphore or Queue can not be created");
  }

  xTaskCreate(TaskGas, "GasSensor", 128, NULL, 1, &HandleTaskGas);
  xTaskCreate(TaskFan, "FanControl", 128, NULL, 2, &HandleTaskFan);
  xTaskCreate(TaskBuzzer, "BuzzerAlarm", 128, NULL, 3, &HandleTaskBuzzer);
  xTaskCreate(TaskLightSensor, "LightSensor", 128, NULL, 1, &HandleTaskLightSensor);

  lcd.init();      // Initialize the LCD
  lcd.backlight(); // Turn on the backlight

  vTaskStartScheduler();
}

void loop() {}

void TaskGas(void *pvParameters) {
  while (1) {
    int gasValue = analogRead(GASA0);

    if (gasValue > 117) {
      xSemaphoreTake(mutex_v, portMAX_DELAY);
      gasDetected = true;
      xSemaphoreGive(mutex_v);
      xSemaphoreGive(gasSemaphore);

      // Send gas level to the queue
      xQueueSend(gasLevelQueue, &gasLevel, portMAX_DELAY);

      // Map gas level to priority
      UBaseType_t mappedPriority = map(gasLevel, 0, 1023, 1, configMAX_PRIORITIES - 1);

      // Set priority of HandleTaskFan
      vTaskPrioritySet(HandleTaskFan, mappedPriority);
    } else {
      xSemaphoreTake(mutex_v, portMAX_DELAY);
      gasDetected = false;
      xSemaphoreGive(mutex_v);
      xSemaphoreTake(gasSemaphore, portMAX_DELAY);

      // Send gas level to the queue
      xQueueSend(gasLevelQueue, &gasLevel, portMAX_DELAY);

      // Set lower priority if gas is not detected
      vTaskPrioritySet(HandleTaskFan, 1);
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void TaskFan(void *pvParameters) {
  while (1) {
    int receivedGasLevel;

    if (xQueueReceive(gasLevelQueue, &receivedGasLevel, portMAX_DELAY) == pdTRUE) {
      xSemaphoreTake(mutex_v, portMAX_DELAY);
      gasLevel = receivedGasLevel;

      // Fan control logic based on gas level
      if (gasDetected && gasLevel > gasThreshold) {
        digitalWrite(fanPin, HIGH); // Activate fan if gas is detected and level is above the threshold
      } else if (lightDetected && analogRead(lightSensor) > 800) {
        digitalWrite(fanPin, LOW); // Turn off fan if gas is not detected or level is below the threshold and light is above 800
      }
      xSemaphoreGive(mutex_v);
    }

    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void TaskBuzzer(void *pvParameters) {
  while (1) {
    xSemaphoreTake(mutex_v, portMAX_DELAY);
    if (gasDetected && gasLevel > gasThreshold) {
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

void gasInterrupt() {
safeCondition = !safeCondition; // Toggle safe condition
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Safe Condition:");
  lcd.setCursor(5, 1);
  lcd.print(safeCondition ? "Yes" : "No");
  lcd.setCursor(0, 1);
  lcd.print("Gas Level: ");
  lcd.print(gasLevel);
  xSemaphoreGive(mutex_v);
}

void buttonInterrupt() {
  xSemaphoreGive(gasSemaphore); // Corrected from xSemaphoreGive(soilMoistureSemaphore);
  Serial.println("Button Pressed");

  xSemaphoreTake(mutex_v, portMAX_DELAY);
  if (analogRead(GASA0) == HIGH) {
    gasDetected = true;
    gasLevel = analogRead(GASA0);
    lcd.print(gasLevel);
  } else {
    gasDetected = false;
    gasLevel = 0;
    lcd.print("Gas is Not Detected);
  }
}
