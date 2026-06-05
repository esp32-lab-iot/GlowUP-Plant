#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <FluxGarage_RoboEyes.h>
#include "secrets.h" // WIFI_SSID, WIFI_PASS e GOOGLE_SCRIPT_URL

// ==========================================
// CONFIGURAÇÕES DO HARDWARE
// ==========================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
RoboEyes<Adafruit_SSD1306> roboEyes(display);

// Cliente HTTPS global
WiFiClientSecure client;

const int SensorPin = 1;     // GPIO 1 - Sensor de umidade
#define BUZZER_PIN 4         // GPIO 4 - Buzzer ativo

const int AirValue = 3800;
const int WaterValue = 1326;

// ==========================================
// VARIÁVEIS GLOBAIS
// ==========================================
bool modoSilencioso = false;

int soilMoistureValue = 0;
int soilmoisturepercent = 0;

unsigned long temporizadorSensor = 0;
unsigned long temporizadorAnimacaoExtra = 0;
unsigned long temporizadorBuzzer = 0;
unsigned long temporizadorGoogleSheets = 0;

// Controle dos bipes da planta seca
unsigned long temporizadorBipSeco = 0;
bool bipSecoAtivo = false;
const unsigned long duracaoBipSeco = 70;

// Envio para Google Sheets
const unsigned long intervaloGoogleSheets = 3600000; // 1 hora

int estadoAtual = 2;
int ultimoEstado = -1;
bool estadoBuzzer = false;

// ==========================================
// CONVERTE ESTADO EM TEXTO
// ==========================================
String nomeEstado(int estado) {
  switch (estado) {
    case 0:
      return "Critico seco";
    case 1:
      return "Com sede";
    case 2:
      return "Perfeito";
    case 3:
      return "Excesso de agua";
    case 4:
      return "Critico afogando";
    default:
      return "Indefinido";
  }
}

// ==========================================
// LÊ SENSOR E ATUALIZA ESTADO DA PLANTA
// ==========================================
void atualizarSensorEEstado() {
  soilMoistureValue = analogRead(SensorPin);

  soilmoisturepercent = map(soilMoistureValue, AirValue, WaterValue, 0, 100);

  if (soilmoisturepercent > 100) soilmoisturepercent = 100;
  if (soilmoisturepercent < 0)   soilmoisturepercent = 0;

  Serial.print("Umidade Solo: ");
  Serial.print(soilmoisturepercent);
  Serial.print("% | ADC: ");
  Serial.println(soilMoistureValue);

  if (soilmoisturepercent <= 20)      estadoAtual = 0;
  else if (soilmoisturepercent <= 45) estadoAtual = 1;
  else if (soilmoisturepercent <= 75) estadoAtual = 2;
  else if (soilmoisturepercent <= 90) estadoAtual = 3;
  else                                estadoAtual = 4;

  if (estadoAtual != ultimoEstado) {
    ultimoEstado = estadoAtual;

    roboEyes.setHFlicker(OFF);
    roboEyes.setVFlicker(OFF);
    roboEyes.setSweat(OFF);
    roboEyes.setCuriosity(OFF);
    roboEyes.setPosition(DEFAULT);

    switch (estadoAtual) {
      case 0:
        Serial.println("-> Estado: Morrendo de Sede!");
        roboEyes.setMood(TIRED);
        roboEyes.setPosition(S);
        roboEyes.setVFlicker(ON, 2);
        break;

      case 1:
        Serial.println("-> Estado: Com Sede / Irritado");
        roboEyes.setMood(ANGRY);
        break;

      case 2:
        Serial.println("-> Estado: Perfeito e Feliz!");
        roboEyes.setMood(HAPPY);

        if (!modoSilencioso) {
          digitalWrite(BUZZER_PIN, HIGH); delay(15);
          digitalWrite(BUZZER_PIN, LOW);  delay(30);
          digitalWrite(BUZZER_PIN, HIGH); delay(15);
          digitalWrite(BUZZER_PIN, LOW);
        }
        break;

      case 3:
        Serial.println("-> Estado: Encharcado / Incomodado");
        roboEyes.setMood(TIRED);
        roboEyes.setSweat(ON);
        break;

      case 4:
        Serial.println("-> Estado: Afogando em Pânico!");
        roboEyes.setMood(ANGRY);
        roboEyes.setSweat(ON);
        roboEyes.setVFlicker(ON, 4);
        break;
    }
  }
}

