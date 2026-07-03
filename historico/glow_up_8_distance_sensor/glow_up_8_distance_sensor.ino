#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <FluxGarage_RoboEyes.h>
#include "Adafruit_VL53L0X.h"
#include "esp_sleep.h"
#include "secrets.h" // WIFI_SSID, WIFI_PASS e GOOGLE_SCRIPT_URL

// ==========================================
// CONFIGURAÇÕES DO HARDWARE
// ==========================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

#define SDA_PIN 8
#define SCL_PIN 9

const int SensorPin = 1;     // GPIO 1 - Sensor de umidade
#define BUZZER_PIN 4         // GPIO 4 - Buzzer ativo

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
RoboEyes<Adafruit_SSD1306> roboEyes(display);
Adafruit_VL53L0X lox = Adafruit_VL53L0X();

// Cliente HTTPS global
WiFiClientSecure client;

const int AirValue = 3800;
const int WaterValue = 1326;

// ==========================================
// CONFIGURAÇÕES DO SENSOR DE DISTÂNCIA / SONO
// ==========================================
const int LIMIAR_PRESENCA_MM = 1000;  // pessoa a menos de 1 m
const int LIMIAR_SAIDA_MM    = 1200;  // histerese para considerar afastamento

const uint64_t TEMPO_DEEP_SLEEP_US = 5000000ULL; // acorda a cada 5 segundos

const unsigned long TEMPO_AFASTADO_PARA_DORMIR = 30000; // 30 segundos
const unsigned long JANELA_UPLOAD_MS = 8000; // segurança para novo upload após reset/manual

// ==========================================
// VARIÁVEIS GLOBAIS
// ==========================================
bool modoSilencioso = false;

int soilMoistureValue = 0;
int soilmoisturepercent = 0;

unsigned long temporizadorSensor = 0;
unsigned long temporizadorDistancia = 0;
unsigned long temporizadorAnimacaoExtra = 0;
unsigned long temporizadorBuzzer = 0;
unsigned long temporizadorGoogleSheets = 0;
unsigned long tempoUltimaPresenca = 0;

// Controle dos bipes da planta seca
unsigned long temporizadorBipSeco = 0;
bool bipSecoAtivo = false;
const unsigned long duracaoBipSeco = 70;

// Envio para Google Sheets
const unsigned long intervaloGoogleSheets = 3600000; // 1 hora

int estadoAtual = 2;
int ultimoEstado = -1;
bool estadoBuzzer = false;

bool displayInicializado = false;
bool sistemaAtivo = false;

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
// LÊ DISTÂNCIA UMA VEZ
// ==========================================
bool medirDistancia(int &distancia_mm) {
  VL53L0X_RangingMeasurementData_t medida;

  lox.rangingTest(&medida, false);

  if (medida.RangeStatus != 4) {
    distancia_mm = medida.RangeMilliMeter;

    if (distancia_mm > 0 && distancia_mm < 2000) {
      return true;
    }
  }

  return false;
}

// ==========================================
// ENTRA EM DEEP SLEEP POR TIMER
// ==========================================
void entrarEmDeepSleep() {
  Serial.println("Preparando para deep sleep...");

  digitalWrite(BUZZER_PIN, LOW);

  if (displayInicializado) {
    display.clearDisplay();
    display.display();
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  Serial.println("Dormindo. Vou acordar em 1 segundo para verificar presenca.");
  Serial.flush();

  esp_sleep_enable_timer_wakeup(TEMPO_DEEP_SLEEP_US);

  delay(100);
  esp_deep_sleep_start();
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
// CONTROLE DO BUZZER
// ==========================================
void atualizarBuzzer() {
  if (modoSilencioso) {
    digitalWrite(BUZZER_PIN, LOW);
    bipSecoAtivo = false;
    return;
  }

  // Estado 4: crítico afogando — alarme rápido, em pânico
  if (estadoAtual == 4) {
    if (millis() - temporizadorBuzzer > 100) {
      temporizadorBuzzer = millis();
      estadoBuzzer = !estadoBuzzer;
      digitalWrite(BUZZER_PIN, estadoBuzzer);
    }

    bipSecoAtivo = false;
  }

  // Estado 0: crítico seco — bipes curtos e cada vez mais lentos
  else if (estadoAtual == 0) {
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
}

// ==========================================
// INICIALIZA OLED E ROBOEYES
// ==========================================
void inicializarDisplayERoboEyes() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Falha ao inicializar o SSD1306"));
    for (;;);
  }

  displayInicializado = true;
  display.ssd1306_command(SSD1306_DISPLAYON);

  roboEyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, 100);
  roboEyes.setAutoblinker(ON, 3, 2);
  roboEyes.setIdleMode(ON, 2, 2);
  roboEyes.setBorderradius(6, 6);
  roboEyes.setMood(DEFAULT);
}

// ==========================================
// INICIALIZA WI-FI
// ==========================================
void inicializarWiFi() {
  Serial.print("Conectando à rede: ");
  Serial.println(WIFI_SSID);

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
}

