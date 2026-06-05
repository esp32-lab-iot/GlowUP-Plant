#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <FluxGarage_RoboEyes.h>
 
// Configurações do Display OLED I2C
#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64 
#define OLED_RESET -1    // Sem pino de reset físico no OLED
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
 
// Instância principal para controle dos olhos robóticos
RoboEyes<Adafruit_SSD1306> roboEyes(display); 

// Constantes de calibração do sensor de umidade (Valores para o ESP32-C3)
const int AirValue = 3800;   // Sensor totalmente seco no ar
const int WaterValue = 1326; // Sensor totalmente imerso na água
const int SensorPin = 1;     // Pino Analógico GPIO 1 do ESP32-C3

// Variáveis para armazenar as leituras de umidade
int soilMoistureValue = 0;
int soilmoisturepercent = 0;

// Temporizadores baseados em millis() para evitar o uso de delay()
unsigned long temporizadorSensor = 0;
unsigned long temporizadorAnimacaoExtra = 0;

// Controladores de estado da máquina de emoções
int estadoAtual = -1;
int ultimoEstado = -1; 
 
void setup() {
  // Inicializa o Monitor Serial a 115200 bps (Padrão para ESP32)
  Serial.begin(115200); 
  
  // Inicializa o barramento I2C nos pinos nativos do ESP32-C3 (SDA=8, SCL=9)
  Wire.begin(8, 9); 
  
  // Inicializa o display OLED no endereço padrão 0x3C
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Falha ao inicializar o SSD1306"));
    for(;;); // Trava o código aqui se o display não for encontrado
  }

  // Configuração inicial da biblioteca RoboEyes
  roboEyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, 100); // 100 FPS para máxima fluidez
  roboEyes.setAutoblinker(ON, 3, 2);                // Robô pisca sozinho a cada 3s (variação de 2s)
  roboEyes.setIdleMode(ON, 2, 2);                   // Robô olha aleatoriamente para os lados
  roboEyes.setBorderradius(6, 6);                   // Suaviza os cantos dos olhos (visual moderno)
  
  Serial.println("--- Sistema de Emoções da Planta Inicializado ---");
}
 
void loop() {
  // CRÍTICO: Atualiza os frames de animação dos olhos constantemente.
  // Esta função precisa rodar o mais rápido possível dentro do loop principal.
  roboEyes.update(); 

  // --- BLOCO 1: LEITURA DO SENSOR (Executado a cada 2 segundos) ---
  if (millis() - temporizadorSensor > 2000) {
    temporizadorSensor = millis(); // Reseta o temporizador do sensor

    // Realiza a leitura analógica e converte para porcentagem
    soilMoistureValue = analogRead(SensorPin);  
    soilmoisturepercent = map(soilMoistureValue, AirValue, WaterValue, 0, 100);
    
    // Filtro de segurança para manter a porcentagem rigorosamente entre 0 e 100
    if(soilmoisturepercent > 100) soilmoisturepercent = 100;
    if(soilmoisturepercent < 0)   soilmoisturepercent = 0;
    
    // Envia os dados para depuração no Monitor Serial
    Serial.print("Umidade: ");
    Serial.print(soilmoisturepercent);
    Serial.println("%");

    // Determina a faixa de estado com base na curva de felicidade da planta
    if (soilmoisturepercent <= 20) {
      estadoAtual = 0; // 0% a 20%: Crítico Seco (Morrendo)
    } else if (soilmoisturepercent <= 45) {
      estadoAtual = 1; // 20% a 45%: Sede (Reclamando)
    } else if (soilmoisturepercent <= 75) {
      estadoAtual = 2; // 45% a 75%: Oásis / Perfeito (Feliz)
    } else if (soilmoisturepercent <= 90) {
      estadoAtual = 3; // 75% a 90%: Excesso de água (Desconforto)
    } else {
      estadoAtual = 4; // 90% a 100%: Crítico Afogando (Pânico)
    }

    // --- BLOCO 2: MÁQUINA DE ESTADOS VISUAIS (Atualiza apenas se a faixa mudar) ---
    if (estadoAtual != ultimoEstado) {
      ultimoEstado = estadoAtual; // Atualiza o histórico de estados

      // Desliga todos os efeitos especiais antigos para evitar sobreposição de desenhos
      roboEyes.setHFlicker(OFF);
      roboEyes.setVFlicker(OFF);
      roboEyes.setSweat(OFF);
      roboEyes.setCuriosity(OFF);
      roboEyes.setPosition(DEFAULT);

      // Aplica a nova configuração de humor e efeitos fixos
      switch (estadoAtual) {
        case 0: // Crítico Seco
          Serial.println("-> Expressao: Morrendo de Sede");
          roboEyes.setMood(TIRED);      // Olhar cansado/caído
          roboEyes.setPosition(S);       // Olhando para baixo (desanimado)
          roboEyes.setVFlicker(ON, 2);   // Olhos tremendo verticalmente por fraqueza
          break;
          
        case 1: // Sede
          Serial.println("-> Expressao: Com Sede / Irritado");
          roboEyes.setMood(ANGRY);      // Olhar bravo exigindo água
          break;
          
        case 2: // Perfeito
          Serial.println("-> Expressao: Saudavel e Feliz");
          roboEyes.setMood(HAPPY);      // Olhar em formato de arco alegre
          break;
          
        case 3: // Excesso
          Serial.println("-> Expressao: Encharcado / Desconfortável");
          roboEyes.setMood(TIRED);      // Olhar incomodado
          roboEyes.setSweat(ON);        // Gotículas de suor começam a aparecer
          break;
          
        case 4: // Crítico Afogando
          Serial.println("-> Expressao: Afogando / Em Panico");
          roboEyes.setMood(ANGRY);      // Olhar fixo de desespero
          roboEyes.setSweat(ON);        // Suor ativo
          roboEyes.setVFlicker(ON, 4);   // Tremedeira vertical intensa simulando pânico
          break;
      }
    }
  }

  // --- BLOCO 3: COMPORTAMENTO DINÂMICO (Surtos de animação a cada 8 segundos) ---
  if (millis() - temporizadorAnimacaoExtra > 8000) {
    temporizadorAnimacaoExtra = millis(); // Reseta o temporizador de ações chamativas

    // Se a planta estiver saudável (Estado 2), ela dá uma risada chamativa
    if (estadoAtual == 2) {
      roboEyes.anim_laugh(); // Faz as pupilas chacoalharem rapidamente para cima e para baixo
    } 
    // Se estiver em estado de alerta por sede (1) ou afogamento (4), ela demonstra confusão/pane
    else if (estadoAtual == 1 || estadoAtual == 4) {
      roboEyes.anim_confused(); // Faz os olhos chacoalharem para as laterais, chamando atenção do usuário
    }
  }
}