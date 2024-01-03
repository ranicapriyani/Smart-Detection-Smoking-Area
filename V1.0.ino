#include <Arduino_FreeRTOS.h>
#include <semphr.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

SemaphoreHandle_t mutex_v;
SemaphoreHandle_t lcdSemaphore;
SemaphoreHandle_t fanSemaphore;
SemaphoreHandle_t buttonSemaphore;
QueueHandle_t gasQueue;

TaskHandle_t gasTaskHandle;
TaskHandle_t fanTaskHandle;
TaskHandle_t buzzerTaskHandle;

const int GasPin = A0;
const int buttonPin = 2;
const int Relaypin = 3;
const int buzzer = 4;

LiquidCrystal_I2C lcd(0x27, 16, 2);

bool lcdState = true;    // Variable to track LCD state
bool gasStatus = false;  // Variable to track gas status

void gasSensorTask(void *pvParameters);
void fanTask(void *pvParameters);
void buzzerTask(void *pvParameters);
void buttonInterrupt();

void setup() {
  Serial.begin(9600);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(Relaypin, OUTPUT);

  mutex_v = xSemaphoreCreateMutex();
  lcdSemaphore = xSemaphoreCreateBinary();
  fanSemaphore = xSemaphoreCreateCounting(1, 0);
  buttonSemaphore = xSemaphoreCreateBinary();
  gasQueue = xQueueCreate(1, sizeof(bool));  // Membuat antrian dengan kapasitas 1 dan ukuran elemen bool
  
  if (mutex_v == NULL || lcdSemaphore == NULL || fanSemaphore == NULL || buttonSemaphore == NULL || gasQueue == NULL) {
    Serial.println("Semaphore/Queue creation failed.");
  }

  attachInterrupt(digitalPinToInterrupt(buttonPin), buttonInterrupt, FALLING);

  xTaskCreate(gasSensorTask, "GasTask", 128, NULL, 2, &gasTaskHandle);
  xTaskCreate(fanTask, "FanTask", 128, NULL, 1, &fanTaskHandle);
  xTaskCreate(buzzerTask, "BuzzerTask", 128, NULL, 1, &buzzerTaskHandle);
  lcd.init();      // Initialize the LCD
  lcd.backlight(); // Turn on the backlight
}

void loop() {}

void gasSensorTask(void *pvParameters) {
  (void)pvParameters;
  while (1) {
    int gasLevel = analogRead(GasPin);
    Serial.println(gasLevel);

    if (xSemaphoreTake(lcdSemaphore, portMAX_DELAY) == pdTRUE) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Gas Level:");

      if (lcdState) {
        // Display gas level
        lcd.setCursor(5, 1);
        lcd.print(gasLevel);
      } else {
        // Display gas status
        lcd.setCursor(5, 1);
        lcd.print(gasStatus ? "Danger" : "Safe");
      }

      xSemaphoreGive(lcdSemaphore); // Give semaphore for LCD update
    }

    // Check if gas level exceeds the threshold
    if (gasLevel > 80) {
      // Give semaphore signal to turn on the fan
      xSemaphoreGive(fanSemaphore);
      changeTaskPriority(fanTaskHandle, 2);
      changeTaskPriority(buzzerTaskHandle, 2);

      // Update gas status
      gasStatus = true;

      // Kirim status gas ke antrian gasQueue
      xQueueSend(gasQueue, &gasStatus, portMAX_DELAY);
    } else {
      gasStatus = false;
    }

    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

void changeTaskPriority(TaskHandle_t taskHandle, UBaseType_t newPriority) {
  vTaskPrioritySet(taskHandle, newPriority);
}

void buzzerTask(void *pvParameters) {
  (void)pvParameters;
  bool receivedStatus;
  while (1) {
    // Tunggu sinyal antrian
    if (xQueueReceive(gasQueue, &receivedStatus, portMAX_DELAY) == pdTRUE) {
      // Fan is turned on
      if (receivedStatus) {
        tone(buzzer, 2000, 500);
        delay(1000); // Wait for 10 seconds
        noTone(buzzer);
      }
      // Turn off the fan
    }
  }
}

void fanTask(void *pvParameters) {
  (void)pvParameters;
  while (1) {
    // Tunggu sinyal semaphore
    if (xSemaphoreTake(fanSemaphore, portMAX_DELAY) == pdTRUE) {
      // Kipas dinyalakan
      digitalWrite(Relaypin, HIGH);
      delay(1000); // Tunggu selama 10 detik

      // Matikan kipas
      digitalWrite(Relaypin, LOW);
    }
  }
}

void buttonInterrupt() {
  xSemaphoreGive(buttonSemaphore);
  Serial.println("Button Pressed");

  // Toggle between displaying gas level and gas status
  lcdState = !lcdState;

  // Update LCD semaphore accordingly
  xSemaphoreGive(lcdSemaphore);
}
