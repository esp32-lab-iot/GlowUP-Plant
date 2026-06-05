#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <FluxGarage_RoboEyes.h>
 
#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64 
#define OLED_RESET -1    
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
 
RoboEyes<Adafruit_SSD1306> roboEyes(display); // Instância dos olhos robóticos

// Mantemos os seus valores calibrados perfeitamente
const int AirValue = 3800;   
const int WaterValue = 1326; 
const int SensorPin = 1;     

int soilMoistureValue = 0;
int soilmoisturepercent = 0;

// Variáveis de controle de tempo e estado
unsigned long temporizadorSensor = 0;
int ultimoEstado = -1; 
 
void setup() {
  Serial.begin(115200); 
  Wire.begin(8, 9); 
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Falha ao inicializar o SSD1306"));
    for(;;);
  }

  // Inicializa a biblioteca de animação dos olhos
  roboEyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, 100);
  roboEyes.setAutoblinker(ON, 3, 2); // Deixa o robô piscando sozinho
  roboEyes.setIdleMode(ON, 2, 2);    // Olhos movem sutilmente sozinhos
  
  // Customização estética: cantos ligeiramente arredondados dão mais expressividade
  roboEyes.setBorderradius(6, 6);
  
  Serial.println("Planta Inteligente Inicializada!");
}
 
void loop() {
  // CRÍTICO: Atualiza constantemente os frames dos olhos. 
  // Deve rodar livremente o mais rápido possível.
  roboEyes.update(); 

  // Executa a leitura do sensor a cada 2000 milissegundos (2 segundos)
  if (millis() - temporizadorSensor > 2000) {
    temporizadorSensor = millis(); // Reseta o temporizador interno

    soilMoistureValue = analogRead(SensorPin);  
    soilmoisturepercent = map(soilMoistureValue, AirValue, WaterValue, 0, 100);
    
    // Restringe os limites
    if(soilmoisturepercent > 100) soilmoisturepercent = 100;
    if(soilmoisturepercent < 0)   soilmoisturepercent = 0;
    
    // Envia os dados atuais para depuração no Monitor Serial
    Serial.print("Umidade: ");
    Serial.print(soilmoisturepercent);
    Serial.println("%");

    // Identifica qual faixa de umidade a planta se encontra
    int estadoAtual;
    if (soilmoisturepercent <= 20) {
      estadoAtual = 0; // Bravo
    } else if (soilmoisturepercent <= 40) {
      estadoAtual = 1; // Curioso
    } else if (soilmoisturepercent <= 60) {
      estadoAtual = 2; // Feliz
    } else if (soilmoisturepercent <= 80) {
      estadoAtual = 3; // Mais feliz
    } else {
      estadoAtual = 4; // Suando
    }

    // Só altera a animação se a planta mudar de faixa de umidade
    // Isso evita reconfigurar os olhos desnecessariamente a cada 2 segundos
    if (estadoAtual != ultimoEstado) {
      ultimoEstado = estadoAtual;

      switch (estadoAtual) {
        case 0: // 0% a 20%: Bravo
          Serial.println("-> Expressao: Bravo");
          roboEyes.setMood(ANGRY);
          roboEyes.setCuriosity(OFF);
          roboEyes.setSweat(OFF);
          break;
          
        case 1: // 20% a 40%: Curioso
          Serial.println("-> Expressao: Curioso");
          roboEyes.setMood(DEFAULT);
          roboEyes.setCuriosity(ON); // Ativa o modo curioso da biblioteca
          roboEyes.setSweat(OFF);
          break;
          
        case 2: // 40% a 60%: Feliz
          Serial.println("-> Expressao: Feliz");
          roboEyes.setMood(HAPPY);
          roboEyes.setCuriosity(OFF);
          roboEyes.setSweat(OFF);
          break;
          
        case 3: // 60% a 80%: Mais feliz
          Serial.println("-> Expressao: Mais Feliz");
          roboEyes.setMood(HAPPY);
          roboEyes.setCuriosity(OFF);
          roboEyes.setSweat(OFF);
          roboEyes.anim_laugh(); // Executa uma animação extra de risada imediata ao entrar nesta faixa
          break;
          
        case 4: // 80% a 100%: Suando (Muita água no solo)
          Serial.println("-> Expressao: Suando");
          roboEyes.setMood(TIRED);
          roboEyes.setSweat(ON); // Ativa as gotas de suor na tela
          roboEyes.setCuriosity(OFF);
          break;
      }
    }
  }
}