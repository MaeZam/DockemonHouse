#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include "SinricPro.h"
#include "SinricProSwitch.h"

// WiFi
const char* ssid = "MEGACABLE-2.4G-5BAB";
const char* password = "AfY3fRm3YE";

// Sinric Pro
#define APP_KEY "6e8e11ea-ab07-4819-a764-3e41b0b11998"
#define APP_SECRET "ac13f763-4c2a-43c8-b459-f581197a9c08-db0d8763-e48a-4306-8653-f894a5596efb"

// Device IDs - Sistema de Entrada
#define SISTEMA_SEGURIDAD_ID "6891cb17edeca866fe999161"
#define PUERTA_PRINCIPAL_ID "6891cb4c7104f15ae5337985"
#define PUERTA_TRASERA_ID "6891cba0ddd2551252bd36fa"

// Device IDs - Iluminación
#define LIGHT_SALA_ID "6891c6e1678c5bc9ab2611f1"
#define LIGHT_CUARTO2_ID "6891c748edeca866fe99900e"
#define LIGHT_COMEDOR_ID "6891ca0dddd2551252bd3654"
#define LIGHT_COCINA_ID "6891ca5dedeca866fe9990f1"
#define LIGHT_LAVANDERIA_ID "6891caa0678c5bc9ab2612e2"
#define LIGHT_BANO_ID "6891cac9edeca866fe99912a"

// Device IDs - Monitoreo
#define ALARMA_COCINA_ID "6891cbdc7104f15ae53379c4"

// Pines - Sistema de Entrada
#define TRIGGER_PIN 5
#define ECHO_PIN 18
#define SERVO_PRINCIPAL_PIN 21
#define SERVO_TRASERA_PIN 23  // NUEVO PIN para servo puerta trasera
#define BUZZER_SEGURIDAD_PIN 22

// Pines - Iluminación (6 LEDs)
#define LED_SALA 2
#define LED_CUARTO2 16
#define LED_COMEDOR 4
#define LED_COCINA 17
#define LED_LAVANDERIA 25
#define LED_BANO 26

// Pines - Monitoreo
#define DHT_PIN 15
#define BUZZER_COCINA_PIN 19
#define DHT_TYPE DHT11

// Variables - Sistema de Entrada
Servo puertaPrincipalServo;
Servo puertaTraseraServo;  // NUEVO servo para puerta trasera
bool sistemaArmado = true;
bool buzzerSeguridadActivo = false;
bool personaAutorizada = false;
bool alarmaAccesoManual = false;

// Variables - Iluminación
bool estadoSala = false;
bool estadoCuarto2 = false;
bool estadoComedor = false;
bool estadoCocina = false;
bool estadoLavanderia = false;
bool estadoBano = false;

// Variables - Monitoreo
DHT dht(DHT_PIN, DHT_TYPE);
bool alarmaCocinaActiva = false;
float temperatura = 0;
float humedad = 0;
float umbralTemperatura = 40.0;

WebServer server(80);

void setup() {
  Serial.begin(115200);

  configurarPines();
  conectarWiFi();
  configurarSinricPro();
  configurarServidor();

  dht.begin();

  Serial.println("DOCKEMONHOUSE");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  SinricPro.handle();
  server.handleClient();

  procesarSistemaEntrada();
  procesarMonitoreo();
  controlarBuzzers();

  delay(50);
}

void configurarPines() {
  // Sistema de entrada
  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_SEGURIDAD_PIN, OUTPUT);
  digitalWrite(BUZZER_SEGURIDAD_PIN, LOW);

  // Servos de puertas
  puertaPrincipalServo.attach(SERVO_PRINCIPAL_PIN);
  puertaTraseraServo.attach(SERVO_TRASERA_PIN);  // NUEVO servo
  puertaPrincipalServo.write(0);
  puertaTraseraServo.write(0);

  // Iluminación
  pinMode(LED_SALA, OUTPUT);
  pinMode(LED_CUARTO2, OUTPUT);
  pinMode(LED_COMEDOR, OUTPUT);
  pinMode(LED_COCINA, OUTPUT);
  pinMode(LED_LAVANDERIA, OUTPUT);
  pinMode(LED_BANO, OUTPUT);

  digitalWrite(LED_SALA, LOW);
  digitalWrite(LED_CUARTO2, LOW);
  digitalWrite(LED_COMEDOR, LOW);
  digitalWrite(LED_COCINA, LOW);
  digitalWrite(LED_LAVANDERIA, LOW);
  digitalWrite(LED_BANO, LOW);

  // Monitoreo
  pinMode(BUZZER_COCINA_PIN, OUTPUT);
  digitalWrite(BUZZER_COCINA_PIN, LOW);
}

void conectarWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
  }
}

void configurarSinricPro() {
  // Sistema de entrada
  SinricProSwitch& sistema = SinricPro[SISTEMA_SEGURIDAD_ID];
  sistema.onPowerState(onSistemaSeguridad);

  SinricProSwitch& puertaPrincipal = SinricPro[PUERTA_PRINCIPAL_ID];
  puertaPrincipal.onPowerState(onPuertaPrincipal);

  // NUEVA configuración para puerta trasera
  SinricProSwitch& puertaTrasera = SinricPro[PUERTA_TRASERA_ID];
  puertaTrasera.onPowerState(onPuertaTrasera);

  // Iluminación
  SinricProSwitch& sala = SinricPro[LIGHT_SALA_ID];
  sala.onPowerState(onLuzSala);

  SinricProSwitch& cuarto2 = SinricPro[LIGHT_CUARTO2_ID];
  cuarto2.onPowerState(onLuzCuarto2);

  SinricProSwitch& comedor = SinricPro[LIGHT_COMEDOR_ID];
  comedor.onPowerState(onLuzComedor);

  SinricProSwitch& cocina = SinricPro[LIGHT_COCINA_ID];
  cocina.onPowerState(onLuzCocina);

  SinricProSwitch& lavanderia = SinricPro[LIGHT_LAVANDERIA_ID];
  lavanderia.onPowerState(onLuzLavanderia);

  SinricProSwitch& bano = SinricPro[LIGHT_BANO_ID];
  bano.onPowerState(onLuzBano);

  // Monitoreo
  SinricProSwitch& alarmaCocina = SinricPro[ALARMA_COCINA_ID];
  alarmaCocina.onPowerState(onAlarmaCocina);

  SinricPro.begin(APP_KEY, APP_SECRET);
}

void configurarServidor() {
  server.enableCORS(true);

  // Páginas principales
  server.on("/", HTTP_GET, handleRoot);
  server.on("/estado", HTTP_GET, handleEstado);

  // Sistema de entrada
  server.on("/sistema", HTTP_GET, handleSistema);
  server.on("/puerta", HTTP_GET, handlePuerta);
  server.on("/puerta_trasera", HTTP_GET, handlePuertaTrasera);  // NUEVO endpoint
  server.on("/autorizar", HTTP_GET, handleAutorizar);
  server.on("/forzar", HTTP_GET, handleForzar);

  // Alarmas
  server.on("/alarma_acceso/encender", HTTP_GET, handleAlarmaAccesoOn);
  server.on("/alarma_acceso/apagar", HTTP_GET, handleAlarmaAccesoOff);

  // Iluminación
  server.on("/api/control", HTTP_POST, handleControlLuces);
  server.on("/api/status", HTTP_GET, handleStatusLuces);

  // Monitoreo
  server.on("/alarma/encender", HTTP_GET, handleAlarmaOn);
  server.on("/alarma/apagar", HTTP_GET, handleAlarmaOff);

  server.begin();
}

void procesarSistemaEntrada() {
  if (sistemaArmado) {
    float distancia = medirDistancia();
    if (distancia > 0 && distancia <= 10) {
      static unsigned long ultimaDeteccion = 0;
      if (millis() - ultimaDeteccion > 2000) {
        if (random(0, 100) > 40) {
          personaAutorizada = true;
        } else {
          personaAutorizada = false;
          buzzerSeguridadActivo = true;
        }
        ultimaDeteccion = millis();
      }
    }
  }
}

void procesarMonitoreo() {
  static unsigned long ultimaLectura = 0;
  if (millis() - ultimaLectura > 2000) {
    leerSensorDHT();
    if (temperatura >= umbralTemperatura && !alarmaCocinaActiva) {
      alarmaCocinaActiva = true;
    }
    ultimaLectura = millis();
  }
}

