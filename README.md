# 🐟 Arduino Fish Feeder

Projeto de **alimentador automático de peixes** desenvolvido com **Arduino Uno**, **motor de passo 28BYJ-48 + driver ULN2003**, e **RTC DS3231** para agendamento preciso de horários de alimentação.

O sistema permite alimentação **manual (botão)** e **automática (por horário e/ou intervalo)**, com segurança garantida contra duplicidade — mesmo após quedas de energia, graças ao uso da **EEPROM**.

---

## 🧠 Funcionalidades

✅ Alimentação **automática** em horários definidos (via DS3231)  
✅ Alimentação **manual** com botão (sem interferir com o automático)  
✅ Proteção contra **alimentação duplicada** (usa EEPROM)  
✅ Intervalo automático opcional (alimentação a cada X minutos)  
✅ Quantidade de voltas configurável (`FEED_TURNS`)  
✅ Totalmente compatível com **Arduino Uno**  
✅ Seguro contra travamentos com controle de estado (`isFeeding`)

---

## ⚙️ Hardware Utilizado

| Componente | Modelo / Observação |
|-------------|---------------------|
| **Microcontrolador** | Arduino Uno |
| **Motor de passo** | 28BYJ-48 |
| **Driver do motor** | ULN2003 |
| **RTC (Relógio de tempo real)** | DS3231 |
| **Botão manual** | Push button (entre pino e GND) |
| **Alimentação** | 5V (do Arduino ou fonte externa adequada) |

---

## 🧩 Ligações (Pinout)

| Componente | Pino no Módulo | Pino no Arduino Uno | Observação |
|-------------|----------------|---------------------|-------------|
| **DS3231 RTC** | SDA | A4 | Comunicação I²C |
| | SCL | A5 | Comunicação I²C |
| | VCC | 5V | Alimentação |
| | GND | GND | Terra |
| **ULN2003 (Motor 28BYJ-48)** | IN1 | 8 | Controle do motor |
| | IN2 | 10 | Controle do motor |
| | IN3 | 9 | Controle do motor |
| | IN4 | 11 | Controle do motor |
| **Botão Manual** | — | 3 | Entre pino e GND (usa `INPUT_PULLUP`) |

---

## 🕓 Lógica de Funcionamento

O programa combina **duas formas de ativação**:

### 🔹 Alimentação Automática por Horário
- Define os horários diários no array:
  ```cpp
  const int feedSchedule[][2] = {
    {8,  0},   // 08:00
    {12, 0},   // 12:00
    {18, 0}    // 18:00
  };
