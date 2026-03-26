/**
 * =============================================================================
 * SOUNDFENDER - DATA LOGGER AMBIENTAL PARA INSTRUMENTOS MUSICAIS
 * =============================================================================
 *
 * Descrição:
 *   Sistema embarcado de monitoramento ambiental desenvolvido para proteger
 *   instrumentos musicais em espaços de armazenamento. Monitora temperatura,
 *   umidade relativa do ar e luminosidade, emitindo alertas visuais e sonoros
 *   quando os parâmetros saem das faixas configuradas. Os eventos de alerta
 *   são registrados com timestamp na memória EEPROM interna do microcontrolador.
 *
 * Microcontrolador: ATmega328P (Arduino Uno R3)
 *
 * Pinagem:
 *   - Pino 2  (Digital): DHT11 - Sensor de temperatura e umidade
 *   - Pino 3  (PWM):     LED RGB - Canal Vermelho
 *   - Pino 4  (Digital): LED RGB - Canal Verde
 *   - Pino 5  (PWM):     LED RGB - Canal Azul
 *   - Pinos 6-9 (Digital): Teclado matricial - linhas
 *   - Pinos 10-13 (Digital): Teclado matricial - colunas
 *   - Pino A0 (Analógico): Buzzer piezoelétrico
 *   - Pino A1 (Analógico): LDR - Sensor de luminosidade
 *   - I2C (SDA/SCL): LCD 20x4 (endereço 0x27) e módulo RTC DS1307
 *
 * Limites padrão:
 *   - Temperatura: 15°C a 25°C
 *   - Umidade:     30% a 50% UR
 *   - Luminosidade: 0% a 30%
 *
 * Armazenamento: 2 registros de log na EEPROM (posições 0 e sizeof(Log))
 *
 * Versão:    1.0
 * =============================================================================
 */

// ─── BIBLIOTECAS ──────────────────────────────────────────────────────────────

#include <Wire.h>              // Comunicação I2C (LCD e RTC)
#include <LiquidCrystal_I2C.h> // Controle do display LCD via I2C
#include <DHT.h>               // Leitura do sensor DHT11
#include <RTClib.h>            // Leitura do relógio de tempo real DS1307
#include <Keypad.h>            // Leitura do teclado matricial 4x4
#include <EEPROM.h>            // Acesso à memória EEPROM interna do ATmega328P

// ─── INSTÂNCIA DO LCD ─────────────────────────────────────────────────────────

// LCD 20 colunas x 4 linhas, endereço I2C 0x27
LiquidCrystal_I2C lcd(0x27, 20, 4);

// ─── DEFINIÇÃO DE PINOS ───────────────────────────────────────────────────────

#define BUZZER  A0   // Saída do buzzer piezoelétrico
#define RGB_R   3    // Canal vermelho do LED RGB (PWM)
#define RGB_G   4    // Canal verde do LED RGB
#define RGB_B   5    // Canal azul do LED RGB (PWM)
#define DHTPIN  2    // Pino de dados do sensor DHT
#define DHTTYPE DHT11 // Tipo do sensor 
#define LDRPIN  A1   // Entrada analógica do divisor de tensão com LDR

// ─── FREQUÊNCIAS DE NOTAS MUSICAIS (Hz) ───────────────────────────────────────

// Usadas na melodia de abertura (tema inspirado em "Ode à Alegria")
#define NOTE_RE  294
#define NOTE_MI  330
#define NOTE_FA  370
#define NOTE_SOL 392
#define NOTE_LA  440

// ─── INSTÂNCIAS DOS SENSORES E RTC ───────────────────────────────────────────

DHT dht(DHTPIN, DHTTYPE);   // Sensor de temperatura e umidade
RTC_DS1307 rtc;              // Módulo de relógio de tempo real

// ─── ESTRUTURA DE LOG ────────────────────────────────────────────────────────

/**
 * Representa um registro de evento de alerta salvo na EEPROM.
 *
 * Campos:
 *   valor   - Valor medido no momento do alerta (float, 4 bytes)
 *   dia     - Dia do mês (1–31)
 *   mes     - Mês do ano (1–12)
 *   hora    - Hora (0–23)
 *   minuto  - Minuto (0–59)
 *   tipo    - Tipo do sensor: 1=Temperatura, 2=Umidade, 3=Luminosidade
 *
 * Tamanho total: ~9 bytes por registro. EEPROM armazena 2 registros:
 *   - Posição 0:            Log mais recente
 *   - Posição sizeof(Log):  Log anterior
 */
struct Log {
  float valor;
  byte dia, mes, hora, minuto;
  byte tipo;
};

// ─── TECLADO MATRICIAL 4x4 ────────────────────────────────────────────────────

const byte ROWS = 4;
const byte COLS = 4;

// Mapa de teclas: linhas x colunas
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {6, 7, 8, 9};     // Pinos das linhas do teclado
byte colPins[COLS] = {10, 11, 12, 13}; // Pinos das colunas do teclado

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ─── VARIÁVEIS DE ESTADO DO SISTEMA ──────────────────────────────────────────

bool sistemaLigado = false;   // true = sistema ativo (após pressionar '*')
int estadoAtual = 0;          // Tela/estado atual da máquina de estados
int cursorMenu = 0;           // Seleção ativa no menu (0=Temp, 1=Umid, 2=Luz, 3=Hora)
bool precisaDesenhar = true;  // Flag: indica que a tela precisa ser redesenhada
bool blinkState = false;      // Estado do pisca-pisca para alertas visuais
unsigned long lastBlink = 0;          // Timestamp do último toggle do pisca
unsigned long lastDisplayUpdate = 0;  // Timestamp da última atualização do display
unsigned long lastSensorRead = 0;     // Timestamp da última leitura dos sensores

// ─── VARIÁVEIS DE LEITURA DOS SENSORES ────────────────────────────────────────

float tempAtual;    // Temperatura na unidade ativa (°C ou °F)
float tempRaw;      // Temperatura em °C (sempre em Celsius internamente)
float humAtual;     // Umidade relativa (%)
float lumAtual;     // Luminosidade mapeada (0–100%)

// ─── LIMITES CONFIGURÁVEIS DE ALERTA ─────────────────────────────────────────

// Estes valores são ajustáveis pelo usuário via menu de configuração.
// Os limites de temperatura seguem a unidade ativa (°C ou °F).
float tempMax = 30.0, tempMin = 20.0; // Faixa segura de temperatura
float humMax  = 70.0, humMin  = 30.0; // Faixa segura de umidade
float lumMax  = 50.0, lumMin  = 20.0; // Faixa segura de luminosidade

// ─── STATUS DOS SENSORES ──────────────────────────────────────────────────────

// 0 = Normal | 1 = Atenção (próximo ao limite) | 2 = Alerta (fora da faixa)
int statusTemp = 0, statusHum = 0, statusLum = 0;
int ultStatusT = 0, ultStatusH = 0, ultStatusL = 0; // Status anterior (para detectar transição)

// ─── FLAGS DE CONFIGURAÇÃO ────────────────────────────────────────────────────

bool muted = false;          // true = buzzer silenciado
bool useFahrenheit = false;  // true = exibir temperatura em °F

// ─── VARIÁVEIS DE ENTRADA DE CONFIGURAÇÃO ─────────────────────────────────────

int paramSelecionado = 0;     // Parâmetro sendo configurado (1=Temp, 2=Umid, 3=Luz)
int limiteSelecionado = 0;    // Limite sendo editado (1=Máximo, 2=Mínimo)
char inputBuffer[5] = "";     // Buffer de entrada numérica (até 4 dígitos + '\0')
byte inputIndex = 0;          // Posição atual no buffer de entrada
int ultProgressoGlob = -1;    // Reservado para animações (não usado ativamente)

