int buzzer = 3;
int GASA0 = 2;      // Pin untuk sensor gas (gunakan pin interrupt eksternal)
int fanPin = 4;     // Pin untuk mengendalikan kipas
int lightSensor = A5;  // Pin untuk sensor cahaya (gunakan pin analog)

volatile bool gasDetected = false;

void setup() {
  Serial.begin(9600);
  pinMode(buzzer, OUTPUT);
  pinMode(fanPin, OUTPUT);
  pinMode(GASA0, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(GASA0), gasInterrupt, CHANGE); // Menghubungkan interrupt ke fungsi gasInterrupt
}

void loop() {
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