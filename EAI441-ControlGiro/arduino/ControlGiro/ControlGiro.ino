
#include <WiFiS3.h>
#include <WiFiSSLClient.h>
#include <ArduinoJson.h>
#include "arduino_secrets.h"

// =====================================================
// CONFIGURACION
// =====================================================

const char API_HOST[] = "eai441-control-giro-api.onrender.com";
const char DEVICE_API_KEY[] = SECRET_DEVICE_API_KEY;
const int API_PORT = 443;

// HC-SR04
const int TRIG_PIN = A1;
const int ECHO_PIN = A2;

// Pines internos del shield L293D compatible con V1
const int MOTOR_PWM_PIN = 11;   // Velocidad de M1
const int MOTOR_DIR_CLK = 4;    // Reloj del registro
const int MOTOR_DIR_EN = 7;     // Habilitacion del registro
const int MOTOR_DIR_SER = 8;    // Datos del registro
const int MOTOR_DIR_LATCH = 12; // Latch del registro

// Bits de direccion del motor M1
const uint8_t MOTOR1_A = 2;
const uint8_t MOTOR1_B = 3;

const int MOTOR_SPEED = 180; // Rango de 0 a 255

const unsigned long COMMAND_INTERVAL_MS = 1500;
const unsigned long TELEMETRY_INTERVAL_MS = 3000;

String currentMode = "AUTOMATICO";
String currentMotorState = "DETENIDO";

int lastRevision = -1;

unsigned long lastCommandRequest = 0;
unsigned long lastTelemetrySent = 0;

// Estado de direccion de los cuatro motores en el registro
uint8_t motorDirectionBits = 0;

// =====================================================
// CONTROL DEL REGISTRO DEL SHIELD
// =====================================================

void writeMotorRegister() {
  digitalWrite(MOTOR_DIR_LATCH, LOW);

  shiftOut(
    MOTOR_DIR_SER,
    MOTOR_DIR_CLK,
    MSBFIRST,
    motorDirectionBits
  );

  digitalWrite(MOTOR_DIR_LATCH, HIGH);
}

void setupMotorShield() {
  pinMode(MOTOR_PWM_PIN, OUTPUT);
  pinMode(MOTOR_DIR_CLK, OUTPUT);
  pinMode(MOTOR_DIR_EN, OUTPUT);
  pinMode(MOTOR_DIR_SER, OUTPUT);
  pinMode(MOTOR_DIR_LATCH, OUTPUT);

  // Habilitar el registro del shield
  digitalWrite(MOTOR_DIR_EN, LOW);

  digitalWrite(MOTOR_DIR_LATCH, LOW);
  digitalWrite(MOTOR_DIR_CLK, LOW);
  digitalWrite(MOTOR_DIR_SER, LOW);

  analogWrite(MOTOR_PWM_PIN, 0);

  motorDirectionBits = 0;
  writeMotorRegister();
}

// =====================================================
// MOTOR M1: DERECHA, IZQUIERDA Y DETENER
// =====================================================

void applyMotorState(const String& newState) {
  if (newState != "DERECHA" &&
      newState != "IZQUIERDA" &&
      newState != "DETENIDO") {
    return;
  }

  if (newState == currentMotorState) {
    return;
  }

  // Quitar primero la potencia del motor
  analogWrite(MOTOR_PWM_PIN, 0);

  // Limpiar los dos bits correspondientes a M1
  motorDirectionBits &= ~(
    (1 << MOTOR1_A) |
    (1 << MOTOR1_B)
  );

  if (newState == "DERECHA") {
    motorDirectionBits |= (1 << MOTOR1_A);
  }
  else if (newState == "IZQUIERDA") {
    motorDirectionBits |= (1 << MOTOR1_B);
  }

  // Enviar la direccion al shield
  writeMotorRegister();

  // DETENIDO: direccion neutra y PWM cero
  if (newState == "DETENIDO") {
    analogWrite(MOTOR_PWM_PIN, 0);
  } else {
    analogWrite(MOTOR_PWM_PIN, MOTOR_SPEED);
  }

  currentMotorState = newState;

  Serial.print("Motor M1: ");
  Serial.println(currentMotorState);
}

// =====================================================
// HTTPS CON RENDER
// =====================================================