// ─── CACHE DE EXIBIÇÃO ────────────────────────────────────────────────────────

// Armazena o último valor impresso no LCD para evitar reescritas desnecessárias
float ultTempImpressa = -999.0;
float ultHumImpressa  = -999.0;
float ultLumImpressa  = -999.0;

// ─── CARACTERES CUSTOMIZADOS (bitmaps 5x8) ────────────────────────────────────

// Ícone de alerta (triângulo com exclamação) — carregado no slot 4
byte alerta[8] = {0b00100,0b01010,0b01110,0b10101,0b10101,0b10001,0b10101,0b11111};

// Ícone de termômetro (4 quadrantes: TopLeft, TopRight, BottomLeft, BottomRight)
byte termTL[8] = {0b00000,0b00001,0b00010,0b00010,0b00010,0b00010,0b00011,0b00011};
byte termTR[8] = {0b00000,0b10000,0b01000,0b01000,0b01000,0b01000,0b11000,0b11000};
byte termBL[8] = {0b00011,0b00111,0b01110,0b01101,0b01111,0b01111,0b00111,0b00011};
byte termBR[8] = {0b11000,0b11100,0b11110,0b11110,0b11010,0b10110,0b11100,0b11000};

// Ícone de gota d'água / umidade (4 quadrantes)
byte umidTL[8] = {0b00001,0b00011,0b00010,0b00110,0b00100,0b01101,0b01001,0b11011};
byte umidTR[8] = {0b10000,0b11000,0b01000,0b01100,0b00100,0b10110,0b10010,0b11011};
byte umidBL[8] = {0b10111,0b10101,0b10110,0b10011,0b01011,0b01000,0b00110,0b00001};
byte umidBR[8] = {0b11101,0b11101,0b11101,0b11001,0b11010,0b00010,0b01100,0b10000};

// Ícone de sol / luminosidade (4 quadrantes)
byte luzTL[8] = {0b00000,0b00001,0b00101,0b01001,0b01001,0b01001,0b00101,0b00101};
byte luzTR[8] = {0b00000,0b10000,0b10100,0b10010,0b10010,0b10010,0b10100,0b10100};
byte luzBL[8] = {0b00111,0b00011,0b00111,0b00011,0b00111,0b00011,0b00001,0b00000};
byte luzBR[8] = {0b11100,0b11000,0b11100,0b11000,0b11100,0b11000,0b10000,0b00000};

// Ícone de relógio / data-hora (4 quadrantes)
byte tempoTL[8] = {0b11111,0b01111,0b00100,0b01000,0b01011,0b00101,0b00010,0b00001};
byte tempoTR[8] = {0b11111,0b11110,0b00100,0b00010,0b11010,0b10100,0b01000,0b10000};
byte tempoBL[8] = {0b00001,0b00010,0b00101,0b01011,0b01011,0b00100,0b01111,0b11111};
byte tempoBR[8] = {0b10000,0b01000,0b10100,0b11010,0b11010,0b00100,0b11110,0b11111};

// Segmentos da barra de progresso horizontal (esquerda, meio, direita — cheios e vazios)
byte barLeftEmpty[8]  = {0b00111,0b01000,0b10000,0b10000,0b10000,0b01000,0b00111,0b00000};
byte barLeftFilled[8] = {0b00111,0b01000,0b10011,0b10111,0b10011,0b01000,0b00111,0b00000};
byte barMidEmpty[8]   = {0b11111,0b00000,0b00000,0b00000,0b00000,0b00000,0b11111,0b00000};
byte barMidFilled[8]  = {0b11111,0b00000,0b11111,0b11111,0b11111,0b00000,0b11111,0b00000};
byte barRightEmpty[8] = {0b11100,0b00010,0b00001,0b00001,0b00001,0b00010,0b11100,0b00000};
byte barRightFilled[8]= {0b11100,0b00010,0b11001,0b11101,0b11001,0b00010,0b11100,0b00000};

// ─── CARACTERES DA ANIMAÇÃO DE ABERTURA ───────────────────────────────────────

// Clave de sol (parte superior e inferior) — logo do sistema
byte clavesolcima[8]  = {0b00010,0b00101,0b00101,0b01110,0b01100,0b10110,0b11101,0b10101};
byte clavesolbaixo[8] = {0b01110,0b00100,0b00100,0b01100,0b01100,0b00000,0b00000,0b00000};

// Clave de fá (decoração do logo)
byte clavefa[8]  = {0b01110,0b10001,0b11101,0b01101,0b00001,0b00011,0b00110,0b01100};
byte facome[8]   = {0b00000,0b00000,0b10010,0b00101,0b10111,0b00100,0b00011,0b00000};

// Escudo esquerdo/direito (bordas do logo)
byte escudoesq[8] = {0b00001,0b01011,0b01111,0b01111,0b01101,0b00101,0b00011,0b00001};
byte escudodir[8] = {0b10000,0b11010,0b01110,0b00110,0b01110,0b01100,0b11000,0b10000};

// Nota musical e estrela (elementos decorativos da animação)
byte nota[8]   = {0b00100,0b00110,0b00101,0b00100,0b00100,0b01100,0b01100,0b00000};
byte estrela[8]= {0b00100,0b00100,0b10101,0b01110,0b10101,0b00100,0b00100,0b00000};

// Segmentos do pentagrama musical (linhas da pauta)
byte pentacima[8]     = {0b00000,0b11111,0b00000,0b00000,0b11111,0b00000,0b00000,0b11111};
byte pentabaixo[8]    = {0b00000,0b00000,0b11111,0b00000,0b00000,0b11111,0b00000,0b00000};
byte pentacimafim[8]  = {0b00000,0b11110,0b00110,0b00110,0b11110,0b00110,0b00110,0b11110};
byte pentabaixofim[8] = {0b00110,0b00110,0b11110,0b00110,0b00110,0b11110,0b00000,0b00000};

// ─── MELODIA DE ABERTURA ("ODE À ALEGRIA" SIMPLIFICADA) ─────────────────────

// Sequência de notas e durações correspondentes em milissegundos
int melodia[] = {NOTE_FA, NOTE_FA, NOTE_SOL, NOTE_LA, NOTE_LA, NOTE_SOL, NOTE_FA, NOTE_MI, NOTE_RE, NOTE_RE, NOTE_MI, NOTE_FA, NOTE_MI, NOTE_RE, NOTE_RE};
int duracoes[] = {600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 500, 200, 800};

// =============================================================================
// FUNÇÕES UTILITÁRIAS
// =============================================================================

/**
 * Efeito de saída de tela: preenche todas as células do LCD com o caractere
 * de bloco sólido (0xFF) linha a linha, depois limpa o display.
 * Produz transição visual de "apagamento" entre telas.
 */
void efeitoSaida() {
  for (int i = 0; i < 4; i++) {
    lcd.setCursor(0, i);
    for (int j = 0; j < 20; j++) {
      lcd.write(255); // Caractere de bloco sólido nativo do LCD
    }
    delay(40);
  }
  delay(50);
  lcd.clear();
}

/**
 * Efeito de entrada de tela: pisca o backlight 3 vezes rapidamente,
 * sinalizando que uma nova tela está sendo carregada.
 */
void efeitoEntrada() {
  for(int i=0; i<3; i++) {
    lcd.noBacklight(); delay(30);
    lcd.backlight();   delay(30);
  }
}

