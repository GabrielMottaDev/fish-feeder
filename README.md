# 🐠 Fish Feeder - Alimentador Automático Inteligente

Firmware ESP32 para alimentador automático de aquários com conectividade WiFi, sincronização horária pela internet e interface web de controle.


## 🎓 Projeto Acadêmico

Projeto desenvolvido para a disciplina de Microcontroladores do curso de Ciência da Computação como trabalho extensionista, resolvendo um problema real de aquaristas que precisam manter rotinas regulares de alimentação.

Aplicando conceitos de sistemas embarcados, redes, eletrônica e arquitetura de software para resolver problema real de aquaristas.

**Diferenciais técnicos**: Arquitetura modular com padrões profissionais (Service Locator, State Machine), código não bloqueante, recuperação automática de falhas, persistência em NVRAM e múltiplas fontes de tempo.

**Conceitos aplicados**: Programação ESP32, GPIO/PWM/I2C, controle de motores, protocolos TCP/IP/UDP/NTP, servidor web embarcado, OOP em C++, multitarefa cooperativa, circuitos com transistores.

## 🚀 Como Usar

**Instalação**:
```bash
git clone https://github.com/GabrielMottaDev/fish-feeder.git
cd fish-feeder
pio run --target upload --upload-port COM6
pio device monitor --port COM6
```

**Configuração inicial**:
1. Conecte ao WiFi "FishFeeder-Setup"
2. Acesse http://192.168.4.1
3. Configure sua rede WiFi
4. Aguarde LED verde (pronto)
5. Ajuste horário: `SET DD/MM/YYYY HH:MM:SS` ou aguarde sincronização automática
6. Teste: `FEED 1` ou toque longo no sensor

## ✨ Features

- ⏰ **Agendamento automático** com 3 horários diários configuráveis
- 🔌 **Recuperação de falha de energia** - detecta e executa alimentações perdidas
- 🌐 **Portal web integrado** (192.168.4.1) para configuração e controle remoto
- 📡 **Sincronização pela internet** (NTP + fallback HTTP) com timezone configurável
- 🔄 **Reconexão WiFi automática** com estratégia exponencial (padrão AWS)
- 👆 **Controle por toque** - sensor capacitivo com feedback vibratório
- 💡 **LED RGB status** - indicação visual de operações e erros
- 💾 **Persistência NVRAM** - configurações sobrevivem a reinicializações
- 🚀 **100% não bloqueante** - 11 tarefas concorrentes, resposta < 100ms
- 🔧 **API REST completa** - integração com automação residencial
- ⚡ **Controle de porções** - 1 a 10 porções por alimentação
- 📊 **Console serial** - comandos detalhados para diagnóstico e configuração

## 📦 Dependências

**Plataforma**: PlatformIO com Arduino Framework para ESP32

**Bibliotecas**:
- `adafruit/RTClib@^2.1.4` - Interface com DS3231 RTC
- `waspinator/AccelStepper@^1.64` - Controle suave do motor de passo
- `arkhipenko/TaskScheduler@^4.0.2` - Multitarefa cooperativa não bloqueante
- `tzapu/WiFiManager@^2.0.17` - Portal de configuração WiFi com captive portal

**Built-in**: WiFi, Preferences, time.h (NTP), Wire (I2C)

## ⚙️ Hardware

**Microcontrolador**: ESP32-WROOM-32 Development Board (dual-core 240MHz, WiFi integrado)

**Componentes necessários**:
- **DS3231 RTC** com bateria CR2032 para manter horário
- **Motor de passo 28BYJ-48** + driver ULN2003 para dispensar alimento
- **Sensor touch capacitivo TTP223** para controle manual
- **Motor de vibração 1027** (3V) para feedback tátil
- **LED RGB 4 pinos** (catodo comum) para indicação visual de status
- **Transistor NPN 2N2222** para controle do motor de vibração
- **Diodo 1N4007** para proteção
- **Resistores**: 3x 330Ω (LED RGB), 1x 1kΩ (base transistor)
- **Capacitor 100nF** para estabilização
- **Fonte 5V** (mínimo 1A)

**Conexões (ESP32)**:
```
DS3231 RTC      → GPIO 21 (SDA), GPIO 22 (SCL)
Motor 28BYJ-48  → GPIO 15, 4, 5, 18
Motor Vibração  → GPIO 26 (via transistor 2N2222)
LED RGB         → GPIO 25 (R), 27 (G), 32 (B)
Sensor Touch    → GPIO 33
```

## 🎮 Controles

**Sensor touch**: Toque curto vibra (feedback), pressão longa (2s) inicia alimentação ou cancela se em progresso.

**LED RGB status**:
- 🟢 Verde sólido = pronto
- 🟢🚦 Verde piscando = alimentando
- 🔴⚡ Vermelho flash = cancelado
- 🔵🚦 Azul piscando = conectando WiFi
- 🔴🚦 Vermelho piscando = erro WiFi

**Comandos principais** (serial 115200 baud):
```
FEED [1-10]                  - Alimenta N porções
TIME                         - Mostra data/hora
SET DD/MM/YYYY HH:MM:SS      - Ajusta horário
WIFI STATUS                  - Status WiFi
WIFI PORTAL                  - Abre portal configuração
SCHEDULE LIST                - Lista agendamentos
NTP SYNC                     - Sincroniza horário agora
HELP                         - Lista todos os comandos
```

## 🚧 Melhorias Futuras (TO-DO)

Sensores de água, câmera remota, editor web de agendamentos, MQTT/Home Assistant, app móvel, gráficos de histórico, OTA updates.

---

Desenvolvido para a comunidade de aquaristas 🐠