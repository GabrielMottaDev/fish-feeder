#include <Wire.h>
#include <RTClib.h>
#include <Stepper.h>
#include <EEPROM.h>

// ===================== CONFIGURAÇÕES =====================

// Motor 28BYJ-48: 2048 passos/volta
const int stepsPerRevolution = 2048;
const int FEED_TURNS = 3;     // voltas por alimentação automática

// Horários diários (hora, minuto) para alimentação agendada
const int feedSchedule[][2] = {
  {8,  0},   // 08:00
  {12, 0},   // 12:00
  {18, 0}    // 18:00
};
const int feedScheduleCount = sizeof(feedSchedule) / sizeof(feedSchedule[0]);

// Pinos
Stepper myStepper(stepsPerRevolution, 8, 10, 9, 11);
const int buttonPin = 3; // botão entre pino e GND (INPUT_PULLUP)

// Automático por intervalo (opcional; pode desativar colocando um valor muito alto)
long feedInterval = (15L * 1000L); // 15s só de exemplo
long lastIntervalFeedMs = -1;

// Debounce botão
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// RTC
RTC_DS3231 rtc;

// Estado global para não sobrepor movimentos
volatile bool isFeeding = false;

// EEPROM: guardamos o "último minuto Unix" em que um FEED AGENDADO foi executado
// (4 bytes para uint32_t)
const int EEPROM_ADDR_LAST_SCHEDULED_MINUTE = 0;

// ===================== HELPERS EEPROM ====================
uint32_t eepromReadU32(int addr) {
  uint32_t value = 0;
  for (int i = 0; i < 4; i++) {
    value |= ((uint32_t)EEPROM.read(addr + i)) << (8 * i);
  }
  return value;
}

void eepromWriteU32(int addr, uint32_t value) {
  for (int i = 0; i < 4; i++) {
    byte b = (value >> (8 * i)) & 0xFF;
    if (EEPROM.read(addr + i) != b) {
      EEPROM.write(addr + i, b);
    }
  }
}

// ======================== SETUP ==========================
void setup() {
  Serial.begin(9600);
  Wire.begin();
  pinMode(buttonPin, INPUT_PULLUP);
  myStepper.setSpeed(10); // RPM

  if (!rtc.begin()) {
    Serial.println("⚠️ Erro: RTC DS3231 não detectado!");
    while (1);
  }
  if (rtc.lostPower()) {
    Serial.println("⚙️ RTC sem hora configurada, ajustando para a hora da compilação...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  Serial.println("✅ Sistema iniciado.");
  Serial.print("FEED_TURNS = ");
  Serial.println(FEED_TURNS);

  uint32_t lastMinute = eepromReadU32(EEPROM_ADDR_LAST_SCHEDULED_MINUTE);
  Serial.print("EEPROM último minuto agendado alimentado = ");
  Serial.println(lastMinute);
}

// ========================= LOOP =========================
void loop() {
  handleButtonManual();      // não interfere com automático (usa isFeeding)
  handleAutoByInterval();    // automático por intervalo (independente do botão)
  handleAutoBySchedule();    // automático por horário (usa EEPROM p/ não duplicar)
  delay(10);
}

// ============= FUNÇÕES DE MOVIMENTO (ISOLADAS) ===========

void feedAutoTurns(int turns, bool fromSchedule, uint32_t scheduleMinuteKey) {
  if (isFeeding) return; // evita sobreposição
  isFeeding = true;

  Serial.print("🐟 Feed AUTO → ");
  Serial.print(turns);
  Serial.println(" volta(s)...");
  myStepper.step(stepsPerRevolution * turns);
  Serial.println("✅ Feed AUTO concluído.\n");

  // Se veio do agendamento (horário do dia), grava o minuto na EEPROM
  if (fromSchedule) {
    eepromWriteU32(EEPROM_ADDR_LAST_SCHEDULED_MINUTE, scheduleMinuteKey);
  }

  isFeeding = false;
}

void feedManualOneTurn() {
  if (isFeeding) {
    // Se o motor está ocupado (auto rodando), ignoramos o manual
    Serial.println("⏳ Motor ocupado com auto-feed. Botão ignorado agora.");
    return;
  }
  isFeeding = true;

  Serial.println("🎛️ Feed MANUAL → 1 volta");
  myStepper.step(stepsPerRevolution);
  Serial.println("✅ Feed MANUAL concluído.\n");

  // Importante: NÃO alterar timers/EEPROM aqui — não interfere com automáticos
  isFeeding = false;
}

// =================== LÓGICA: BOTÃO MANUAL =================
void handleButtonManual() {
  int reading = digitalRead(buttonPin);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading == LOW && lastButtonState == HIGH) {
      feedManualOneTurn();
    }
  }
  lastButtonState = reading;
}

// ======= LÓGICA: AUTOMÁTICO POR INTERVALO (OPCIONAL) ======
void handleAutoByInterval() {
  unsigned long nowMs = millis();

  // Não execute se o motor está em uso (manual/schedule)
  if (isFeeding) return;

  if (lastIntervalFeedMs == -1 || nowMs - lastIntervalFeedMs >= (unsigned long)feedInterval) {
    Serial.println("⏱️ Auto por intervalo");
    feedAutoTurns(FEED_TURNS, /*fromSchedule=*/false, /*scheduleMinuteKey=*/0);
    lastIntervalFeedMs = nowMs;
  }
}

// ======= LÓGICA: AUTOMÁTICO POR HORÁRIO (DS3231 + EEPROM) ======
void handleAutoBySchedule() {
  if (isFeeding) return; // não tentar se já está alimentando

  DateTime now = rtc.now();
  int h = now.hour();
  int m = now.minute();

  // Chave única do minuto: epoch em minutos
  uint32_t minuteKey = (uint32_t)(now.unixtime() / 60);

  // Já alimentamos neste minuto?
  uint32_t lastMinuteDone = eepromReadU32(EEPROM_ADDR_LAST_SCHEDULED_MINUTE);
  if (lastMinuteDone == minuteKey) {
    // já foi alimentado pelo agendamento neste minuto (inclusive após reset)
    return;
  }

  // Verifica se (h,m) está na tabela
  for (int i = 0; i < feedScheduleCount; i++) {
    if (feedSchedule[i][0] == h && feedSchedule[i][1] == m) {
      // Agenda bateu. Executa uma vez por minuto.
      char buf[6];
      snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
      Serial.print("🕒 Feed agendado às ");
      Serial.println(buf);

      feedAutoTurns(FEED_TURNS, /*fromSchedule=*/true, /*scheduleMinuteKey=*/minuteKey);
      break; // evita chamadas múltiplas no mesmo loop
    }
  }
}