/**
 * Converte temperatura de Celsius para a unidade ativa.
 * Se useFahrenheit == true, aplica a fórmula °F = (°C × 9/5) + 32.
 *
 * @param celsius  Temperatura em graus Celsius
 * @return         Temperatura na unidade ativa (°C ou °F)
 */
float toActiveUnit(float celsius) {
  return useFahrenheit ? (celsius * 9.0 / 5.0 + 32.0) : celsius;
}

/**
 * Define a cor do LED RGB usando valores PWM independentes por canal.
 * O LED é conectado ao catodo comum, portanto valores maiores = mais brilhante.
 *
 * @param r  Intensidade do canal vermelho (0–255)
 * @param g  Intensidade do canal verde (0–255)
 * @param b  Intensidade do canal azul (0–255)
 */
void setRGB(byte r, byte g, byte b) {
  analogWrite(RGB_R, r);
  analogWrite(RGB_G, g);
  analogWrite(RGB_B, b);
}

// =============================================================================
// SETUP
// =============================================================================

/**
 * Inicialização do sistema.
 * Executado uma única vez ao ligar o equipamento.
 *
 * Sequência:
 *   1. Inicializa o LCD (I2C)
 *   2. Carrega o caractere de alerta no slot 4 da CGRAM
 *   3. Inicializa o sensor DHT
 *   4. Inicializa o RTC DS1307
 *   5. Configura os pinos do buzzer e LED RGB como saída
 *   6. Desliga telas (aguarda o operador pressionar '*' para ligar)
 */
void setup() {
  lcd.init();
  lcd.createChar(4, alerta); // Pré-carrega o ícone de alerta na CGRAM
  dht.begin();
  rtc.begin();
  pinMode(BUZZER, OUTPUT);
  pinMode(RGB_R, OUTPUT);
  pinMode(RGB_G, OUTPUT);
  pinMode(RGB_B, OUTPUT);
  desligarTelas(); // Inicia com display apagado e sistema desligado
}

// =============================================================================
// GERENCIAMENTO DE LOGS NA EEPROM
// =============================================================================

/**
 * Salva um novo registro de evento de alerta na EEPROM usando esquema FIFO
 * de 2 posições (shift register manual).
 *
 * Lógica:
 *   - O log atual (posição 0) é movido para a posição 1 (sizeof(Log))
 *   - O novo log é escrito na posição 0
 *
 * Ignora valores inválidos (NaN) provenientes de leitura falha do sensor.
 *
 * @param v  Valor medido a ser registrado
 * @param t  Tipo do sensor: 1=Temperatura, 2=Umidade, 3=Luminosidade
 */
void salvarLog(float v, byte t) {
  if (isnan(v)) return; // Descarta leituras inválidas do sensor
  DateTime agora = rtc.now();
  Log novo = {v, (byte)agora.day(), (byte)agora.month(), (byte)agora.hour(), (byte)agora.minute(), t};
  Log antigo;
  EEPROM.get(0, antigo);              // Lê o log mais recente
  EEPROM.put(sizeof(Log), antigo);    // Desloca para a posição 1
  EEPROM.put(0, novo);               // Grava o novo log na posição 0
}

// =============================================================================
// BARRA DE PROGRESSO (LINHA 3 DO LCD)
// =============================================================================

/**
 * Renderiza uma barra de progresso horizontal de 20 caracteres na linha 3
 * do LCD, representando o valor atual em relação ao intervalo [vmin, vmax].
 *
 * Comportamento:
 *   - Abaixo de vmin: barra completamente vazia
 *   - Entre vmin e vmax: preenchimento proporcional (18 segmentos intermediários)
 *   - Acima ou igual a vmax: barra completamente preenchida
 *   - Nos extremos (fora da faixa): carrega o caractere de alerta no slot 7
 *     da CGRAM (para uso posterior na tela de dados)
 *
 * Nota: Esta função sobrescreve os slots 4–7 da CGRAM. O slot 4 (alerta) é
 * restaurado ao retornar à tela de menu.
 *
 * @param valor  Valor atual do parâmetro monitorado
 * @param vmin   Limite mínimo configurado
 * @param vmax   Limite máximo configurado
 */
void desenharBarraTotal(float valor, float vmin, float vmax) {
  if(valor > vmin) {
    lcd.createChar(4, barLeftFilled);
    lcd.createChar(5, barMidFilled);
    lcd.setCursor(0, 3);
    lcd.write(4);

    if(valor < vmax) {
      // Preenchimento parcial: calcula quantos segmentos intermediários ficam cheios
      lcd.createChar(6, barMidEmpty);
      lcd.createChar(7, barRightEmpty);
      int qtdfilled = ((valor - vmin) / (vmax - vmin) * 18);
      for (int i = 1; i <= 18; i++) {
        if(i > qtdfilled) { lcd.setCursor(i, 3); lcd.write(6); }
      }
      for (int i = 1; i <= qtdfilled; i++) {
        lcd.setCursor(i, 3); lcd.write(5);
      }
      lcd.setCursor(19, 3); lcd.write(7);
    } else {
      // Barra cheia (valor >= vmax)
      lcd.createChar(6, barRightFilled);
      for (int i = 1; i < 19; i++) { lcd.setCursor(i, 3); lcd.write(5); }
      lcd.setCursor(19, 3); lcd.write(6);
    }
  } else {
    // Barra vazia (valor <= vmin)
    lcd.createChar(4, barMidEmpty);
    lcd.createChar(5, barLeftEmpty);
    lcd.createChar(6, barRightEmpty);
    lcd.setCursor(0, 3);  lcd.write(5);
    for (int i = 1; i < 19; i++) { lcd.setCursor(i, 3); lcd.write(4); }
    lcd.setCursor(19, 3); lcd.write(6);
  }

  // Nos extremos, prepara o ícone de alerta no slot 7 para uso pelas telas de dados
  if(valor <= vmin || valor >= vmax) {
    lcd.createChar(7, alerta);
  }
}

// =============================================================================
// LEITURA E MONITORAMENTO DE SENSORES
// =============================================================================

/**
 * Lê os sensores a cada 2 segundos, calcula os status de alerta e controla
 * o LED RGB e o buzzer de acordo com o estado global.
 *
 * Lógica de status por sensor:
 *   - Status 0 (Normal):    valor dentro da faixa segura
 *   - Status 1 (Atenção):   valor dentro da faixa, mas próximo ao limite
 *                           (Temp ±2°, Umid/Lum ±5%)
 *   - Status 2 (Alerta):    valor fora da faixa configurada
 *
 * Transição para Alerta (status 2):
 *   Um log é salvo na EEPROM apenas na transição de "não-alerta" para "alerta",
 *   evitando gravações repetidas enquanto a condição persiste.
 *
 * LED RGB:
 *   - Verde (0,255,0):   Status global = 0 (tudo normal)
 *   - Amarelo (255,255,0): Status global = 1 (atenção)
 *   - Vermelho piscante (255,0,0) / Apagado: Status global = 2 (alerta)
 *
 * Buzzer:
 *   - Emite bip de 1800 Hz por 100ms a cada ciclo de pisca-pisca (400ms)
 *     quando status global = 2 e som não está mudo.
 */
