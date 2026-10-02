/*
  SMART HELMET – STM32 Bluepill (STM32F103CBT6) + Arduino IDE

  Modules:
    - MQ3 Alcohol Sensor (Analog)  -> A0 (PA0)
    - MPU6050 (I2C @ 3.3V)         -> PB6=SCL, PB7=SDA
    - GPS NEO-6M (9600 baud)       -> Serial2
    - SIM900A GSM (9600 baud)      -> Serial1

  Features:
    - Alcohol Detection
    - Accident Detection
    - GPS Tracking
    - GSM Emergency Alerts
*/

#include <Wire.h>
#include <TinyGPS++.h>
#include <MPU6050_tockn.h>

//////////////////// USER CONFIG ////////////////////

const char EMERGENCY_NUMBER[] = "+91XXXXXXXXXX";

const int MQ3_PIN = A0;
const int LED_PIN = PC13;

uint16_t MQ3_THRESHOLD = 2400;

float CRASH_G_THRESHOLD = 2.8;

uint32_t ALERT_COOLDOWN_MS = 20000;

/////////////////////////////////////////////////////

TinyGPSPlus gps;
MPU6050 mpu(Wire);

uint32_t lastAlcoholSMS = 0;
uint32_t lastCrashSMS = 0;

void ledOn()
{
  digitalWrite(LED_PIN, LOW);
}

void ledOff()
{
  digitalWrite(LED_PIN, HIGH);
}

void blink(int n, int onTime = 100, int offTime = 100)
{
  for (int i = 0; i < n; i++)
  {
    ledOn();
    delay(onTime);
    ledOff();
    delay(offTime);
  }
}

bool sendSMS(const String &body)
{
  Serial1.println("AT");
  delay(500);

  Serial1.println("ATE0");
  delay(500);

  Serial1.println("AT+CMGF=1");
  delay(500);

  Serial1.print("AT+CMGS=\"");
  Serial1.print(EMERGENCY_NUMBER);
  Serial1.println("\"");

  delay(600);

  Serial1.print(body);
  Serial1.write(26);

  delay(2500);

  return true;
}

String mapsLink(double lat, double lng)
{
  String link = "https://maps.google.com/?q=";
  link += String(lat, 6);
  link += ",";
  link += String(lng, 6);

  return link;
}

void setup()
{
  pinMode(LED_PIN, OUTPUT);
  ledOff();

  Serial.begin(115200);

  Wire.begin();

  mpu.begin();
  mpu.calcGyroOffsets(true);

  Serial1.begin(9600);
  Serial2.begin(9600);

  Serial.println("Smart Helmet System Started");

  blink(3, 150, 150);
}

void loop()
{
  uint16_t mqRaw = analogRead(MQ3_PIN);

  mpu.update();

  float ax = mpu.getAccX();
  float ay = mpu.getAccY();
  float az = mpu.getAccZ();

  float accelMagnitude =
      sqrt(ax * ax + ay * ay + az * az);

  while (Serial2.available())
  {
    gps.encode(Serial2.read());
  }

  static uint32_t lastPrint = 0;

  if (millis() - lastPrint > 1000)
  {
    lastPrint = millis();

    Serial.print("MQ3: ");
    Serial.print(mqRaw);

    Serial.print(" | Acceleration: ");
    Serial.print(accelMagnitude, 2);

    if (gps.location.isValid())
    {
      Serial.print(" | GPS: ");
      Serial.print(gps.location.lat(), 6);
      Serial.print(",");
      Serial.print(gps.location.lng(), 6);
    }
    else
    {
      Serial.print(" | GPS Searching...");
    }

    Serial.println();
  }

  // Alcohol Detection

  if (mqRaw > MQ3_THRESHOLD &&
      (millis() - lastAlcoholSMS) > ALERT_COOLDOWN_MS)
  {
    String message =
        "ALERT: Alcohol detected!\nValue = " +
        String(mqRaw);

    if (gps.location.isValid())
    {
      message += "\n";
      message += mapsLink(
          gps.location.lat(),
          gps.location.lng());
    }

    sendSMS(message);

    lastAlcoholSMS = millis();

    blink(2, 150, 150);
  }

  // Accident Detection

  if (accelMagnitude > CRASH_G_THRESHOLD &&
      (millis() - lastCrashSMS) > ALERT_COOLDOWN_MS)
  {
    uint32_t startTime = millis();

    while (!gps.location.isValid() &&
           (millis() - startTime) < 4000)
    {
      while (Serial2.available())
      {
        gps.encode(Serial2.read());
      }
    }

    String message = "EMERGENCY: Crash detected!";

    if (gps.location.isValid())
    {
      double lat = gps.location.lat();
      double lng = gps.location.lng();

      message += "\nLatitude: " + String(lat, 6);
      message += "\nLongitude: " + String(lng, 6);
      message += "\n" + mapsLink(lat, lng);
    }
    else
    {
      message += "\nLocation unavailable";
    }

    sendSMS(message);

    lastCrashSMS = millis();

    blink(3, 120, 120);
  }

  delay(50);
}
