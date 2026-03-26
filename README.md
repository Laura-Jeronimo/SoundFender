# 🎸 SoundFender

> **Data Logger Ambiental para Instrumentos Musicais**  
> Monitoramento inteligente de temperatura, umidade e luminosidade para proteção de instrumentos em espaços de armazenamento.

---

## 📋 Índice

- [Sobre o Projeto](#-sobre-o-projeto)
- [Funcionalidades](#-funcionalidades)
- [Hardware](#-hardware)
  - [Componentes Necessários](#componentes-necessários)
  - [Pinagem](#pinagem)
  - [Diagrama de Conexões](#diagrama-de-conexões)
- [Software](#-software)
  - [Bibliotecas Necessárias](#bibliotecas-necessárias)
  - [Estrutura do Código](#estrutura-do-código)
- [Interface do Usuário](#-interface-do-usuário)
  - [Máquina de Estados](#máquina-de-estados)
  - [Navegação por Teclado](#navegação-por-teclado)
- [Limites e Configurações](#-limites-e-configurações)
- [Sistema de Alertas](#-sistema-de-alertas)
- [Log de Eventos (EEPROM)](#-log-de-eventos-eeprom)
- [Animação de Inicialização](#-animação-de-inicialização)
- [Como Usar](#-como-usar)
- [Versão](#-versão)

---

## 🎵 Sobre o Projeto

O **SoundFender** é um sistema embarcado de monitoramento ambiental desenvolvido para proteger instrumentos musicais em espaços de armazenamento — como estúdios, depósitos e salas de ensaio. Utilizando um **Arduino Uno R3 (ATmega328P)**, o sistema lê continuamente temperatura, umidade relativa e luminosidade, emite alertas visuais e sonoros quando os parâmetros saem das faixas configuradas, e registra os eventos com timestamp na memória EEPROM interna do microcontrolador.

O projeto foi desenvolvido com atenção especial à experiência do usuário, incluindo uma **animação de abertura temática musical** com melodia inspirada na "Ode à Alegria" de Beethoven, ícones customizados desenhados em bitmap 5×8 para o display LCD, e uma interface de configuração completa acessível pelo teclado matricial.

---

## ✨ Funcionalidades

- **Monitoramento em tempo real** de temperatura, umidade e luminosidade com atualização a cada 2 segundos
- **Três níveis de status por sensor:** Normal (verde) → Atenção (amarelo) → Alerta (vermelho piscante)
- **Alertas visuais** com LED RGB tricolor e ícones piscantes no LCD
- **Alertas sonoros** com buzzer piezoelétrico (bip de 1800 Hz em modo alerta)
- **Registro de eventos** com timestamp na EEPROM (FIFO de 2 registros)
- **Limites totalmente configuráveis** via teclado matricial
- **Suporte a °C e °F** com conversão automática dos limites
- **Modo mudo** para silenciar o buzzer sem desativar os alertas visuais
- **Data e hora em tempo real** via módulo RTC DS1307
- **Barra de progresso** visual no LCD para cada parâmetro monitorado
- **Animação de abertura** com pentagrama musical e melodia de "Ode à Alegria"
- **Efeitos de transição** entre telas (fade in/out via backlight e preenchimento com blocos)

---

## 🔧 Hardware

### Componentes Necessários

| Componente | Quantidade | Descrição |
|---|---|---|
| Arduino Uno R3 | 1 | Microcontrolador ATmega328P |
| Sensor DHT11 | 1 | Temperatura e umidade |
| Módulo RTC DS1307 | 1 | Relógio de tempo real (I2C) |
| LCD 20×4 com módulo I2C | 1 | Display (endereço 0x27) |
| LED RGB | 1 | Indicador de status (catodo comum) |
| Buzzer piezoelétrico | 1 | Alertas e melodia |
| LDR (fotorresistor) | 1 | Sensor de luminosidade |
| Teclado matricial 4×4 | 1 | Interface de navegação |
| Resistor 10kΩ | 1 | Divisor de tensão para o LDR |
| Resistores 220Ω | 3 | Limitadores de corrente para o LED RGB |

### Pinagem

| Pino Arduino | Tipo | Componente |
|---|---|---|
| `2` | Digital | DHT11 — dado |
| `3` | PWM | LED RGB — canal vermelho |
| `4` | Digital | LED RGB — canal verde |
| `5` | PWM | LED RGB — canal azul |
| `6` – `9` | Digital | Teclado matricial — linhas |
| `10` – `13` | Digital | Teclado matricial — colunas |
| `A0` | Analógico (saída) | Buzzer piezoelétrico |
| `A1` | Analógico (entrada) | LDR — divisor de tensão |
| `SDA` / `SCL` | I2C | LCD 20×4 e módulo RTC DS1307 |

### Diagrama de Conexões

```
Arduino Uno
┌─────────────────────────────────────────────┐
│                                             │
│  D2  ─────────────── DHT11 (DATA)           │
│  D3  ──[220Ω]──────── LED RGB (R)           │
│  D4  ──[220Ω]──────── LED RGB (G)           │
│  D5  ──[220Ω]──────── LED RGB (B)           │
│                                             │
│  D6  ─┐                                     │
│  D7  ─┤── Teclado Matricial 4×4 (Linhas)   │
│  D8  ─┤                                     │
│  D9  ─┘                                     │
│                                             │
│  D10 ─┐                                     │
│  D11 ─┤── Teclado Matricial 4×4 (Colunas)  │
│  D12 ─┤                                     │
│  D13 ─┘                                     │
│                                             │
│  A0  ─────────────── Buzzer (+)             │
│  A1  ─────────────── LDR ──[10kΩ]── GND    │
│                                             │
│  SDA ─┬────────────── LCD I2C (SDA)         │
│  SCL ─┤   ┌────────── RTC DS1307 (SDA/SCL) │
│       └───┘                                 │
└─────────────────────────────────────────────┘
```

> **Nota:** O LCD e o RTC compartilham o barramento I2C. Certifique-se de que os endereços I2C não colidem (LCD: `0x27`, RTC DS1307: `0x68`).

---

## 💻 Software

### Bibliotecas Necessárias

Instale as seguintes bibliotecas pelo **Gerenciador de Bibliotecas** da Arduino IDE (`Sketch > Incluir Biblioteca > Gerenciar Bibliotecas...`):

| Biblioteca | Versão Recomendada | Finalidade |
|---|---|---|
| `Wire` | (built-in) | Comunicação I2C |
| `LiquidCrystal_I2C` | ≥ 1.1.2 | Controle do LCD via I2C |
| `DHT sensor library` | ≥ 1.4.4 | Leitura do sensor DHT11 |
| `RTClib` | ≥ 2.1.1 | Leitura do RTC DS1307 |
| `Keypad` | ≥ 3.1.1 | Leitura do teclado matricial |
| `EEPROM` | (built-in) | Armazenamento de logs |

### Estrutura do Código

```
soundfender.ino
│
├── Definições e Instâncias
│   ├── Pinos de hardware
│   ├── Notas musicais (Hz)
│   ├── Instâncias: LCD, DHT, RTC, Keypad
│   └── Struct Log (registro EEPROM)
│
├── Variáveis de Estado
│   ├── sistemaLigado / estadoAtual / cursorMenu
│   ├── Leituras: tempAtual, humAtual, lumAtual
│   ├── Limites configuráveis (tempMax/Min, humMax/Min, lumMax/Min)
│   └── Flags: muted, useFahrenheit, precisaDesenhar
│
├── Bitmaps 5×8 (Caracteres Customizados)
│   ├── Ícones: termômetro, gota, sol, relógio, alerta
│   ├── Barra de progresso (segmentos: left/mid/right × filled/empty)
│   └── Elementos de animação: clave de sol, pentagrama, notas, estrelas
│
├── setup()
│
├── loop()
│   └── Despacho para máquina de estados
│
├── monitorarSensores()
│   ├── Leitura DHT11 + LDR (intervalo: 2s)
│   ├── Cálculo de status (Normal/Atenção/Alerta)
│   ├── Salvamento de log na EEPROM (transição para alerta)
│   └── Controle de LED RGB e buzzer
│
├── animacaoInicio()
│   ├── Animação do pentagrama (scroll)
│   ├── Montagem do logo "SoundFender"
│   ├── Melodia "Ode à Alegria" (15 notas)
│   └── Loop de estrelas/notas caindo
│
├── Telas da Interface
│   ├── lobby()           — Estado 0: tela inicial
│   ├── menuPrincipal()   — Estado 1: seleção de sensor
│   ├── telaDados()       — Estado 2: exibição de sensor/hora
│   ├── configParametro() — Estado 3: seleção de parâmetro
│   ├── configLimite()    — Estado 4: máximo ou mínimo
│   ├── configValor()     — Estado 5: entrada numérica
│   ├── telaLogs()        — Estado 6: histórico EEPROM
│   └── configSistema()   — Estado 7: unidade e som
│
└── Utilitários
    ├── efeitoSaida() / efeitoEntrada()
    ├── setRGB() / toActiveUnit()
    ├── desenharBarraTotal()
    ├── salvarLog() / esperaPulo()
    └── desligarTelas()
```

---

## 🖥️ Interface do Usuário

### Máquina de Estados

```
                    ┌─────────────────────────────┐
      [*] Liga      │                             │      [*] Desliga
    ──────────────► │  animacaoInicio()            │ ─────────────────►  OFF
                    │                             │
                    └──────────────┬──────────────┘
                                   │
                                   ▼
                    ┌──────────────────────────────┐
                    │       Estado 0: LOBBY        │
                    │  A: Menu  B: Config  C: Logs │
                    └──────┬───────────┬───────────┘
                           │A          │B           \C
                           ▼           ▼             ▼
              ┌────────────────┐  ┌──────────┐  ┌──────────┐
              │ Estado 1: MENU │  │ Estado 3 │  │ Estado 6 │
              │ 1.Temp 2.Umid  │  │ CONFIG   │  │  LOGS    │
              │ 3.Luz  4.Hora  │  │ PARAM    │  │  EEPROM  │
              └───────┬────────┘  └────┬─────┘  └──────────┘
                      │1-4             │1-3 / 4
                      ▼               │
             ┌──────────────┐         ├──────────────────────────────────┐
             │  Estado 2:   │         ▼                                  ▼
             │  DADOS DO    │  ┌──────────────┐               ┌──────────────────┐
             │  SENSOR /    │  │  Estado 4:   │               │    Estado 7:     │
             │  DATA-HORA   │  │ CONFIG LIMITE│               │  CONFIG SISTEMA  │
             └──────────────┘  │ 1.Max / 2.Min│               │  1.Unid / 2.Som  │
                               └──────┬───────┘               └──────────────────┘
                                      │1 ou 2
                                      ▼
                               ┌──────────────┐
                               │  Estado 5:   │
                               │  ENTRADA     │
                               │  NUMÉRICA    │
                               └──────────────┘
```

### Navegação por Teclado

| Tela | Tecla | Ação |
|---|---|---|
| **Qualquer** | `*` | Liga / desliga o sistema |
| **Lobby** | `A` | Abrir menu principal |
| **Lobby** | `B` | Ir para configurações |
| **Lobby** | `C` | Ver logs da EEPROM |
| **Menu** | `1` – `4` | Selecionar sensor/hora para visualizar |
| **Menu** | `B` | Voltar ao Lobby |
| **Dados** | `B` | Voltar ao Menu |
| **Config. Parâmetro** | `1` / `2` / `3` | Temperatura / Umidade / Luminosidade |
| **Config. Parâmetro** | `4` | Configurações do sistema |
| **Config. Parâmetro** | `B` | Voltar ao Lobby |
| **Config. Limite** | `1` / `2` | Editar máximo / mínimo |
| **Config. Limite** | `B` | Voltar à seleção de parâmetro |
| **Entrada de Valor** | `0` – `9` | Digitar valor (até 4 dígitos) |
| **Entrada de Valor** | `A` | Confirmar valor |
| **Entrada de Valor** | `B` | Cancelar e voltar |
| **Entrada de Valor** | `C` | Limpar buffer de entrada |
| **Config. Sistema** | `1` | Alternar unidade (°C ↔ °F) |
| **Config. Sistema** | `2` | Alternar som (ON ↔ OFF) |
| **Config. Sistema** | `B` | Voltar |
| **Logs** | `B` | Voltar ao Lobby |
| **Animação** | qualquer | Pular para o lobby |

---

## ⚙️ Limites e Configurações

### Limites Padrão

| Parâmetro | Mínimo | Máximo | Unidade |
|---|---|---|---|
| Temperatura | 20°C | 30°C | °C ou °F |
| Umidade | 30% | 70% | % UR |
| Luminosidade | 20% | 50% | % |

> **Referência:** Para instrumentos de corda (violões, guitarras, violinos), a NAMM e fabricantes como Gibson e Taylor recomendam temperatura entre **18°C e 24°C** e umidade entre **45% e 55%**.

### Zona de Atenção (Status 1 — LED Amarelo)

O sistema emite aviso de atenção antes de atingir o limite, para permitir ação preventiva:

| Parâmetro | Margem de Atenção |
|---|---|
| Temperatura | ± 2°C do limite |
| Umidade | ± 5% do limite |
| Luminosidade | ± 5% do limite |

### Configuração de Limites

Todos os limites são configuráveis em tempo de execução via **menu de configuração** (Lobby → `B`). A validação garante que o **máximo sempre seja maior que o mínimo**. Ao alternar entre °C e °F, os limites de temperatura são convertidos automaticamente.

---

## 🚨 Sistema de Alertas

O sistema utiliza três indicadores de alerta simultâneos:

### LED RGB

| Status | Cor | Comportamento |
|---|---|---|
| **Normal** | 🟢 Verde | Aceso continuamente |
| **Atenção** | 🟡 Amarelo | Aceso continuamente |
| **Alerta** | 🔴 Vermelho | Piscando (400ms ON / 400ms OFF) |

### Buzzer

| Evento | Frequência | Duração | Condição |
|---|---|---|---|
| Navegação de menu | 2500 Hz | 15ms | Qualquer tecla (exceto `*`) |
| Alerta ativo | 1800 Hz | 100ms | A cada ciclo de pisca (400ms) |
| Confirmação | 2000 Hz | 150–300ms | Ao salvar configuração |
| Erro de validação | 400 Hz | 500ms | Valor inválido inserido |

> O buzzer pode ser silenciado via **Config. Sistema → Tecla 2** sem desativar os alertas visuais.

### Ícones Piscantes no LCD

Quando um parâmetro está fora da faixa, o ícone de alerta (▲!) pisca nas laterais da linha de valores na tela de dados.

---

## 💾 Log de Eventos (EEPROM)

### Estrutura do Registro

```c
struct Log {
  float valor;              // Valor medido no momento do alerta
  byte  dia, mes;           // Data do evento
  byte  hora, minuto;       // Hora do evento
  byte  tipo;               // 1=Temperatura | 2=Umidade | 3=Luminosidade
};
// Tamanho: ~9 bytes por registro
```

### Comportamento

O sistema armazena os **2 eventos de alerta mais recentes** em um esquema FIFO manual:

- **Posição 0** — Evento mais recente
- **Posição `sizeof(Log)`** — Evento anterior

Um novo log é salvo **apenas na transição** de um estado não-alerta para alerta, evitando escritas repetidas enquanto a condição persiste. A EEPROM do ATmega328P suporta **100.000 ciclos de escrita** por posição.

### Visualização

Acesse os logs via **Lobby → `C`**. A tela exibe tipo do sensor, valor medido e timestamp de cada evento.

```
L1 TEMP:28.5C
14/6 22:43
L2 UMID:75.0%
13/6 18:07
```

---

## 🎬 Animação de Inicialização

A animação de abertura é executada automaticamente ao ligar o sistema (`*`) e pode ser **pulada a qualquer momento** pressionando qualquer tecla.

**Sequência:**

1. **Pentagrama musical** — as linhas da pauta se expandem da esquerda para a direita, com notas distribuídas aleatoriamente
2. **Clave de Sol** — aparece na posição 6 do pentagrama
3. **Logo "SoundFender"** — "Sound" + clave de fá + "nder" se formam gradualmente
4. **Elementos decorativos** — escudos laterais completam o logo
5. **Loop de estrelas e notas** — elementos caem verticalmente pelas colunas laterais (aguarda tecla para continuar)

**Melodia:** Os 15 primeiros compassos da "Ode à Alegria" de Beethoven (simplificados) são tocados pelo buzzer sincronizados com a animação.

---

## 🚀 Como Usar

### 1. Montagem

Monte o circuito conforme a [pinagem descrita acima](#pinagem) e o diagrama de conexões.

### 2. Instalação das Bibliotecas

Na Arduino IDE, instale todas as [bibliotecas listadas](#bibliotecas-necessárias) pelo Gerenciador de Bibliotecas.

### 3. Upload do Código

```bash
# Clone o repositório
git clone https://github.com/seu-usuario/soundfender.git

# Abra soundfender.ino na Arduino IDE
# Selecione: Ferramentas > Placa > Arduino Uno
# Selecione: Ferramentas > Porta > (porta correta)
# Clique em Upload
```

### 4. Primeira Utilização

1. Ligue o circuito — o display ficará apagado (modo standby)
2. Pressione `*` no teclado matricial para ligar o sistema
3. Aguarde ou pule a animação de abertura
4. Na tela de **Lobby**, pressione `B` para acessar as configurações
5. Configure os limites adequados para o seu ambiente e instrumentos
6. Pressione `A` no Lobby para acessar o menu e monitorar os sensores

### 5. Configuração Rápida dos Limites

```
Lobby → [B] → Config. Parâmetro
  → [1] Temperatura → [1] Máximo → Digite valor → [A]
  → [2] Temperatura → [2] Mínimo → Digite valor → [A]
  (repita para Umidade [2] e Luminosidade [3])
```
---

## 📌 Versão

| Campo | Valor |
|---|---|
| **Versão** | 1.0 |
| **Microcontrolador** | ATmega328P (Arduino Uno R3) |
| **Memória EEPROM utilizada** | ~18 bytes (2 registros de log) |
| **Intervalo de leitura** | 2 segundos |
| **Intervalo de pisca** | 400ms |
| **Protótipo** | [Protótipo SoundFender - Wokwi](https://wokwi.com/projects/459529633590347777) |

---

<div align="center">

**SoundFender** — Protegendo instrumentos com tecnologia embarcada 🎶

</div>