void monitorarSensores() {
  // ── Leitura a cada 2 segundos ──
  if (millis() - lastSensorRead >= 2000) {
    float tRead = dht.readTemperature();
    float hRead = dht.readHumidity();

    // Aceita a leitura apenas se for válida (não NaN)
    if (!isnan(tRead)) tempRaw = tRead;
    if (!isnan(hRead)) humAtual = hRead;

    tempAtual = toActiveUnit(tempRaw); // Converte para a unidade ativa

    // Luminosidade: leitura analógica mapeada de 0–1023 para 0–100%
    int ldrRaw = analogRead(LDRPIN);
    lumAtual = map(ldrRaw, 0, 1023, 0, 100);

    // ── Cálculo de status ──
    statusTemp = (tempAtual > tempMax || tempAtual < tempMin) ? 2
               : ((tempAtual >= tempMax - 2 || tempAtual <= tempMin + 2) ? 1 : 0);

    statusHum  = (humAtual > humMax || humAtual < humMin) ? 2
               : ((humAtual >= humMax - 5 || humAtual <= humMin + 5) ? 1 : 0);

    statusLum  = (lumAtual > lumMax || lumAtual < lumMin) ? 2
               : ((lumAtual >= lumMax - 5 || lumAtual <= lumMin + 5) ? 1 : 0);

    // ── Log de eventos: salva apenas na transição para alerta ──
    if (statusTemp == 2 && ultStatusT != 2) salvarLog(tempRaw, 1);
    if (statusHum  == 2 && ultStatusH != 2) salvarLog(humAtual, 2);
    if (statusLum  == 2 && ultStatusL != 2) salvarLog(lumAtual, 3);

    ultStatusT = statusTemp;
    ultStatusH = statusHum;
    ultStatusL = statusLum;

    lastSensorRead = millis();
  }

  // ── Toggle do pisca-pisca a cada 400ms ──
  if (millis() - lastBlink > 400) {
    blinkState = !blinkState;
    lastBlink = millis();
    // Buzzer de alerta: bipa no estado "aceso" do pisca-pisca
    if ((statusTemp == 2 || statusHum == 2 || statusLum == 2) && blinkState && !muted)
      tone(BUZZER, 1800, 100);
  }

  // ── Controle do LED RGB pelo status global ──
  int statusGlobal = max(statusTemp, max(statusHum, statusLum));

  if      (statusGlobal == 0)                    setRGB(0, 255, 0);   // Verde
  else if (statusGlobal == 1)                    setRGB(255, 255, 0); // Amarelo
  else if (statusGlobal == 2 && blinkState)      setRGB(255, 0, 0);   // Vermelho (aceso)
  else                                           setRGB(0, 0, 0);     // Apagado (piscando)
}

// =============================================================================
// LOOP PRINCIPAL
// =============================================================================

/**
 * Loop principal do sistema.
 *
 * Responsabilidades:
 *   1. Leitura da tecla pressionada no teclado matricial
 *   2. Toggle de liga/desliga com a tecla '*'
 *   3. Despacho para a função de tela correspondente ao estadoAtual
 *
 * Máquina de estados (estadoAtual):
 *   0 = Lobby (tela inicial)
 *   1 = Menu principal
 *   2 = Tela de dados (exibição de sensor ou data/hora)
 *   3 = Configurar parâmetro (seleção do parâmetro)
 *   4 = Configurar limite (Máximo ou Mínimo)
 *   5 = Configurar valor (entrada numérica)
 *   6 = Tela de logs (visualização da EEPROM)
 *   7 = Configurar sistema (unidade e som)
 */
void loop() {
  char key = keypad.getKey();

  // ── Liga/desliga via tecla '*' ──
  if (key == '*') {
    sistemaLigado = !sistemaLigado;
    if (sistemaLigado) {
      animacaoInicio();       // Exibe animação e melodia de abertura
      estadoAtual = 0;
      precisaDesenhar = true;
    } else {
      desligarTelas();        // Apaga LCD, LED e buzzer
    }
  }

  if (sistemaLigado) {
    monitorarSensores();

    // Bip de navegação ao pressionar qualquer tecla (exceto '*')
    if (key && key != '*' && !muted) tone(BUZZER, 2500, 15);

    // ── Despacho por estado ──
    if      (estadoAtual == 0) lobby(key);
    else if (estadoAtual == 1) menuPrincipal(key);
    else if (estadoAtual == 2) telaDados(key);
    else if (estadoAtual == 3) configParametro(key);
    else if (estadoAtual == 4) configLimite(key);
    else if (estadoAtual == 5) configValor(key);
    else if (estadoAtual == 6) telaLogs(key);
    else if (estadoAtual == 7) configSistema(key);
  }
}

// =============================================================================
// ANIMAÇÃO DE INICIALIZAÇÃO
// =============================================================================

/**
 * Aguarda por até `ms` milissegundos, retornando imediatamente se uma tecla
 * for pressionada. Usado para permitir que o usuário pule a animação.
 *
 * @param ms  Tempo máximo de espera em milissegundos
 * @return    true se uma tecla foi pressionada antes do timeout, false caso contrário
 */
bool esperaPulo(int ms) {
  unsigned long start = millis();
  while(millis() - start < ms) {
    if(keypad.getKey()) return true;
  }
  return false;
}

/**
 * Exibe a animação de abertura do SoundFender com pentagrama animado,
 * logo musical (clave de sol + "Sound" + clave de fá + "under") e
 * melodia de "Ode à Alegria" pelo buzzer.
 *
 * Etapas da animação:
 *   1. Pentagrama (linhas musicais) se expande da esquerda para a direita
 *      com notas aleatórias distribuídas pelas duas linhas centrais do LCD
 *   2. Clave de sol aparece na posição 6, e o logo "Sound" é exibido
 *   3. Elementos decorativos (clave de fá, escudos) completam o logo
 *   4. Loop infinito de estrelas/notas caindo verticalmente (aguarda tecla)
 *
 * O usuário pode pular qualquer etapa pressionando qualquer tecla.
 * Ao final, restaura o caractere de alerta no slot 4 da CGRAM.
 */
