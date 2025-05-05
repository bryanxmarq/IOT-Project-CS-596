#include <Wire.h>
#include <Arduino.h>
#include <Adafruit_CAP1188.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <HTTPClient.h>

// pin Definitions
#define RED_LED_PIN 26
#define GREEN_LED_PIN 27
#define SERVO_PIN 13

// create the CAP1188 touch sensor object
Adafruit_CAP1188 cap = Adafruit_CAP1188();
Servo myServo;

// WiFi credentials
const char* ssid = "NETGEAR86";
const char* password = "**********";

// flask server URL
const char* serverURL = "http://18.191.11.199:5000";

String correctCode = "1234";      // define correct code for unlocking
String enteredCode = "";          // code entered by user
unsigned long lockedOutUntil = 0; // time until lockout ends (in ms)

// function to send status and code to server
String sendToServer(String status, String code) {
  String responseText = "";
  
  // check if wifi is connected
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String(serverURL) + "/?status=" + status + "&code=" + code;

    Serial.print("Sending request to: ");
    Serial.println(url);

    http.begin(url);
    int httpResponseCode = http.GET();

    // if response received successfully
    if (httpResponseCode > 0) {
      responseText = http.getString();
      Serial.print("Server response: ");
      Serial.println(responseText);
    } else {
      Serial.print("Failed to connect. Error code: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  } else {
    Serial.println("WiFi not connected");
  }

  return responseText;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  // connect to wifi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi!");

  // initialize CAP1188 
  if (!cap.begin()) {
    Serial.println("CAP1188 not detected, check connections!");
    while (1);
  }

  // set led with red led on at initialization
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);

  //servo set to lock position
  myServo.attach(SERVO_PIN);
  myServo.write(90); // Locked position

  digitalWrite(RED_LED_PIN, HIGH);
  digitalWrite(GREEN_LED_PIN, LOW);

  Serial.println("Digital Lock Initialized!");
}

void loop() {
  // if locked out, skip input
  if (millis() < lockedOutUntil) {
    Serial.println("Device is locked out. Please wait...");
    delay(1000);
    return;
  }

  // read which sensor lights were touched
  uint8_t touched = cap.touched();

  for (int i = 0; i < 8; i++) {
    if (touched & (1 << i)) {
      Serial.print("Sensor ");
      Serial.print(i + 1);
      Serial.println(" touched");

      enteredCode += String(i + 1);
      delay(500);
    }
  }

  // process code when 4 digits are entered
  if (enteredCode.length() >= 4) {
    if (enteredCode == correctCode) {
      Serial.println("Correct code! Unlocking...");
      sendToServer("unlocked", enteredCode);
      
      // unlock servo and led green
      myServo.write(180);  // Unlock position
      digitalWrite(GREEN_LED_PIN, HIGH);
      digitalWrite(RED_LED_PIN, LOW);
      delay(5000);

      // re-lock
      myServo.write(90);  // Lock position
      digitalWrite(GREEN_LED_PIN, LOW);
      digitalWrite(RED_LED_PIN, HIGH);
    } else {
      // incorrect code 
      Serial.println("Incorrect code!");
      String response = sendToServer("attempt", enteredCode);

      // check for lock out in server response
      if (response.indexOf("LOCKED OUT for") != -1) {
        int secondsIndex = response.indexOf("seconds");
        int forIndex = response.indexOf("for");

        if (secondsIndex != -1 && forIndex != -1) {
          String secondsStr = response.substring(forIndex + 4, secondsIndex - 1);
          int lockoutSeconds = secondsStr.toInt();
          Serial.print("Parsed lockout time: ");
          Serial.println(lockoutSeconds);
          // set lockout timer
          lockedOutUntil = millis() + (lockoutSeconds * 1000);
        }
      }

      // blink red LED on wrong code
      for (int i = 0; i < 3; i++) {
        digitalWrite(RED_LED_PIN, HIGH);
        delay(200);
        digitalWrite(RED_LED_PIN, LOW);
        delay(200);
      }
    }

    enteredCode = ""; // reset code
  }

  delay(100);
}
