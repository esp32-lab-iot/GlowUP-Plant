#include <Wire.h>
#include <WiFi.h> // Biblioteca nativa de Wi-Fi do ESP32
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <FluxGarage_RoboEyes.h>
#include "secrets.h" // Inclui o arquivo separado com as senhas

// ==========================================
// CONFIGURAÇÕES DO HARDWARE (PINOS E TELA)
// ==========================================
#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64 
#define OLED_RESET -1    
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
 
RoboEyes<Adafruit_SSD1306> roboEyes(display); 

const int SensorPin = 1;     // Pino Analógico do Sensor de Umidade (GPIO 1)
#define BUZZER_PIN 4        // Pino do Buzzer Ativo (GPIO 4)

const int AirValue = 3800;   
const int WaterValue = 1326; 

// ==========================================
// CONTROLE DE MODO E PARÂMETROS GLOBAIS
// ==========================================
bool modoSilencioso = false; // true para mudo, false para com som

int soilMoistureValue = 0;
int soilmoisturepercent = 0;

// Temporizadores baseados em millis()
unsigned long temporizadorSensor = 0;
unsigned long temporizadorAnimacaoExtra = 0;
unsigned long temporizadorBuzzer = 0;

// Controladores de estado da máquina de emoções
int estadoAtual = -1;
int ultimoEstado = -1; 
bool estadoBuzzer = false; 

void setup() {
  Serial.begin(115200); 
  
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  
  Wire.begin(8, 9); 
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Falha ao inicializar o SSD1306"));
    for(;;); 
  }

  // Configuração inicial do RoboEyes
  roboEyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, 100); 
  roboEyes.setAutoblinker(ON, 3, 2);                
  roboEyes.setIdleMode(ON, 2, 2);                   
  roboEyes.setBorderradius(6, 6);                   
  
  // -----------------------------------------------------------------
  // CONEXÃO WI-FI (Com animação nos olhos)
  // -----------------------------------------------------------------
  Serial.print("Conectando à rede: ");
  Serial.println(WIFI_SSID);
  
  // Define uma expressão de "expectativa/espera" enquanto conecta
  roboEyes.setMood(DEFAULT);
  
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  unsigned long temporizadorSerialWifi = millis();
  
  // Enquanto não conectar, mantém os olhos ativos e piscando
  while (WiFi.status() != WL_CONNECTED) {
    roboEyes.update(); // CRÍTICO: Mantém a animação fluida durante a espera
    
    // Printa um ponto no monitor serial a cada 500ms sem travar a tela
    if (millis() - temporizadorSerialWifi > 500) {
      temporizadorSerialWifi = millis();
      Serial.print(".");
    }
  }
  
  // Sucesso na conexão!
  Serial.println("\nWi-Fi Conectado com sucesso!");
  Serial.print("IP obtido: ");
  Serial.println(WiFi.localIP());
  
  // Comemoração visual e sonora de conexão estabelecida
  roboEyes.setMood(HAPPY);
  if (!modoSilencioso) {
    digitalWrite(BUZZER_PIN, HIGH); delay(40);
    digitalWrite(BUZZER_PIN, LOW);  delay(40);
    digitalWrite(BUZZER_PIN, HIGH); delay(100);
    digitalWrite(BUZZER_PIN, LOW);
  }
  
  Serial.println("--- Planta Inteligente Online e Inicializada ---");
}
 
void loop() {
  // Atualiza constantemente as animações dos olhos
  roboEyes.update(); 

  // -----------------------------------------------------------------
  // BLOCO 1: LEITURA DO SENSOR E MAPEAMENTO (A cada 2 segundos)
  // -----------------------------------------------------------------
  if (millis() - temporizadorSensor > 2000) {
    temporizadorSensor = millis(); 

    soilMoistureValue = analogRead(SensorPin);  
    soilmoisturepercent = map(soilMoistureValue, AirValue, WaterValue, 0, 100);
    
    if(soilmoisturepercent > 100) soilmoisturepercent = 100;
    if(soilmoisturepercent < 0)   soilmoisturepercent = 0;
    
    Serial.print("Umidade Solo: ");
    Serial.print(soilmoisturepercent);
    Serial.println("%");

    if (soilmoisturepercent <= 20)      estadoAtual = 0; // Crítico Seco
    else if (soilmoisturepercent <= 45) estadoAtual = 1; // Sede
    else if (soilmoisturepercent <= 75) estadoAtual = 2; // Perfeito (Oásis)
    else if (soilmoisturepercent <= 90) estadoAtual = 3; // Excesso
    else                                estadoAtual = 4; // Crítico Afogando

    // -----------------------------------------------------------------
    // BLOCO 2: MÁQUINA DE ESTADOS VISUAIS E SOM PONTUAL
    // -----------------------------------------------------------------
    if (estadoAtual != ultimoEstado) {
      ultimoEstado = estadoAtual; 

      roboEyes.setHFlicker(OFF);
      roboEyes.setVFlicker(OFF);
      roboEyes.setSweat(OFF);
      roboEyes.setCuriosity(OFF);
      roboEyes.setPosition(DEFAULT);

      switch (estadoAtual) {
        case 0: // Crítico Seco
          Serial.println("-> Estado: Morrendo de Sede!");
          roboEyes.setMood(TIRED);      
          roboEyes.setPosition(S);       
          roboEyes.setVFlicker(ON, 2);   
          break;
          
        case 1: // Sede
          Serial.println("-> Estado: Com Sede / Irritado");
          roboEyes.setMood(ANGRY);      
          break;
          
        case 2: // Perfeito (Oásis)
          Serial.println("-> Estado: Perfeito e Feliz!");
          roboEyes.setMood(HAPPY);      
          
          if (!modoSilencioso) {
            digitalWrite(BUZZER_PIN, HIGH); delay(15);
            digitalWrite(BUZZER_PIN, LOW);  delay(30);
            digitalWrite(BUZZER_PIN, HIGH); delay(15);
            digitalWrite(BUZZER_PIN, LOW); 
          }
          break;
          
        case 3: // Excesso
          Serial.println("-> Estado: Encharcado / Incomodado");
          roboEyes.setMood(TIRED);      
          roboEyes.setSweat(ON);        
          break;
          
        case 4: // Crítico Afogando
          Serial.println("-> Estado: Afogando em Pânico!");
          roboEyes.setMood(ANGRY);      
          roboEyes.setSweat(ON);        
          roboEyes.setVFlicker(ON, 4);   
          break;
      }
    }
  }

  // -----------------------------------------------------------------
  // BLOCO 3: ALARMES SONOROS RÍTMICOS (Respeita o Modo Silencioso)
  // -----------------------------------------------------------------
  if (!modoSilencioso && (estadoAtual == 0 || estadoAtual == 4)) {
    if (millis() - temporizadorBuzzer > 100) {
      temporizadorBuzzer = millis();
      estadoBuzzer = !estadoBuzzer; 
      digitalWrite(BUZZER_PIN, estadoBuzzer);
    }
  } 
  else if (!modoSilencioso && estadoAtual == 1) {
    unsigned long tempoCiclo = millis() % 3000; 
    if (tempoCiclo < 80) {
      digitalWrite(BUZZER_PIN, HIGH);
    } else {
      digitalWrite(BUZZER_PIN, LOW);
    }
  } 
  else {
    digitalWrite(BUZZER_PIN, LOW);
  }

  // -----------------------------------------------------------------
  // BLOCO 4: SURTOS DE ANIMAÇÃO DINÂMICA (A cada 8 segundos)
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
}