void animacaoInicio() {
  lcd.backlight();
  lcd.clear();

  // Carrega os bitmaps do pentagrama e da clave de sol
  lcd.createChar(0, pentacimafim);
  lcd.createChar(1, pentabaixofim);
  lcd.createChar(2, pentacima);
  lcd.createChar(3, pentabaixo);
  lcd.createChar(4, clavesolcima);
  lcd.createChar(5, clavesolbaixo);
  lcd.createChar(6, nota);

  // Posição aleatória das notas no pentagrama (0=sem nota, 1=linha superior, 2=linha inferior)
  int posNotas[20];
  for (int c = 0; c < 20; c++) {
    if (c == 6) posNotas[c] = 0;                    // Posição da clave: sem nota
    else if (random(0, 10) > 6) posNotas[c] = random(1, 3);
    else posNotas[c] = 0;
  }

  // ── Etapa 1 & 2: Animação de scroll do pentagrama ──
  for (int i = 0; i < 22; i++) {
    if (keypad.getKey()) goto fimAnimacao;
    int posClave = i - 7; // A clave começa a aparecer 7 frames após o início

    if (posClave < 6) {
      lcd.clear();
      int inicioDesenho = (posClave > 0) ? posClave : 0;
      for (int x = inicioDesenho; x <= i; x++) {
        if (x > 19) break;
        if (x == posClave && x >= 0) {
          // Desenha a clave de sol
          lcd.setCursor(x, 1); lcd.write(4);
          lcd.setCursor(x, 2); lcd.write(5);
        } else if (x == i) {
          // Desenha o caractere de fim de pauta
          lcd.setCursor(x, 1); lcd.write(0);
          lcd.setCursor(x, 2); lcd.write(1);
        } else {
          // Pentagrama com notas aleatórias
          lcd.setCursor(x, 1);
          if (posNotas[x] == 1) lcd.write(6); else lcd.write(2);
          lcd.setCursor(x, 2);
          if (posNotas[x] == 2) lcd.write(6); else lcd.write(3);
        }
      }
    }
    else if (posClave == 6) {
      lcd.clear();
      lcd.setCursor(6, 1); lcd.write(4);
      lcd.setCursor(6, 2); lcd.write(5);
    }

    // ── Etapa 3: Montagem do logo ──
    if (i == 13) {
      lcd.createChar(0, escudoesq);
      lcd.createChar(1, escudodir);
      lcd.createChar(2, clavefa);
      lcd.createChar(3, facome);
      lcd.setCursor(7, 1); lcd.print(F("ound"));   // "Sound" (o "S" foi substituído pelo ícone)
      lcd.setCursor(7, 2); lcd.write(2);
      lcd.setCursor(8, 2); lcd.write(3);
    }
    if (i == 14) {
      lcd.setCursor(11, 1); lcd.write(0);
      lcd.setCursor(12, 1); lcd.write(1);
      lcd.setCursor(9, 2); lcd.print(F("nder")); // "under" (completa "SoundFender" / "Sounder")
    }

    // Toca a melodia sincronizada com a animação (15 notas)
    if (i < 15) {
      if(!muted) tone(BUZZER, melodia[i], duracoes[i] * 0.9);
      if (esperaPulo(duracoes[i])) { noTone(BUZZER); goto fimAnimacao; }
      noTone(BUZZER);
    } else {
      if (esperaPulo(100)) goto fimAnimacao;
    }
  }

  // ── Etapa 4: Loop de estrelas/notas caindo ──
  {
    const int numColunas = 10;
    int colunas[] = {0, 2, 4, 5, 13, 14, 16, 17, 18, 19};
    int linhaAtual[numColunas];
    int tipoElemento[numColunas]; // 6=nota, 7=estrela

    // Limpa as bordas superior e inferior para o efeito de queda
    for (int c = 0; c < 20; c++) {
      lcd.setCursor(c, 0); lcd.print(" ");
      lcd.setCursor(c, 3); lcd.print(" ");
    }

    lcd.createChar(7, estrela);

    for (int j = 0; j < numColunas; j++) {
      linhaAtual[j] = random(-10, 0); // Início fora da tela (acima)
      tipoElemento[j] = random(6, 8); // Alterna entre nota (6) e estrela (7)
    }

    while (true) {
      for (int j = 0; j < numColunas; j++) {
        // Apaga o caractere anterior
        if (linhaAtual[j] >= 0 && linhaAtual[j] < 4) {
          bool naAreaDoLogo = (linhaAtual[j] == 1 || linhaAtual[j] == 2) && (colunas[j] >= 6 && colunas[j] <= 12);
          if (!naAreaDoLogo) {
            lcd.setCursor(colunas[j], linhaAtual[j]);
            lcd.print(" ");
          }
        }

        linhaAtual[j]++;

        // Reinicia quando o elemento sai da tela
        if (linhaAtual[j] > 3) {
          linhaAtual[j] = random(-5, 0);
          tipoElemento[j] = random(6, 8);
        }

        // Desenha o novo caractere (evitando sobrescrever o logo)
        if (linhaAtual[j] >= 0 && linhaAtual[j] < 4) {
          bool naAreaDoLogo = (linhaAtual[j] == 1 || linhaAtual[j] == 2) && (colunas[j] >= 6 && colunas[j] <= 12);
          if (!naAreaDoLogo) {
            lcd.setCursor(colunas[j], linhaAtual[j]);
            lcd.write(tipoElemento[j]);
          }
        }
      }
      if (esperaPulo(200)) goto fimAnimacao;
    }
  }

fimAnimacao:
  noTone(BUZZER);
  lcd.createChar(4, alerta); // Restaura o ícone de alerta no slot 4
  lcd.clear();
}

// =============================================================================
// TELAS DA INTERFACE
// =============================================================================

/**
 * TELA 0 — LOBBY (tela inicial após ligar o sistema)
 *
 * Exibe as opções de navegação disponíveis.
 * Redesenha apenas quando `precisaDesenhar` é true (otimização de LCD).
 *
 * Teclas:
 *   A → Menu Principal (estado 1)
 *   B → Configurar (estado 3)
 *   C → Ver Logs (estado 6)
 *
 * @param key  Tecla pressionada neste ciclo (0 se nenhuma)
 */
void lobby(char key) {
  if (precisaDesenhar) {
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print(F("|====||LOBBY||=====|"));
    lcd.setCursor(3, 1); lcd.print(F("A: ABRIR MENU"));
    lcd.setCursor(3, 2); lcd.print(F("B: CONFIGURAR"));
    lcd.setCursor(3, 3); lcd.print(F("C: VER LOGS"));
    precisaDesenhar = false;
  }

  if (key == 'A') { efeitoSaida(); estadoAtual = 1; precisaDesenhar = true; efeitoEntrada(); }
  if (key == 'B') { efeitoSaida(); estadoAtual = 3; precisaDesenhar = true; efeitoEntrada(); }
  if (key == 'C') { efeitoSaida(); estadoAtual = 6; precisaDesenhar = true; efeitoEntrada(); }
}

/**
 * TELA 1 — MENU PRINCIPAL
 *
 * Exibe os 4 sensores/visualizações disponíveis.
 * Atualiza os ícones de alerta (slot 4 da CGRAM) ao lado de cada opção
 * a cada 400ms: piscante para status 2 (alerta), fixo para status 1 (atenção).
 *
 * Teclas:
 *   1 → Tela de temperatura (estado 2, cursorMenu=0)
 *   2 → Tela de umidade (estado 2, cursorMenu=1)
 *   3 → Tela de luminosidade (estado 2, cursorMenu=2)
 *   4 → Tela de data/hora (estado 2, cursorMenu=3)
 *   B → Voltar ao Lobby (estado 0)
 *
 * @param key  Tecla pressionada neste ciclo
 */
void menuPrincipal(char key) {
  if (precisaDesenhar) {
    lcd.clear();
    lcd.setCursor(0,0); lcd.print(F("|=====||MENU||=====|"));
    lcd.setCursor(0,1); lcd.print(F("1.TEMP "));
    lcd.setCursor(10,1); lcd.print(F("2.UMID "));
    lcd.setCursor(0,2); lcd.print(F("3.LUZ  "));
    lcd.setCursor(10,2); lcd.print(F("4.HORA"));
    lcd.setCursor(0,3); lcd.print(F("[B] VOLTAR"));
    precisaDesenhar = false;
  }

  // Atualiza indicadores de alerta periodicamente (sem redesenhar a tela toda)
  if (millis() - lastDisplayUpdate > 400) {
    lcd.setCursor(7,1);
    if(statusTemp == 2) { if(blinkState) lcd.write(4); else lcd.print(" "); }
    else if(statusTemp == 1) lcd.write(4);
    else lcd.print(" ");

    lcd.setCursor(17,1);
    if(statusHum == 2) { if(blinkState) lcd.write(4); else lcd.print(" "); }
    else if(statusHum == 1) lcd.write(4);
    else lcd.print(" ");

    lcd.setCursor(7,2);
    if(statusLum == 2) { if(blinkState) lcd.write(4); else lcd.print(" "); }
    else if(statusLum == 1) lcd.write(4);
    else lcd.print(" ");

    lastDisplayUpdate = millis();
  }

  if (key == '1') { efeitoSaida(); cursorMenu = 0; estadoAtual = 2; ultTempImpressa = -999.0; precisaDesenhar = true; efeitoEntrada(); }
  if (key == '2') { efeitoSaida(); cursorMenu = 1; estadoAtual = 2; ultHumImpressa = -999.0; precisaDesenhar = true; efeitoEntrada(); }
  if (key == '3') { efeitoSaida(); cursorMenu = 2; estadoAtual = 2; ultLumImpressa = -999.0; precisaDesenhar = true; efeitoEntrada(); }
  if (key == '4') { efeitoSaida(); cursorMenu = 3; estadoAtual = 2; precisaDesenhar = true; efeitoEntrada(); }
  if (key == 'B') { efeitoSaida(); estadoAtual = 0; precisaDesenhar = true; efeitoEntrada(); }
}