String apiRequest(
  const char* method,
  const char* path,
  const String& body = ""
) {
  WiFiSSLClient client;

  Serial.print("HTTPS ");
  Serial.println(path);

  if (!client.connect(API_HOST, API_PORT)) {
    Serial.println("Error de conexion con Render.");
    client.stop();
    return "";
  }

  client.print(method);
  client.print(" ");
  client.print(path);
  client.println(" HTTP/1.1");

  client.print("Host: ");
  client.println(API_HOST);
  client.println("Connection: close");
  client.println("Accept: application/json");
  client.print("X-Device-Key: ");
  client.println(DEVICE_API_KEY);

  if (strcmp(method, "POST") == 0) {
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(body.length());
  }

  client.println();

  if (strcmp(method, "POST") == 0) {
    client.print(body);
  }

  String response;
  unsigned long started = millis();

  while (millis() - started < 10000) {
    while (client.available()) {
      response += (char)client.read();
      started = millis();
    }

    if (!client.connected() && !client.available()) {
      break;
    }
  }

  client.stop();

  int bodyStart = response.indexOf("\r\n\r\n");

  if (bodyStart < 0) {
    Serial.println("Respuesta HTTP incompleta.");
    return "";
  }

  return response.substring(bodyStart + 4);
}

// =====================================================
// SENSOR HC-SR04
// =====================================================

float readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(3);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);

  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000);

  if (duration == 0) {
    return -1.0;
  }

  return duration * 0.0343 / 2.0;
}

// =====================================================
// LOGICA AUTOMATICA
// =====================================================

String getAutomaticState(float distanceCm) {
  if (distanceCm < 0) {
    return "DETENIDO";
  }

  if (distanceCm >= 10 && distanceCm <= 25) {
    return "DERECHA";
  }

  if (distanceCm >= 26 && distanceCm <= 40) {
    return "IZQUIERDA";
  }

  return "DETENIDO";
}

// =====================================================
// CONSULTAR MODO Y COMANDO DE LA WEB
// =====================================================

void pollCommands() {
  String response = apiRequest(
    "GET",
    "/api/system/device/next"
  );

  if (response.length() == 0) {
    Serial.println("No se recibieron comandos.");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, response);

  if (error) {
    Serial.println("JSON de comandos no valido.");
    return;
  }

  const char* mode = doc["mode"] | "AUTOMATICO";
  const char* command = doc["command"] | "DETENIDO";
  int revision = doc["revision"] | 0;

  currentMode = String(mode);

  if (revision != lastRevision) {
    lastRevision = revision;

    if (currentMode == "MANUAL") {
      applyMotorState(String(command));
    } else {
      applyMotorState("DETENIDO");
    }
  }
}

// =====================================================
// ENVIAR TELEMETRIA
// =====================================================

void sendTelemetry(float distanceCm) {
  JsonDocument doc;

  doc["distanceCm"] = distanceCm >= 0 ? distanceCm : 0;
  doc["motorState"] = currentMotorState;

  String body;
  serializeJson(doc, body);

  String response = apiRequest(
    "POST",
    "/api/system/telemetry",
    body
  );

  if (response.length() > 0) {
    Serial.println("Telemetria enviada.");
  } else {
    Serial.println("No se pudo enviar la telemetria.");
  }
}

// =====================================================
// WIFI
// =====================================================

void connectWiFi() {
  Serial.print("Conectando a Wi-Fi: ");
  Serial.println(WIFI_SSID);

  while (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    delay(5000);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Wi-Fi conectado.");
  Serial.print("Direccion IP: ");
  Serial.println(WiFi.localIP());
}

// =====================================================
// SETUP
// =====================================================

void setup() {
  Serial.begin(115200);

  // Inicializar el motor y dejarlo detenido
  setupMotorShield();

  // Inicializar el sensor
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  Serial.println("EAI441 - Control de giro");
  Serial.println("Arduino UNO R4 WiFi");

  connectWiFi();
}

// =====================================================
// LOOP
// =====================================================

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  unsigned long now = millis();

  if (now - lastCommandRequest >= COMMAND_INTERVAL_MS) {
    lastCommandRequest = now;
    pollCommands();
  }

  float distanceCm = readDistanceCm();

  if (currentMode == "AUTOMATICO") {
    applyMotorState(getAutomaticState(distanceCm));
  }

  if (now - lastTelemetrySent >= TELEMETRY_INTERVAL_MS) {
    lastTelemetrySent = now;
    sendTelemetry(distanceCm);
  }

  delay(50);
}