void controlarBuzzers() {
  static unsigned long ultimoBuzzer = 0;
  if (millis() - ultimoBuzzer > 500) {
    // Buzzer seguridad
    if (buzzerSeguridadActivo || alarmaAccesoManual) {
      digitalWrite(BUZZER_SEGURIDAD_PIN, !digitalRead(BUZZER_SEGURIDAD_PIN));
    } else {
      digitalWrite(BUZZER_SEGURIDAD_PIN, LOW);
    }

    // Buzzer cocina
    if (alarmaCocinaActiva) {
      digitalWrite(BUZZER_COCINA_PIN, !digitalRead(BUZZER_COCINA_PIN));
    } else {
      digitalWrite(BUZZER_COCINA_PIN, LOW);
    }

    ultimoBuzzer = millis();
  }
}

float medirDistancia() {
  digitalWrite(TRIGGER_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIGGER_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGGER_PIN, LOW);

  long duracion = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duracion == 0) return 0;

  return duracion * 0.034 / 2;
}

void abrirPuertaPrincipal() {
  puertaPrincipalServo.write(180);
  delay(3000);
  puertaPrincipalServo.write(0);
}

// NUEVA función para puerta trasera
void abrirPuertaTrasera() {
  puertaTraseraServo.write(180);
  delay(3000);
  puertaTraseraServo.write(0);
}

void leerSensorDHT() {
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  if (!isnan(temp) && !isnan(hum)) {
    temperatura = temp;
    humedad = hum;
  }
}

// Callbacks Sinric Pro - Sistema de Entrada
bool onSistemaSeguridad(const String &deviceId, bool &state) {
  sistemaArmado = state;
  buzzerSeguridadActivo = false;
  return true;
}

bool onPuertaPrincipal(const String &deviceId, bool &state) {
  if (state) {
    if (personaAutorizada || !sistemaArmado) {
      abrirPuertaPrincipal();
      return true;
    } else {
      buzzerSeguridadActivo = true;
      return false;
    }
  }
  return true;
}

// NUEVO callback para puerta trasera
bool onPuertaTrasera(const String &deviceId, bool &state) {
  if (state) {
    abrirPuertaTrasera();
    return true;
  }
  return true;
}

// Callbacks Sinric Pro - Iluminación
bool onLuzSala(const String &deviceId, bool &state) {
  estadoSala = state;
  digitalWrite(LED_SALA, state ? HIGH : LOW);
  return true;
}

bool onLuzCuarto2(const String &deviceId, bool &state) {
  estadoCuarto2 = state;
  digitalWrite(LED_CUARTO2, state ? HIGH : LOW);
  return true;
}

bool onLuzComedor(const String &deviceId, bool &state) {
  estadoComedor = state;
  digitalWrite(LED_COMEDOR, state ? HIGH : LOW);
  return true;
}

bool onLuzCocina(const String &deviceId, bool &state) {
  estadoCocina = state;
  digitalWrite(LED_COCINA, state ? HIGH : LOW);
  return true;
}

bool onLuzLavanderia(const String &deviceId, bool &state) {
  estadoLavanderia = state;
  digitalWrite(LED_LAVANDERIA, state ? HIGH : LOW);
  return true;
}

bool onLuzBano(const String &deviceId, bool &state) {
  estadoBano = state;
  digitalWrite(LED_BANO, state ? HIGH : LOW);
  return true;
}

// Callback Sinric Pro - Monitoreo
bool onAlarmaCocina(const String &deviceId, bool &state) {
  alarmaCocinaActiva = state;
  return true;
}