// ==========================================
// AJUSTA HORÁRIO VIA NTP
// Necessário para HTTPS funcionar bem no ESP32-C3
// ==========================================
void ajustarHorarioNTP() {
  Serial.println("\nAjustando horario via NTP...");

  configTime(-3 * 3600, 0, "pool.ntp.org", "time.google.com", "time.nist.gov");

  struct tm timeinfo;
  int tentativas = 0;

  while (!getLocalTime(&timeinfo) && tentativas < 20) {
    roboEyes.update();
    delay(500);
    Serial.print(".");
    tentativas++;
  }

  if (getLocalTime(&timeinfo)) {
    Serial.println("\n[OK] Horario ajustado:");
    Serial.println(&timeinfo, "%d/%m/%Y %H:%M:%S");
  } else {
    Serial.println("\n[ERRO] Nao conseguiu ajustar horario via NTP.");
  }
}

// ==========================================
// ENVIA DADOS PARA O GOOGLE SHEETS
// ==========================================
void enviarDadosGoogleSheets() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[Sheets] Wi-Fi desconectado. Tentando reconectar...");
    WiFi.reconnect();
    return;
  }

  HTTPClient http;

  if (!http.begin(client, GOOGLE_SCRIPT_URL)) {
    Serial.println("[Sheets] Falha na conexão.");
    return;
  }

  // Evita resposta HTML "Moved Temporarily"
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  http.addHeader("Content-Type", "application/json");

  String payload = "{";
  payload += "\"umidade_solo\":" + String(soilmoisturepercent) + ",";
  payload += "\"leitura_adc\":" + String(soilMoistureValue) + ",";
  payload += "\"estado\":\"" + nomeEstado(estadoAtual) + "\"";
  payload += "}";

  int httpCode = http.POST(payload);

  if (httpCode == 200) {
    Serial.println("[Sheets] Dados enviados com sucesso.");
  } 
  else if (httpCode > 0) {
    Serial.print("[Sheets] Resposta HTTP inesperada: ");
    Serial.println(httpCode);
  } 
  else {
    Serial.print("[Sheets] Erro no envio: ");
    Serial.println(http.errorToString(httpCode));
  }

  http.end();
}

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  Wire.begin(8, 9);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Falha ao inicializar o SSD1306"));
    for (;;);
  }

  roboEyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, 100);
  roboEyes.setAutoblinker(ON, 3, 2);
  roboEyes.setIdleMode(ON, 2, 2);
  roboEyes.setBorderradius(6, 6);

  // -----------------------------------------------------------------
  // CONEXÃO WI-FI
  // -----------------------------------------------------------------
  Serial.print("Conectando à rede: ");
  Serial.println(WIFI_SSID);

  roboEyes.setMood(DEFAULT);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long temporizadorSerialWifi = millis();

  while (WiFi.status() != WL_CONNECTED) {
    roboEyes.update();

    if (millis() - temporizadorSerialWifi > 500) {
      temporizadorSerialWifi = millis();
      Serial.print(".");
    }
  }

  Serial.println("\nWi-Fi Conectado com sucesso!");
  Serial.print("IP obtido: ");
  Serial.println(WiFi.localIP());

  client.setInsecure();

  ajustarHorarioNTP();

  // Comemoração visual e sonora de conexão estabelecida
  roboEyes.setMood(HAPPY);

  if (!modoSilencioso) {
    digitalWrite(BUZZER_PIN, HIGH); delay(40);
    digitalWrite(BUZZER_PIN, LOW);  delay(40);
    digitalWrite(BUZZER_PIN, HIGH); delay(100);
    digitalWrite(BUZZER_PIN, LOW);
  }

  Serial.println("--- Planta Inteligente Online e Inicializada ---");

  // Primeira leitura e primeiro envio logo ao ligar
  atualizarSensorEEstado();
  enviarDadosGoogleSheets();

  // Reinicia o contador para o próximo envio acontecer daqui a 1 hora
  temporizadorGoogleSheets = millis();
}

