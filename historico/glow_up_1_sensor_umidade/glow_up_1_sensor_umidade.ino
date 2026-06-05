#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
 
#define SCREEN_WIDTH 128 // Largura do display OLED
#define SCREEN_HEIGHT 64 // Altura do display OLED
#define OLED_RESET -1    // Sem pino de reset no OLED I2C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
 
// ATENÇÃO: Veja o aviso sobre estes valores logo abaixo do código!
const int AirValue = 3800;   // Valor com o sensor totalmente seco no ESP32-C3
const int WaterValue = 1326; // Valor com o sensor dentro da água no ESP32-C3

const int SensorPin = 1;     // Pino GPIO 1 do ESP32-C3 (Pino Analógico ADC1_CH1)
int soilMoistureValue = 0;
int soilmoisturepercent = 0;
 
void setup() {
  Serial.begin(115200); 
  
  // Inicializa o I2C nos pinos 8 e 9 do ESP32-C3
  Wire.begin(8, 9); 
  
  // Inicializa o display OLED no endereço 0x3C
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Falha ao inicializar o SSD1306"));
    for(;;);
  }
  
  display.clearDisplay();
}
 
void loop() {
  // Lê o valor analógico do sensor
  soilMoistureValue = analogRead(SensorPin);  
  Serial.print("Valor Bruto ADC: ");
  Serial.println(soilMoistureValue);
  
  // Converte o valor bruto para porcentagem
  soilmoisturepercent = map(soilMoistureValue, AirValue, WaterValue, 0, 100);
  
  // Restringe a porcentagem entre 0% e 100% para evitar valores negativos ou acima de 100
  if(soilmoisturepercent > 100) soilmoisturepercent = 100;
  if(soilmoisturepercent < 0)   soilmoisturepercent = 0;
  
  // Imprime no Monitor Serial
  Serial.print("Umidade: ");
  Serial.print(soilmoisturepercent);
  Serial.println("%");
  
  // DESENHO NO DISPLAY OLED
  display.clearDisplay();
  
  // Título: "Soil"
  display.setCursor(45, 0);  
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE); // Garante compatibilidade com versões novas da biblioteca
  display.println("Solo");
  
  // Subtítulo: "Moisture"
  display.setCursor(20, 15);  
  display.println("Umidade");
  
  // Valor da Porcentagem
  display.setCursor(30, 40);  
  display.setTextSize(3);
  display.print(soilmoisturepercent);
  display.println(" %");
  
  // Envia tudo para a tela
  display.display();
 
  delay(250);
}