// Handlers Web
void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<title>DOCKEMONHOUSE</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body{font-family:Arial;margin:20px;background:#2c3e50;color:white;}";
  html += ".container{max-width:1000px;margin:0 auto;background:#34495e;padding:30px;border-radius:15px;}";
  html += ".section{margin:25px 0;padding:20px;border-radius:10px;background:#1a252f;}";
  html += ".controls{text-align:center;margin:15px 0;}";
  html += "button,a{display:inline-block;padding:12px 20px;margin:8px;font-size:14px;text-decoration:none;border:none;border-radius:6px;cursor:pointer;}";
  html += ".btn-green{background:#27ae60;color:white;}";
  html += ".btn-red{background:#e74c3c;color:white;}";
  html += ".btn-blue{background:#3498db;color:white;}";
  html += ".btn-orange{background:#f39c12;color:white;}";
  html += ".btn-purple{background:#9b59b6;color:white;}";
  html += ".status{padding:15px;margin:10px 0;border-radius:8px;text-align:center;font-weight:bold;}";
  html += ".status-on{background:#27ae60;}";
  html += ".status-off{background:#e74c3c;}";
  html += ".status-normal{background:#2980b9;}";
  html += "h2{color:#ecf0f1;border-bottom:2px solid #3498db;padding-bottom:10px;}";
  html += "h3{color:#bdc3c7;margin-top:20px;}";
  html += ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:15px;}";
  html += ".led-card{background:#283747;padding:15px;border-radius:8px;text-align:center;}";
  html += ".door-section{background:#2c3e50;margin:15px 0;padding:20px;border-radius:10px;border:2px solid #34495e;}";
  html += "</style></head><body>";

  html += "<div class='container'>";
  html += "<h1 style='text-align:center'>DOCKEMONHOUSE</h1>";

  // Sección Sistema de Entrada
  html += "<div class='section'>";
  html += "<h2>Sistema de seguridad</h2>";
  html += "<div class='status " + String(sistemaArmado ? "status-on" : "status-off") + "'>";
  html += "Sistema: " + String(sistemaArmado ? "ACTIVADA" : "DESACTIVADO");
  html += "</div>";
  html += "<div class='status " + String(personaAutorizada ? "status-on" : "status-off") + "'>";
  html += "Persona: " + String(personaAutorizada ? "AUTORIZADA" : "NO AUTORIZADA");
  html += "</div>";
  html += "<div class='status status-normal'>";
  html += "Distancia: " + String(medirDistancia()) + " cm";
  html += "</div>";
  html += "<div class='controls'>";
  html += "<a href='/sistema?estado=true' class='btn-green'>Armar Sistema</a>";
  html += "<a href='/sistema?estado=false' class='btn-red'>Desarmar Sistema</a>";
  html += "<a href='/autorizar' class='btn-blue'>Autorizar Persona</a>";
  html += "</div>";

  // Subsección Puertas
  html += "<div class='door-section'>";
  html += "<h3>Control de Puertas</h3>";
  html += "<div style='display:grid;grid-template-columns:1fr 1fr;gap:20px;'>";

  html += "<div style='text-align:center;'>";
  html += "<h4>Puerta Principal</h4>";
  html += "<div class='status status-normal'>Controlada por Sistema de Seguridad</div>";
  html += "<a href='/puerta' class='btn-orange'>Abrir Principal</a>";
  html += "</div>";

  html += "<div style='text-align:center;'>";
  html += "<h4>Puerta Trasera</h4>";
  html += "<div class='status status-normal'>Acceso Independiente</div>";
  html += "<a href='/puerta_trasera' class='btn-purple'>Abrir Trasera</a>";
  html += "</div>";

  html += "</div></div></div>";

  // Sección Iluminación
  html += "<div class='section'>";
  html += "<h2>Control de Iluminación</h2>";
  html += "<div class='controls'>";
  html += "<button class='btn-green' onclick='controlAll(\"on\")'>Encender Todas</button>";
  html += "<button class='btn-red' onclick='controlAll(\"off\")'>Apagar Todas</button>";
  html += "</div>";

  html += "<div class='grid'>";

  html += "<div class='led-card'>";
  html += "<h4>Sala</h4>";
  html += "<div class='status " + String(estadoSala ? "status-on" : "status-off") + "'>";
  html += estadoSala ? "ENCENDIDA" : "APAGADA";
  html += "</div>";
  html += "<button class='btn-green' onclick='controlLED(\"sala\", \"on\")'>ON</button>";
  html += "<button class='btn-red' onclick='controlLED(\"sala\", \"off\")'>OFF</button>";
  html += "</div>";

  html += "<div class='led-card'>";
  html += "<h4>Cuarto 2</h4>";
  html += "<div class='status " + String(estadoCuarto2 ? "status-on" : "status-off") + "'>";
  html += estadoCuarto2 ? "ENCENDIDA" : "APAGADA";
  html += "</div>";
  html += "<button class='btn-green' onclick='controlLED(\"cuarto2\", \"on\")'>ON</button>";
  html += "<button class='btn-red' onclick='controlLED(\"cuarto2\", \"off\")'>OFF</button>";
  html += "</div>";

  html += "<div class='led-card'>";
  html += "<h4>Comedor</h4>";
  html += "<div class='status " + String(estadoComedor ? "status-on" : "status-off") + "'>";
  html += estadoComedor ? "ENCENDIDA" : "APAGADA";
  html += "</div>";
  html += "<button class='btn-green' onclick='controlLED(\"comedor\", \"on\")'>ON</button>";
  html += "<button class='btn-red' onclick='controlLED(\"comedor\", \"off\")'>OFF</button>";
  html += "</div>";

  html += "<div class='led-card'>";
  html += "<h4>Cocina</h4>";
  html += "<div class='status " + String(estadoCocina ? "status-on" : "status-off") + "'>";
  html += estadoCocina ? "ENCENDIDA" : "APAGADA";
  html += "</div>";
  html += "<button class='btn-green' onclick='controlLED(\"cocina\", \"on\")'>ON</button>";
  html += "<button class='btn-red' onclick='controlLED(\"cocina\", \"off\")'>OFF</button>";
  html += "</div>";

  html += "<div class='led-card'>";
  html += "<h4>Lavandería</h4>";
  html += "<div class='status " + String(estadoLavanderia ? "status-on" : "status-off") + "'>";
  html += estadoLavanderia ? "ENCENDIDA" : "APAGADA";
  html += "</div>";
  html += "<button class='btn-green' onclick='controlLED(\"lavanderia\", \"on\")'>ON</button>";
  html += "<button class='btn-red' onclick='controlLED(\"lavanderia\", \"off\")'>OFF</button>";
  html += "</div>";

  html += "<div class='led-card'>";
  html += "<h4>Baño</h4>";
  html += "<div class='status " + String(estadoBano ? "status-on" : "status-off") + "'>";
  html += estadoBano ? "ENCENDIDA" : "APAGADA";
  html += "</div>";
  html += "<button class='btn-green' onclick='controlLED(\"bano\", \"on\")'>ON</button>";
  html += "<button class='btn-red' onclick='controlLED(\"bano\", \"off\")'>OFF</button>";
  html += "</div>";

  html += "</div></div>";

  // Sección Monitoreo y Alarmas
  html += "<div class='section'>";
  html += "<h2>Monitoreo y Alarmas</h2>";

  // Subsección Cocina
  html += "<h3>Cocina</h3>";
  html += "<div class='status status-normal'>";
  html += "Temperatura: " + String(temperatura, 1) + "°C";
  html += "</div>";
  html += "<div class='status status-normal'>";
  html += "Humedad: " + String(humedad, 1) + "%";
  html += "</div>";
  html += "<div class='status " + String(alarmaCocinaActiva ? "status-on" : "status-off") + "'>";
  html += "Alarma Cocina: " + String(alarmaCocinaActiva ? "ACTIVA" : "INACTIVA");
  html += "</div>";
  html += "<div class='controls'>";
  html += "<a href='/alarma/encender' class='btn-red'>Encender Alarma Cocina</a>";
  html += "<a href='/alarma/apagar' class='btn-green'>Apagar Alarma Cocina</a>";
  html += "</div>";

  // Subsección Alarma de Acceso
  html += "<h3>Alarma de Acceso</h3>";
  html += "<div class='status " + String((buzzerSeguridadActivo || alarmaAccesoManual) ? "status-on" : "status-off") + "'>";
  html += "Alarma Acceso: " + String((buzzerSeguridadActivo || alarmaAccesoManual) ? "ACTIVA" : "INACTIVA");
  html += "</div>";
  html += "<div class='status status-normal'>";
  html += "Modo: " + String(alarmaAccesoManual ? "MANUAL" : (buzzerSeguridadActivo ? "AUTOMÁTICA" : "DESACTIVADA"));
  html += "</div>";
  html += "<div class='controls'>";
  html += "<a href='/alarma_acceso/encender' class='btn-red'>Activar Alarma Acceso</a>";
  html += "<a href='/alarma_acceso/apagar' class='btn-green'>Desactivar Alarma Acceso</a>";
  html += "</div>";

  html += "</div>";

  html += "</div>";

  // JavaScript
  html += "<script>";
  html += "function controlLED(room, action) {";
  html += "  fetch('/api/control', {";
  html += "    method: 'POST',";
  html += "    headers: {'Content-Type': 'application/json'},";
  html += "    body: JSON.stringify({device: 'light', room: room, action: action})";
  html += "  }).then(() => setTimeout(() => location.reload(), 500));";
  html += "}";

  html += "function controlAll(action) {";
  html += "  const rooms = ['sala', 'cuarto2', 'comedor', 'cocina', 'lavanderia', 'bano'];";
  html += "  rooms.forEach((room, i) => {";
  html += "    setTimeout(() => controlLED(room, action), i * 200);";
  html += "  });";
  html += "}";
  html += "</script>";

  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleEstado() {
  String json = "{";
  json += "\"sistema\":" + String(sistemaArmado ? "true" : "false") + ",";
  json += "\"persona_autorizada\":" + String(personaAutorizada ? "true" : "false") + ",";
  json += "\"distancia\":" + String(medirDistancia()) + ",";
  json += "\"temperatura\":" + String(temperatura) + ",";
  json += "\"humedad\":" + String(humedad) + ",";
  json += "\"alarma_cocina\":" + String(alarmaCocinaActiva ? "true" : "false") + ",";
  json += "\"alarma_acceso\":" + String((buzzerSeguridadActivo || alarmaAccesoManual) ? "true" : "false") + ",";
  json += "\"luces\":{";
  json += "\"sala\":" + String(estadoSala ? "true" : "false") + ",";
  json += "\"cuarto2\":" + String(estadoCuarto2 ? "true" : "false") + ",";
  json += "\"comedor\":" + String(estadoComedor ? "true" : "false") + ",";
  json += "\"cocina\":" + String(estadoCocina ? "true" : "false") + ",";
  json += "\"lavanderia\":" + String(estadoLavanderia ? "true" : "false") + ",";
  json += "\"bano\":" + String(estadoBano ? "true" : "false");
  json += "}}";
  server.send(200, "application/json", json);
}

