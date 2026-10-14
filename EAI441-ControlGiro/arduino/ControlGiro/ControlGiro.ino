#include <WiFiS3.h>
#include <WiFiSSLClient.h>
#include <ArduinoJson.h>
#include "arduino_secrets.h"

// =====================================================
// CONFIGURACION
// =====================================================

const char WIFI_SSID[] = SECRET_SSID;
const char WIFI_PASSWORD[] = SECRET_PASS;
const char DEVICE_API_KEY[] = SECRET_DEVICE_API_KEY;

const char API_HOST[] = "eai441-control-giro-api.onrender.com";
const char DEVICE_API_KEY[] = "PEGA_AQUI_TU_DEVICE_API_KEY";

const int API_PORT = 443;

// Pines provisionales del HC-SR04.
// Confirma que el shield no los utiliza antes de conectar.
const int TRIG_PIN = 7;
const int ECHO_PIN = 8;

const unsigned long COMMAND_INTERVAL_MS = 1500;
const unsigned long TELEMETRY_INTERVAL_MS = 3000;

WiFiSSLClient sslClient;

String currentMode = "AUTOMATICO";
String currentMotorState = "DETENIDO";
int lastRevision = -1;

unsigned long lastCommandRequest = 0;
unsigned long lastTelemetrySent = 0;

// =====================================================
// COMUNICACION HTTPS CON LA API
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
    Serial.println("No se pudo conectar con Render.");
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
  client.println("X-Device-Key: " + String(DEVICE_API_KEY));

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

  String httpBody = response.substring(bodyStart + 4);

  Serial.println("Respuesta recibida.");
  return httpBody;
}

// =====================================================
// SENSOR ULTRASONICO HC-SR04
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
// LOGICA DE GIRO AUTOMATICO
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
// CONTROL DEL MOTOR
// =====================================================

// IMPORTANTE:
// Esta funcion NO acciona el motor todavia.
// Debe implementarse cuando confirmemos el modelo
// exacto del shield L293D y su biblioteca compatible.

void applyMotorState(const String& newState) {
  if (newState != "DERECHA" &&
      newState != "IZQUIERDA" &&
      newState != "DETENIDO") {
    return;
  }

  currentMotorState = newState;

  Serial.print("Estado solicitado para el motor: ");
  Serial.println(currentMotorState);
}

// =====================================================
// CONSULTAR MODO Y COMANDO
// =====================================================

void pollCommands() {
  String response = apiRequest(
    "GET",
    "/api/system/device/next"
  );

  if (response.length() == 0) {
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
      // El movimiento automatico lo decide el sensor.
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
// CONEXION WIFI
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

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  digitalWrite(TRIG_PIN, LOW);

  Serial.println("EAI441 - Control de giro");
  Serial.println("Inicializando Arduino UNO R4 WiFi...");

  connectWiFi();
}

// =====================================================
// LOOP PRINCIPAL
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