// ==========================================
// INICIALIZA SISTEMA ATIVO DA PLANTA
// ==========================================
void inicializarSistemaAtivo() {
  sistemaAtivo = true;

  inicializarDisplayERoboEyes();

  roboEyes.setMood(DEFAULT);

  inicializarWiFi();

  // Comemoração visual e sonora de conexão estabelecida
  roboEyes.setMood(HAPPY);

  if (!modoSilencioso) {
    digitalWrite(BUZZER_PIN, HIGH); delay(40);
    digitalWrite(BUZZER_PIN, LOW);  delay(40);
    digitalWrite(BUZZER_PIN, HIGH); delay(100);
    digitalWrite(BUZZER_PIN, LOW);
  }

  Serial.println("--- GlowUp Plant Online e Inicializada ---");

  // Primeira leitura e primeiro envio logo ao ativar
  atualizarSensorEEstado();
  enviarDadosGoogleSheets();

  temporizadorGoogleSheets = millis();
  temporizadorSensor = millis();
  temporizadorDistancia = millis();
  tempoUltimaPresenca = millis();
}

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("===== GlowUp Plant - Deep Sleep por Presenca =====");

  esp_sleep_wakeup_cause_t motivo = esp_sleep_get_wakeup_cause();

  if (motivo == ESP_SLEEP_WAKEUP_TIMER) {
    Serial.println("Acordou pelo timer.");
  } else {
    Serial.println("Inicializacao normal, reset ou upload.");
    Serial.println("Janela de seguranca para novo upload...");
    delay(JANELA_UPLOAD_MS);
  }

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // OLED e VL53L0X compartilham o mesmo barramento I2C
  Wire.begin(SDA_PIN, SCL_PIN);

  // Inicializa apenas o sensor de distância para decidir se ativa o sistema
  if (!lox.begin(0x29, false, &Wire, Adafruit_VL53L0X::VL53L0X_SENSE_HIGH_SPEED)) {
    Serial.println("Falha ao inicializar VL53L0X.");
    Serial.println("Sem sensor de presenca, sistema ficara ativo para diagnostico.");

    inicializarSistemaAtivo();
    return;
  }

  int distancia = 0;

  if (medirDistancia(distancia)) {
    Serial.print("Distancia inicial: ");
    Serial.print(distancia);
    Serial.println(" mm");

    if (distancia < LIMIAR_PRESENCA_MM) {
      Serial.println("Presenca detectada. Ativando GlowUp Plant.");
      inicializarSistemaAtivo();
      return;
    }
  } else {
    Serial.println("Sem leitura valida de distancia.");
  }

  Serial.println("Nenhuma pessoa proxima. Voltando ao deep sleep.");
  entrarEmDeepSleep();
}

// ==========================================
// LOOP
// ==========================================
void loop() {
  roboEyes.update();

  // -----------------------------------------------------------------
  // BLOCO 0: VERIFICA PRESENÇA
  // Enquanto houver pessoa perto, sistema continua ativo.
  // Se afastar por mais de 30 segundos, dorme.
  // -----------------------------------------------------------------
  if (millis() - temporizadorDistancia > 500) {
    temporizadorDistancia = millis();

    int distancia = 0;

    if (medirDistancia(distancia)) {
      Serial.print("Distancia: ");
      Serial.print(distancia);
      Serial.println(" mm");

      if (distancia < LIMIAR_PRESENCA_MM) {
        tempoUltimaPresenca = millis();
      }

      if (distancia > LIMIAR_SAIDA_MM) {
        if (millis() - tempoUltimaPresenca > TEMPO_AFASTADO_PARA_DORMIR) {
          Serial.println("Pessoa afastada por mais de 30 segundos.");
          entrarEmDeepSleep();
        }
      }
    } else {
      Serial.println("Sem leitura valida de distancia.");

      if (millis() - tempoUltimaPresenca > TEMPO_AFASTADO_PARA_DORMIR) {
        Serial.println("Sem presenca confirmada por mais de 30 segundos.");
        entrarEmDeepSleep();
      }
    }
  }

  // -----------------------------------------------------------------
  // BLOCO 1: LEITURA DO SENSOR DE UMIDADE
  // -----------------------------------------------------------------
  if (millis() - temporizadorSensor > 2000) {
    temporizadorSensor = millis();
    atualizarSensorEEstado();
  }

  // -----------------------------------------------------------------
  // BLOCO 2: ALARMES SONOROS RÍTMICOS
  // -----------------------------------------------------------------
  atualizarBuzzer();

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
  // Envia uma vez ao ativar e depois a cada 1 hora,
  // enquanto houver pessoa perto e o sistema estiver ativo.
  // -----------------------------------------------------------------
  if (millis() - temporizadorGoogleSheets > intervaloGoogleSheets) {
    temporizadorGoogleSheets = millis();
    enviarDadosGoogleSheets();
  }
}