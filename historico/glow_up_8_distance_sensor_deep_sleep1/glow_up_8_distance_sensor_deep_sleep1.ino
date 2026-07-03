#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Adafruit_VL53L0X.h"

#include "esp_sleep.h"

// ==========================================
// CONFIGURAÇÕES DO HARDWARE
// ==========================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

#define SDA_PIN 8
#define SCL_PIN 9

// ==========================================
// CONFIGURAÇÕES DE DISTÂNCIA
// ==========================================
const int LIMIAR_PRESENCA_MM = 1000;  // 1 metro
const int LIMIAR_SAIDA_MM    = 1200;  // histerese

// Tempo dormindo entre uma medição e outra
const uint64_t TEMPO_DEEP_SLEEP_US = 1000000ULL; // 1 segundo

// Tempo sem presença antes de voltar a dormir
const unsigned long TEMPO_SEM_PRESENCA_PARA_DORMIR = 4000; // ms

// Janela de segurança após upload/reset manual
const unsigned long JANELA_UPLOAD_MS = 8000;

// ==========================================
// OBJETOS
// ==========================================
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_VL53L0X lox = Adafruit_VL53L0X();

// ==========================================
// VARIÁVEIS
// ==========================================
unsigned long tempoUltimaPresenca = 0;
unsigned long temporizadorLeitura = 0;

int ultimaDistanciaMostrada = -1;

// ==========================================
// TEXTO CENTRALIZADO
// ==========================================
void escreverCentralizado(String texto, int y, int tamanhoTexto) {
  int16_t x1, y1;
  uint16_t w, h;

  display.setTextSize(tamanhoTexto);
  display.getTextBounds(texto.c_str(), 0, y, &x1, &y1, &w, &h);

  int x = (SCREEN_WIDTH - w) / 2;
  display.setCursor(x, y);
  display.print(texto);
}

// ==========================================
// MENSAGEM SIMPLES NO OLED
// ==========================================
void mostrarMensagem(String linha1, String linha2) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  escreverCentralizado("GlowUp Plant", 0, 1);
  display.drawLine(0, 12, SCREEN_WIDTH, 12, SSD1306_WHITE);

  escreverCentralizado(linha1, 25, 1);
  escreverCentralizado(linha2, 40, 1);

  display.display();
}

// ==========================================
// MOSTRA DISTÂNCIA NO OLED
// ==========================================
void mostrarDistancia(int distancia_mm) {
  if (distancia_mm == ultimaDistanciaMostrada) {
    return;
  }

  ultimaDistanciaMostrada = distancia_mm;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  escreverCentralizado("GlowUp Plant", 0, 1);
  display.drawLine(0, 12, SCREEN_WIDTH, 12, SSD1306_WHITE);

  escreverCentralizado("Distancia", 17, 1);

  String valor = String(distancia_mm);
  escreverCentralizado(valor, 30, 3);

  escreverCentralizado("mm", 56, 1);

  display.display();
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
  Serial.println("Sem presenca. Entrando em deep sleep por timer...");
  Serial.println("Vou acordar em 1 segundo para medir novamente.");
  Serial.flush();

  display.clearDisplay();
  display.display();
  display.ssd1306_command(SSD1306_DISPLAYOFF);

  esp_sleep_enable_timer_wakeup(TEMPO_DEEP_SLEEP_US);

  delay(100);
  esp_deep_sleep_start();
}

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("===== GlowUp Plant - Deep Sleep por Timer =====");

  esp_sleep_wakeup_cause_t motivo = esp_sleep_get_wakeup_cause();

  if (motivo == ESP_SLEEP_WAKEUP_TIMER) {
    Serial.println("Acordou pelo timer.");
  } else {
    Serial.println("Inicializacao normal, reset ou upload.");
    Serial.println("Janela de seguranca para novo upload...");
    delay(JANELA_UPLOAD_MS);
  }

  Wire.begin(SDA_PIN, SCL_PIN);

  // Inicializa OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Falha ao inicializar OLED.");
    for (;;);
  }

  display.ssd1306_command(SSD1306_DISPLAYON);
  mostrarMensagem("Iniciando", "VL53L0X");

  // Inicializa VL53L0X
  if (!lox.begin(0x29, false, &Wire, Adafruit_VL53L0X::VL53L0X_SENSE_HIGH_SPEED)) {
    Serial.println("Falha ao inicializar VL53L0X.");
    mostrarMensagem("Erro", "VL53L0X");
    for (;;);
  }

  Serial.println("VL53L0X inicializado.");

  int distancia = 0;

  if (medirDistancia(distancia)) {
    Serial.print("Distancia inicial: ");
    Serial.print(distancia);
    Serial.println(" mm");

    if (distancia < LIMIAR_PRESENCA_MM) {
      Serial.println("Presenca detectada. Mantendo acordado.");
      tempoUltimaPresenca = millis();
      mostrarDistancia(distancia);
      return;
    }
  } else {
    Serial.println("Sem leitura valida.");
  }

  Serial.println("Nenhuma presenca detectada.");
  entrarEmDeepSleep();
}

// ==========================================
// LOOP
// ==========================================
void loop() {
  if (millis() - temporizadorLeitura >= 200) {
    temporizadorLeitura = millis();

    int distancia = 0;

    if (medirDistancia(distancia)) {
      Serial.print("Distancia: ");
      Serial.print(distancia);
      Serial.println(" mm");

      mostrarDistancia(distancia);

      if (distancia < LIMIAR_PRESENCA_MM) {
        tempoUltimaPresenca = millis();
      }

      if (distancia > LIMIAR_SAIDA_MM) {
        if (millis() - tempoUltimaPresenca > TEMPO_SEM_PRESENCA_PARA_DORMIR) {
          entrarEmDeepSleep();
        }
      }
    } else {
      Serial.println("Sem leitura valida.");

      if (millis() - tempoUltimaPresenca > TEMPO_SEM_PRESENCA_PARA_DORMIR) {
        entrarEmDeepSleep();
      }
    }
  }
}