/**
 * TELA 6 — LOGS DA EEPROM
 *
 * Lê e exibe os 2 registros de alerta armazenados na EEPROM.
 * Para cada log, exibe: tipo do sensor, valor e timestamp (dia/mês hora:minuto).
 * Se a posição estiver vazia (tipo fora de 1–3), exibe "VAZIO".
 *
 * Teclas:
 *   B → Voltar ao Lobby (estado 0)
 *
 * @param key  Tecla pressionada neste ciclo
 */
void telaLogs(char key) {
  if (precisaDesenhar) {
    Log l1, l2;
    EEPROM.get(0, l1);              // Log mais recente
    EEPROM.get(sizeof(Log), l2);   // Log anterior

    // ── Log 1 (mais recente) ──
    lcd.setCursor(0,0); lcd.print(F("L1 "));
    if      (l1.tipo == 1) { lcd.print(F("TEMP:")); lcd.print(toActiveUnit(l1.valor), 1); lcd.print(useFahrenheit ? F("F") : F("C")); }
    else if (l1.tipo == 2) { lcd.print(F("UMID:")); lcd.print(l1.valor, 1); lcd.print(F("%")); }
    else if (l1.tipo == 3) { lcd.print(F("LUZ:")); lcd.print(l1.valor, 1); lcd.print(F("%")); }
    else                   { lcd.print(F("VAZIO          ")); }

    if (l1.tipo >= 1 && l1.tipo <= 3) {
      lcd.setCursor(0,1); lcd.print(l1.dia); lcd.print(F("/")); lcd.print(l1.mes);
      lcd.print(F(" ")); lcd.print(l1.hora); lcd.print(F(":")); lcd.print(l1.minuto);
    } else {
      lcd.setCursor(0,1); lcd.print(F("                "));
    }

    // ── Log 2 (anterior) ──
    lcd.setCursor(0,2); lcd.print(F("L2 "));
    if      (l2.tipo == 1) { lcd.print(F("TEMP:")); lcd.print(toActiveUnit(l2.valor), 1); lcd.print(useFahrenheit ? F("F") : F("C")); }
    else if (l2.tipo == 2) { lcd.print(F("UMID:")); lcd.print(l2.valor, 1); lcd.print(F("%")); }
    else if (l2.tipo == 3) { lcd.print(F("LUZ:")); lcd.print(l2.valor, 1); lcd.print(F("%")); }
    else                   { lcd.print(F("VAZIO          ")); }

    if (l2.tipo >= 1 && l2.tipo <= 3) {
      lcd.setCursor(0,3); lcd.print(l2.dia); lcd.print(F("/")); lcd.print(l2.mes);
      lcd.print(F(" ")); lcd.print(l2.hora); lcd.print(F(":")); lcd.print(l2.minuto);
    } else {
      lcd.setCursor(0,3); lcd.print(F("                "));
    }

    precisaDesenhar = false;
  }

  if (key == 'B') { efeitoSaida(); estadoAtual = 0; precisaDesenhar = true; efeitoEntrada(); }
}

/**
 * TELA 2 — DADOS DO SENSOR / DATA-HORA
 *
 * Exibe o valor atual de um sensor com ícone customizado e barra de progresso,
 * ou a data/hora atual do RTC.
 *
 * Modos (determinados por `cursorMenu`):
 *   0 = Temperatura: ícone de termômetro, valor em °C/°F, limites, barra
 *   1 = Umidade: ícone de gota, valor em %, limites, barra
 *   2 = Luminosidade: ícone de sol, valor em %, limites, barra
 *   3 = Data/Hora: ícone de relógio, leitura do RTC DS1307
 *
 * Otimização de display:
 *   O valor numérico só é reescrito quando muda (comparação com cache ultXImpressa).
 *   Os ícones de alerta piscantes são atualizados a cada 400ms.
 *
 * Ícone de alerta (slot 7 da CGRAM):
 *   Aparece piscando nas posições (4,1) e (15,1) quando o valor está fora da faixa.
 *   Carregado pela função desenharBarraTotal().
 *
 * Teclas:
 *   B → Voltar ao Menu Principal (estado 1), restaura ícone de alerta no slot 4
 *
 * @param key  Tecla pressionada neste ciclo
 */