// ==========================================
// LOOP
// ==========================================
void loop() {
  roboEyes.update();

  // -----------------------------------------------------------------
  // BLOCO 1: LEITURA DO SENSOR E MAPEAMENTO
  // -----------------------------------------------------------------
  if (millis() - temporizadorSensor > 2000) {
    temporizadorSensor = millis();
    atualizarSensorEEstado();
  }

  // -----------------------------------------------------------------
  // BLOCO 2: ALARMES SONOROS RÍTMICOS
  // -----------------------------------------------------------------

  if (modoSilencioso) {
    digitalWrite(BUZZER_PIN, LOW);
    bipSecoAtivo = false;
  }

  // Estado 4: crítico afogando — alarme rápido, em pânico
  else if (estadoAtual == 4) {
    if (millis() - temporizadorBuzzer > 100) {
      temporizadorBuzzer = millis();
      estadoBuzzer = !estadoBuzzer;
      digitalWrite(BUZZER_PIN, estadoBuzzer);
    }

    bipSecoAtivo = false;
  }

  // Estado 0: crítico seco — bipes curtos e cada vez mais lentos
  else if (estadoAtual == 0) {

    // Quanto mais seca a planta, maior o intervalo entre os bipes.
    // 20% -> bip a cada 900 ms
    // 0%  -> bip a cada 5000 ms
    unsigned long intervaloBipSeco = map(soilmoisturepercent, 0, 20, 5000, 900);

    if (!bipSecoAtivo) {
      if (millis() - temporizadorBipSeco > intervaloBipSeco) {
        temporizadorBipSeco = millis();
        bipSecoAtivo = true;
        digitalWrite(BUZZER_PIN, HIGH);
      }
    } else {
      if (millis() - temporizadorBipSeco > duracaoBipSeco) {
        bipSecoAtivo = false;
        digitalWrite(BUZZER_PIN, LOW);
        temporizadorBipSeco = millis();
      }
    }
  }

  // Estado 1: com sede — bip discreto ocasional
  else if (estadoAtual == 1) {
    unsigned long tempoCiclo = millis() % 4000;

    if (tempoCiclo < 60) {
      digitalWrite(BUZZER_PIN, HIGH);
    } else {
      digitalWrite(BUZZER_PIN, LOW);
    }

    bipSecoAtivo = false;
  }

  // Estados 2 e 3: sem alarme contínuo
  else {
    digitalWrite(BUZZER_PIN, LOW);
    bipSecoAtivo = false;
  }

  // -----------------------------------------------------------------
  // BLOCO 3: SURTOS DE ANIMAÇÃO DINÂMICA
  // -----------------------------------------------------------------
  if (millis() - temporizadorAnimacaoExtra > 8000) {
    temporizadorAnimacaoExtra = millis();

    if (estadoAtual == 2) {
      roboEyes.anim_laugh();
    }
    else if (estadoAtual == 1 || estadoAtual == 4) {
      roboEyes.anim_confused();
    }
  }

  // -----------------------------------------------------------------
  // BLOCO 4: ENVIO AO GOOGLE SHEETS
  // Envia uma vez ao ligar e depois a cada 1 hora
  // -----------------------------------------------------------------
  if (millis() - temporizadorGoogleSheets > intervaloGoogleSheets) {
    temporizadorGoogleSheets = millis();
    enviarDadosGoogleSheets();
  }
}