void handleSistema() {
  String estado = server.arg("estado");
  if (estado == "true") {
    sistemaArmado = true;
    buzzerSeguridadActivo = false;
  } else if (estado == "false") {
    sistemaArmado = false;
    buzzerSeguridadActivo = false;
  }

  String html = "<h1 style='text-align:center;color:" + String(sistemaArmado ? "green" : "orange") + "'>Sistema " + String(sistemaArmado ? "ARMADO" : "DESARMADO") + "</h1>";
  html += "<p style='text-align:center'><a href='/'>Volver</a></p>";
  server.send(200, "text/html", html);
}

void handlePuerta() {
  if (personaAutorizada || !sistemaArmado) {
    abrirPuertaPrincipal();
    String html = "<h1 style='text-align:center;color:green'>Puerta Principal Abierta</h1>";
    html += "<p style='text-align:center'>Acceso autorizado por sistema de seguridad</p>";
    html += "<p style='text-align:center'><a href='/'>Volver</a></p>";
    server.send(200, "text/html", html);
  } else {
    String html = "<h1 style='text-align:center;color:red'>Acceso Denegado</h1>";
    html += "<p style='text-align:center'>Sistema armado - Persona no autorizada</p>";
    html += "<p style='text-align:center'><a href='/autorizar'>Autorizar</a> | <a href='/'>Volver</a></p>";
    server.send(200, "text/html", html);
  }
}