void telaDados(char key) {
  if (precisaDesenhar) {
    lcd.clear();
    if (cursorMenu == 0) {
      // Carrega ícone de termômetro nos slots 0–3
      lcd.createChar(0,termTL); lcd.createChar(1,termTR); lcd.createChar(2,termBL); lcd.createChar(3,termBR);
      lcd.setCursor(0, 0); lcd.write(0); lcd.write(1); lcd.print(F("||TEMPERATURA:||"));
      lcd.setCursor(18, 0); lcd.write(0); lcd.write(1);
      lcd.setCursor(0, 1); lcd.write(2); lcd.write(3);
      lcd.setCursor(18, 1); lcd.write(2); lcd.write(3);
      // Exibe os limites na linha 2
      lcd.setCursor(0,2); lcd.print(tempMin);
      lcd.setCursor(5,2); lcd.write(223); lcd.print(useFahrenheit ? F("F") : F("C")); lcd.print(F("|----|"));
      lcd.setCursor(13,2); lcd.print(tempMax);
      lcd.setCursor(18,2); lcd.write(223); lcd.print(useFahrenheit ? F("F") : F("C"));
    } else if (cursorMenu == 1) {
      lcd.createChar(0,umidTL); lcd.createChar(1,umidTR); lcd.createChar(2,umidBL); lcd.createChar(3,umidBR);
      lcd.setCursor(1, 0); lcd.write(0); lcd.write(1); lcd.print(F(" ||UMIDADE:||"));
      lcd.setCursor(17, 0); lcd.write(0); lcd.write(1);
      lcd.setCursor(1, 1); lcd.write(2); lcd.write(3);
      lcd.setCursor(17, 1); lcd.write(2); lcd.write(3);
      lcd.setCursor(0,2); lcd.print(humMin); lcd.print(F("%"));
      lcd.setCursor(5,2); lcd.print(F("%")); lcd.setCursor(6,2); lcd.print(F("|------|"));
      lcd.setCursor(14,2); lcd.print(humMax); lcd.setCursor(19,2); lcd.print(F("%"));
    } else if (cursorMenu == 2) {
      lcd.createChar(0,luzTL); lcd.createChar(1,luzTR); lcd.createChar(2,luzBL); lcd.createChar(3,luzBR);
      lcd.setCursor(1, 0); lcd.write(0); lcd.write(1); lcd.print(F(" ||  LUZ:  ||"));
      lcd.setCursor(17, 0); lcd.write(0); lcd.write(1);
      lcd.setCursor(1, 1); lcd.write(2); lcd.write(3);
      lcd.setCursor(17, 1); lcd.write(2); lcd.write(3);
      lcd.setCursor(0,2); lcd.print(lumMin); lcd.print(F("%"));
      lcd.setCursor(5,2); lcd.print(F("%")); lcd.setCursor(6,2); lcd.print(F("|------|"));
      lcd.setCursor(14,2); lcd.print(lumMax); lcd.setCursor(19,2); lcd.print(F("%"));
    } else {
      // Tela de data/hora
      lcd.createChar(0,tempoTL); lcd.createChar(1,tempoTR); lcd.createChar(2,tempoBL); lcd.createChar(3,tempoBR);
      lcd.setCursor(1, 0); lcd.write(0); lcd.write(1); lcd.print(F("||DATA/HORA:||")); lcd.write(0); lcd.write(1);
      lcd.setCursor(1, 1); lcd.write(2); lcd.write(3);
      lcd.setCursor(17, 1); lcd.write(2); lcd.write(3);
      lcd.setCursor(0,3); lcd.print(F("|==================|"));
    }
    // Invalida o cache para forçar atualização imediata dos valores
    ultTempImpressa = -999.0;
    ultHumImpressa  = -999.0;
    ultLumImpressa  = -999.0;
    precisaDesenhar = false;
  }

  bool updateBlink = (millis() - lastDisplayUpdate > 400);

  if (cursorMenu == 0) {
    // Atualiza valor de temperatura apenas se mudou
    if (tempAtual != ultTempImpressa) {
      lcd.setCursor(5, 1); lcd.print(F("T:")); lcd.print(tempAtual, 1);
      lcd.setCursor(12,1); lcd.write(223); lcd.print(useFahrenheit ? F("F") : F("C"));
      desenharBarraTotal(tempAtual, tempMin, tempMax);
      ultTempImpressa = tempAtual;
    }
    // Ícones de alerta piscantes
    if (updateBlink) {
      if (tempAtual <= tempMin || tempAtual >= tempMax) {
        lcd.setCursor(4,1); if(blinkState) lcd.write(7); else lcd.print(" ");
        lcd.setCursor(15,1); if(blinkState) lcd.write(7); else lcd.print(" ");
      } else {
        lcd.setCursor(4,1); lcd.print(" "); lcd.setCursor(15,1); lcd.print(" ");
      }
    }
  } else if (cursorMenu == 1) {
    if (humAtual != ultHumImpressa) {
      lcd.setCursor(6, 1); lcd.print(F("UMID:")); lcd.print(humAtual, 0);
      lcd.setCursor(13,1); lcd.print(F("%"));
      desenharBarraTotal(humAtual, humMin, humMax);
      ultHumImpressa = humAtual;
    }
    if (updateBlink) {
      if (humAtual <= humMin || humAtual >= humMax) {
        lcd.setCursor(4,1); if(blinkState) lcd.write(7); else lcd.print(" ");
        lcd.setCursor(15,1); if(blinkState) lcd.write(7); else lcd.print(" ");
      } else {
        lcd.setCursor(4,1); lcd.print(" "); lcd.setCursor(15,1); lcd.print(" ");
      }
    }
  } else if (cursorMenu == 2) {
    if (lumAtual != ultLumImpressa) {
      lcd.setCursor(6, 1); lcd.print(F("LUZ: ")); lcd.print(lumAtual, 0);
      lcd.setCursor(13,1); lcd.print(F("%"));
      desenharBarraTotal(lumAtual, lumMin, lumMax);
      ultLumImpressa = lumAtual;
    }
    if (updateBlink) {
      if (lumAtual <= lumMin || lumAtual >= lumMax) {
        lcd.setCursor(4,1); if(blinkState) lcd.write(7); else lcd.print(" ");
        lcd.setCursor(15,1); if(blinkState) lcd.write(7); else lcd.print(" ");
      } else {
        lcd.setCursor(4,1); lcd.print(" "); lcd.setCursor(15,1); lcd.print(" ");
      }
    }
  } else {
    // Data/hora: atualiza a cada 400ms
    if (updateBlink) {
      DateTime now = rtc.now();
      lcd.setCursor(4,1); lcd.print(F("DATA: ")); lcd.print(now.day()); lcd.print(F("/")); lcd.print(now.month()); lcd.print(F("  "));
      lcd.setCursor(4, 2); lcd.print(F("HORA: ")); lcd.print(now.hour()); lcd.print(F(":"));
      if (now.minute() < 10) lcd.print("0");
      lcd.print(now.minute()); lcd.print(F("  "));
    }
  }

  if (updateBlink) lastDisplayUpdate = millis();

  if (key == 'B') {
    efeitoSaida();
    estadoAtual = 1;
    lcd.createChar(4, alerta); // Restaura o ícone de alerta no slot 4 para o menu
    precisaDesenhar = true;
    efeitoEntrada();
  }
}

// =============================================================================
// TELAS DE CONFIGURAÇÃO
// =============================================================================

/**
 * TELA 3 — SELECIONAR PARÂMETRO A CONFIGURAR
 *
 * Ponto de entrada para configuração de limites.
 * Permite selecionar qual parâmetro terá seus limites alterados,
 * ou acessar as configurações gerais do sistema.
 *
 * Teclas:
 *   1 → Configurar limites de temperatura (paramSelecionado=1, estado 4)
 *   2 → Configurar limites de umidade (paramSelecionado=2, estado 4)
 *   3 → Configurar limites de luminosidade (paramSelecionado=3, estado 4)
 *   4 → Configurações do sistema (estado 7)
 *   B → Voltar ao Lobby (estado 0)
 *
 * @param key  Tecla pressionada neste ciclo
 */
void configParametro(char key) {
  if (precisaDesenhar) {
    lcd.setCursor(0,0); lcd.print(F("CONFIGURAR:"));
    lcd.setCursor(0,1); lcd.print(F("1. TEMP"));
    lcd.setCursor(10,1); lcd.print(F("2. UMID"));
    lcd.setCursor(0,2); lcd.print(F("3. LUZ"));
    lcd.setCursor(10,2); lcd.print(F("4. SISTEMA"));
    lcd.setCursor(0,3); lcd.print(F("[B] VOLTAR"));
    precisaDesenhar = false;
  }

  if(key == '1') { efeitoSaida(); paramSelecionado = 1; estadoAtual = 4; precisaDesenhar = true; efeitoEntrada(); }
  if(key == '2') { efeitoSaida(); paramSelecionado = 2; estadoAtual = 4; precisaDesenhar = true; efeitoEntrada(); }
  if(key == '3') { efeitoSaida(); paramSelecionado = 3; estadoAtual = 4; precisaDesenhar = true; efeitoEntrada(); }
  if(key == '4') { efeitoSaida(); estadoAtual = 7; precisaDesenhar = true; efeitoEntrada(); }
  if(key == 'B') { efeitoSaida(); estadoAtual = 0; precisaDesenhar = true; efeitoEntrada(); }
}

/**
 * TELA 7 — CONFIGURAÇÕES DO SISTEMA
 *
 * Permite alternar a unidade de temperatura (°C/°F) e o estado do som (ON/OFF).
 *
 * Ao alternar a unidade:
 *   Os limites de temperatura (tempMax e tempMin) são convertidos automaticamente
 *   para a nova unidade, preservando os valores configurados pelo usuário.
 *
 * Teclas:
 *   1 → Alternar unidade de temperatura (°C ↔ °F)
 *   2 → Alternar som (muted ↔ ON)
 *   B → Voltar à seleção de parâmetro (estado 3)
 *
 * @param key  Tecla pressionada neste ciclo
 */