// NUEVO handler para puerta trasera
void handlePuertaTrasera() {
  abrirPuertaTrasera();
  String html = "<h1 style='text-align:center;color:purple'>Puerta Trasera Abierta</h1>";
  html += "<p style='text-align:center'>Acceso independiente del sistema de seguridad</p>";
  html += "<p style='text-align:center'><a href='/'>Volver</a></p>";
  server.send(200, "text/html", html);
}

void handleAutorizar() {
  personaAutorizada = true;
  buzzerSeguridadActivo = false;

  String html = "<h1 style='text-align:center;color:green'>Persona Autorizada</h1>";
  html += "<p style='text-align:center'><a href='/puerta'>Abrir Puerta Principal</a> | <a href='/'>Volver</a></p>";
  server.send(200, "text/html", html);
}

void handleForzar() {
  abrirPuertaPrincipal();
  String html = "<h1 style='text-align:center;color:orange'>Apertura Forzada</h1>";
  html += "<p style='text-align:center'><a href='/'>Volver</a></p>";
  server.send(200, "text/html", html);
}

void handleAlarmaAccesoOn() {
  alarmaAccesoManual = true;
  buzzerSeguridadActivo = false;

  Serial.println("Alarma de acceso activada manualmente");

  String html = "<h1 style='text-align:center;color:red'>Alarma de Acceso Activada</h1>";
  html += "<p style='text-align:center'>La alarma de acceso está ahora ENCENDIDA</p>";
  html += "<p style='text-align:center'><a href='/alarma_acceso/apagar'>Apagar Alarma</a> | <a href='/'>Volver</a></p>";
  server.send(200, "text/html", html);
}