void configSistema(char key) {
  if (precisaDesenhar) {
    lcd.setCursor(0,0); lcd.print(F("SISTEMA:"));
    lcd.setCursor(0,1); lcd.print(F("1. UNIDADE: ")); lcd.print(useFahrenheit ? F("F  ") : F("C  "));
    lcd.setCursor(0,2); lcd.print(F("2. SOM: ")); lcd.print(muted ? F("OFF") : F("ON "));
    lcd.setCursor(0,3); lcd.print(F("[B] VOLTAR"));
    precisaDesenhar = false;
  }

  if(key == '1') {
    // Converte os limites de temperatura para a nova unidade
    if (useFahrenheit) {
      tempMax = (tempMax - 32.0) * 5.0 / 9.0;
      tempMin = (tempMin - 32.0) * 5.0 / 9.0;
    } else {
      tempMax = tempMax * 9.0 / 5.0 + 32.0;
      tempMin = tempMin * 9.0 / 5.0 + 32.0;
    }
    useFahrenheit = !useFahrenheit;
    if (!muted) tone(BUZZER, 2000, 150);
    precisaDesenhar = true;
  }

  if(key == '2') {
    muted = !muted;
    if (!muted) tone(BUZZER, 2000, 150); // Confirmação sonora ao reativar
    precisaDesenhar = true;
  }

  if(key == 'B') { efeitoSaida(); estadoAtual = 3; precisaDesenhar = true; efeitoEntrada(); }
}

/**
 * TELA 4 — SELECIONAR LIMITE (MÁXIMO OU MÍNIMO)
 *
 * Após selecionar o parâmetro, o usuário escolhe qual limite editar.
 *
 * Teclas:
 *   1 → Editar limite máximo (limiteSelecionado=1, estado 5)
 *   2 → Editar limite mínimo (limiteSelecionado=2, estado 5)
 *   B → Voltar à seleção de parâmetro (estado 3)
 *
 * @param key  Tecla pressionada neste ciclo
 */
void configLimite(char key) {
  if (precisaDesenhar) {
    lcd.setCursor(0,0);
    if      (paramSelecionado == 1) lcd.print(F("TEMP - LIMITE:"));
    else if (paramSelecionado == 2) lcd.print(F("UMID - LIMITE:"));
    else                            lcd.print(F("LUZ - LIMITE:"));
    lcd.setCursor(0,1); lcd.print(F("1. MAXIMO"));
    lcd.setCursor(0,2); lcd.print(F("2. MINIMO"));
    lcd.setCursor(0,3); lcd.print(F("[B] VOLTAR"));
    precisaDesenhar = false;
  }

  if(key == '1') { efeitoSaida(); limiteSelecionado = 1; memset(inputBuffer, 0, sizeof(inputBuffer)); inputIndex = 0; estadoAtual = 5; precisaDesenhar = true; efeitoEntrada(); }
  if(key == '2') { efeitoSaida(); limiteSelecionado = 2; memset(inputBuffer, 0, sizeof(inputBuffer)); inputIndex = 0; estadoAtual = 5; precisaDesenhar = true; efeitoEntrada(); }
  if(key == 'B') { efeitoSaida(); estadoAtual = 3; precisaDesenhar = true; efeitoEntrada(); }
}

/**
 * TELA 5 — ENTRADA DE VALOR NUMÉRICO
 *
 * Permite ao usuário digitar um valor numérico inteiro (até 4 dígitos)
 * usando o teclado matricial, para definir um novo limite de alerta.
 *
 * Validação ao confirmar ('A'):
 *   - Limite máximo deve ser maior que o mínimo atual
 *   - Limite mínimo deve ser menor que o máximo atual
 *   - Se inválido: exibe mensagem de erro e aguarda tecla 'C'
 *   - Se válido: aplica o novo limite e retorna ao Lobby
 *
 * Teclas:
 *   0–9 → Adiciona dígito ao buffer (máx. 4 dígitos)
 *   A   → Confirma e valida o valor
 *   B   → Cancela e volta à seleção de limite (estado 4)
 *   C   → Limpa o buffer de entrada
 *
 * @param key  Tecla pressionada neste ciclo
 */
void configValor(char key) {
  if (precisaDesenhar) {
    lcd.setCursor(0,0); lcd.print(F("DIGITE O VALOR:"));
    lcd.setCursor(0,1); lcd.print(F("-> ")); lcd.print(inputBuffer); lcd.print(F("_   "));
    lcd.setCursor(0,3); lcd.print(F("A:CONFIRMA  C:LIMPA"));
    precisaDesenhar = false;
  }

  // ── Entrada de dígitos ──
  if ((key >= '0' && key <= '9') && inputIndex < 4) {
    inputBuffer[inputIndex] = key;
    inputIndex++;
    inputBuffer[inputIndex] = '\0';
    precisaDesenhar = true;
  }

  // ── Limpar buffer ──
  if (key == 'C') {
    memset(inputBuffer, 0, sizeof(inputBuffer));
    inputIndex = 0;
    precisaDesenhar = true;
  }

  if(key == 'B') { efeitoSaida(); estadoAtual = 4; precisaDesenhar = true; efeitoEntrada(); }

  // ── Confirmação e validação ──
  if (key == 'A' && inputIndex > 0) {
    float v = atof(inputBuffer); // Converte string para float
    bool erro = false;

    // Verifica consistência: máximo > mínimo atual / mínimo < máximo atual
    if (paramSelecionado == 1) {
      if (limiteSelecionado == 1 && v <= tempMin) erro = true;
      if (limiteSelecionado == 2 && v >= tempMax) erro = true;
    } else if (paramSelecionado == 2) {
      if (limiteSelecionado == 1 && v <= humMin)  erro = true;
      if (limiteSelecionado == 2 && v >= humMax)  erro = true;
    } else if (paramSelecionado == 3) {
      if (limiteSelecionado == 1 && v <= lumMin)  erro = true;
      if (limiteSelecionado == 2 && v >= lumMax)  erro = true;
    }

    if (erro) {
      // ── Exibe mensagem de erro e aguarda confirmação ──
      lcd.clear();
      lcd.setCursor(0,0); lcd.print(F("VALOR INVALIDO!"));
      if (limiteSelecionado == 1) {
        lcd.setCursor(0,1); lcd.print(F("MAX DEVE SER MAIOR"));
        lcd.setCursor(0,2); lcd.print(F("QUE O MIN ATUAL."));
      } else {
        lcd.setCursor(0,1); lcd.print(F("MIN DEVE SER MENOR"));
        lcd.setCursor(0,2); lcd.print(F("QUE O MAX ATUAL."));
      }
      lcd.setCursor(0,3); lcd.print(F("PRESSIONE [C]..."));
      if (!muted) tone(BUZZER, 400, 500); // Bip de erro (frequência grave)
      while(keypad.getKey() != 'C') { delay(50); } // Aguarda confirmação
      memset(inputBuffer, 0, sizeof(inputBuffer));
      inputIndex = 0;
      precisaDesenhar = true;
      lcd.clear();
      return;
    }

    // ── Aplica o novo limite ──
    if      (paramSelecionado == 1) { if(limiteSelecionado == 1) tempMax = v; else tempMin = v; }
    else if (paramSelecionado == 2) { if(limiteSelecionado == 1) humMax = v;  else humMin = v;  }
    else                            { if(limiteSelecionado == 1) lumMax = v;  else lumMin = v;  }

    if (!muted) tone(BUZZER, 2000, 300); // Bip de confirmação (frequência alta)
    efeitoSaida(); estadoAtual = 0; precisaDesenhar = true; efeitoEntrada();
  }
}

// =============================================================================
// DESLIGAMENTO
// =============================================================================

/**
 * Desliga o sistema completamente:
 *   - Limpa e apaga o backlight do LCD
 *   - Para o buzzer
 *   - Apaga o LED RGB
 *   - Marca sistemaLigado = false
 *
 * Chamado ao pressionar '*' com o sistema ligado, ou na inicialização (setup).
 */
void desligarTelas() {
  lcd.clear();
  lcd.noBacklight();
  noTone(BUZZER);
  setRGB(0, 0, 0);
  sistemaLigado = false;
}