void handleAlarmaAccesoOff() {
  alarmaAccesoManual = false;
  buzzerSeguridadActivo = false;

  Serial.println("Alarma de acceso desactivada manualmente");

  String html = "<h1 style='text-align:center;color:green'>Alarma de Acceso Desactivada</h1>";
  html += "<p style='text-align:center'>La alarma de acceso está ahora APAGADA</p>";
  html += "<p style='text-align:center'><a href='/alarma_acceso/encender'>Encender Alarma</a> | <a href='/'>Volver</a></p>";
  server.send(200, "text/html", html);
}

void handleControlLuces() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }

  String body = server.arg("plain");
  StaticJsonDocument<200> json;

  if (deserializeJson(json, body)) {
    server.send(400, "application/json", "{\"success\":false}");
    return;
  }

  String room = json["room"];
  String action = json["action"];
  bool newState = (action == "on");
  bool success = false;

  if (room == "sala") {
    estadoSala = newState;
    digitalWrite(LED_SALA, newState ? HIGH : LOW);
    SinricProSwitch& sw = SinricPro[LIGHT_SALA_ID];
    sw.sendPowerStateEvent(newState);
    success = true;
  } else if (room == "cuarto2") {
    estadoCuarto2 = newState;
    digitalWrite(LED_CUARTO2, newState ? HIGH : LOW);
    SinricProSwitch& sw = SinricPro[LIGHT_CUARTO2_ID];
    sw.sendPowerStateEvent(newState);
    success = true;
  } else if (room == "comedor") {
    estadoComedor = newState;
    digitalWrite(LED_COMEDOR, newState ? HIGH : LOW);
    SinricProSwitch& sw = SinricPro[LIGHT_COMEDOR_ID];
    sw.sendPowerStateEvent(newState);
    success = true;
  } else if (room == "cocina") {
    estadoCocina = newState;
    digitalWrite(LED_COCINA, newState ? HIGH : LOW);
    SinricProSwitch& sw = SinricPro[LIGHT_COCINA_ID];
    sw.sendPowerStateEvent(newState);
    success = true;
  } else if (room == "lavanderia") {
    estadoLavanderia = newState;
    digitalWrite(LED_LAVANDERIA, newState ? HIGH : LOW);
    SinricProSwitch& sw = SinricPro[LIGHT_LAVANDERIA_ID];
    sw.sendPowerStateEvent(newState);
    success = true;
  } else if (room == "bano") {
    estadoBano = newState;
    digitalWrite(LED_BANO, newState ? HIGH : LOW);
    SinricProSwitch& sw = SinricPro[LIGHT_BANO_ID];
    sw.sendPowerStateEvent(newState);
    success = true;
  }

  server.send(200, "application/json", "{\"success\":" + String(success ? "true" : "false") + "}");
}

void handleStatusLuces() {
  StaticJsonDocument<300> json;

  json["lights"]["sala"] = estadoSala;
  json["lights"]["cuarto2"] = estadoCuarto2;
  json["lights"]["comedor"] = estadoComedor;
  json["lights"]["cocina"] = estadoCocina;
  json["lights"]["lavanderia"] = estadoLavanderia;
  json["lights"]["bano"] = estadoBano;

  String response;
  serializeJson(json, response);
  server.send(200, "application/json", response);
}

void handleAlarmaOn() {
  alarmaCocinaActiva = true;

  String html = "<h1 style='text-align:center;color:red'>Alarma Cocina Encendida</h1>";
  html += "<p style='text-align:center'><a href='/'>Volver</a></p>";
  server.send(200, "text/html", html);
}

void handleAlarmaOff() {
  alarmaCocinaActiva = false;

  String html = "<h1 style='text-align:center;color:green'>Alarma Cocina Apagada</h1>";
  html += "<p style='text-align:center'><a href='/'>Volver</a></p>";
  server.send(200, "text/html", html);
}