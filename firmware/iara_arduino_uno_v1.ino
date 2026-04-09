/*
  MaeDagua - Arduino Uno V1

  Função deste sketch:
  - ler temperatura no DS18B20 (D3)
  - ler TDS no A1
  - enviar dados pela USB Serial

  O notebook/webapp pode ler essa serial,
  sincronizar e guardar logs quando quiser.

  Formato enviado:
  DEVICE=iara-uno-0001;SEQ=1;TS_MS=1034;TEMP_C=25.19;TDS_PPM=131;TDS_ADC=287;TDS_V=1.404;TEMP_OK=1;TDS_OK=1;FW=1.0.0
*/

#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>

const int PIN_TEMP_SENSOR = 3;
const int PIN_TDS_SENSOR = A1;
const char* DEVICE_ID = "iara-uno-0001";
const char* FW_VERSION = "1.5.0";

const int OLED_WIDTH = 128;
const int OLED_HEIGHT = 64;
const int OLED_RESET_PIN = -1;
const uint8_t OLED_I2C_ADDR = 0x3C;

OneWire oneWireBus(PIN_TEMP_SENSOR);
DallasTemperature temperatureSensors(&oneWireBus);
Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET_PIN);

const unsigned long SEND_INTERVAL_MS = 1000;
unsigned long lastSendAtMs = 0;
unsigned long packetSequence = 0;
const unsigned long OLED_PAGE_INTERVAL_MS = 5000;
const unsigned long OLED_TRANSITION_MS = 900;
const unsigned long OLED_REFRESH_MS = 80;
const int SERIAL_CMD_BUFFER_SIZE = 32;

const float TDS_CALIBRATION_FACTOR = 0.5;
const int TDS_SAMPLES = 20;
const int TDS_SAMPLE_DELAY_MS = 2;
const float TDS_MIN_RAW_PPM_FOR_CAL = 5.0;

const int EEPROM_ADDR_MAGIC = 0;
const int EEPROM_ADDR_TDS_GAIN = EEPROM_ADDR_MAGIC + sizeof(uint16_t);
const uint16_t EEPROM_CAL_MAGIC = 0xC411;

bool isOledReady = false;

enum CalState {
  CAL_STATE_IDLE = 0,
  CAL_STATE_WAIT_REFERENCE = 1,
  CAL_STATE_APPLIED_NOT_SAVED = 2,
  CAL_STATE_SAVED = 3
};

uint8_t calibrationState = CAL_STATE_IDLE;
float tdsCalibrationGain = 1.0;

enum ReadMode {
  READ_MODE_NORMAL = 0,
  READ_MODE_PAUSE_OLED_DURING_READ = 1,
  READ_MODE_DISPLAY_OFF_DURING_READ = 2,
  READ_MODE_OLED_DISABLED = 3,
  READ_MODE_ALTERNATE_SENSORS = 4,
  READ_MODE_COUNT
};

uint8_t currentReadMode = READ_MODE_NORMAL;
bool isSensorReadInProgress = false;
bool nextAlternateReadIsTemp = true;
char serialCmdBuffer[SERIAL_CMD_BUFFER_SIZE];
uint8_t serialCmdPos = 0;

enum DisplayMode {
  DISPLAY_MODE_ROTATING = 0,
  DISPLAY_MODE_SINGLE_SCREEN = 1
};

uint8_t currentDisplayMode = DISPLAY_MODE_ROTATING;

enum OledPage {
  OLED_PAGE_TEMP = 0,
  OLED_PAGE_TDS_PPM,
  OLED_PAGE_TDS_VOLTAGE,
  OLED_PAGE_FISH,
  OLED_PAGE_COUNT
};

uint8_t currentOledPage = OLED_PAGE_TEMP;
unsigned long oledPageStartedAtMs = 0;
unsigned long lastOledRefreshAtMs = 0;
bool isTransitionActive = false;
unsigned long transitionStartedAtMs = 0;

bool latestTempOk = false;
float latestTempC = NAN;
int latestTdsAdc = 0;
float latestRawTdsPpm = 0;
float latestTdsVoltage = 0;
float latestTdsPpm = 0;
bool hasSensorData = false;
float lastValidTempC = 0;
bool hasLastValidTemp = false;
bool tempSampledThisCycle = false;
bool tdsSampledThisCycle = false;

float readTemperatureCelsius(bool &ok) {
  temperatureSensors.requestTemperatures();
  float value = temperatureSensors.getTempCByIndex(0);
  ok = (value != DEVICE_DISCONNECTED_C);
  return ok ? value : NAN;
}

int readTdsMedianAdc() {
  int samples[TDS_SAMPLES];

  for (int i = 0; i < TDS_SAMPLES; i++) {
    samples[i] = analogRead(PIN_TDS_SENSOR);
    delay(TDS_SAMPLE_DELAY_MS);
  }

  for (int i = 0; i < TDS_SAMPLES - 1; i++) {
    for (int j = i + 1; j < TDS_SAMPLES; j++) {
      if (samples[j] < samples[i]) {
        int aux = samples[i];
        samples[i] = samples[j];
        samples[j] = aux;
      }
    }
  }

  return samples[TDS_SAMPLES / 2];
}

float convertAdcToVoltage(int adc) {
  return adc * (5.0 / 1023.0);
}

float convertAdcToTdsPpm(int adc) {
  float voltage = convertAdcToVoltage(adc);
  float tds = (voltage - 0.15) * TDS_CALIBRATION_FACTOR * 1000.0;
  if (tds < 0) tds = 0;
  return tds;
}

void loadCalibrationFromEeprom() {
  uint16_t magic = 0;
  float gain = 1.0;
  EEPROM.get(EEPROM_ADDR_MAGIC, magic);
  EEPROM.get(EEPROM_ADDR_TDS_GAIN, gain);

  if (magic == EEPROM_CAL_MAGIC && gain > 0.05 && gain < 20.0) {
    tdsCalibrationGain = gain;
    calibrationState = CAL_STATE_SAVED;
  } else {
    tdsCalibrationGain = 1.0;
    calibrationState = CAL_STATE_IDLE;
  }
}

void saveCalibrationToEeprom() {
  EEPROM.put(EEPROM_ADDR_MAGIC, EEPROM_CAL_MAGIC);
  EEPROM.put(EEPROM_ADDR_TDS_GAIN, tdsCalibrationGain);
  calibrationState = CAL_STATE_SAVED;
}

void resetCalibration() {
  tdsCalibrationGain = 1.0;
  EEPROM.put(EEPROM_ADDR_MAGIC, (uint16_t)0);
  EEPROM.put(EEPROM_ADDR_TDS_GAIN, tdsCalibrationGain);
  calibrationState = CAL_STATE_IDLE;
}

void setOledPower(bool on) {
  if (!isOledReady) {
    return;
  }
  oled.ssd1306_command(on ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF);
}

void applyReadMode(uint8_t mode) {
  if (mode >= READ_MODE_COUNT) {
    return;
  }

  currentReadMode = mode;
  nextAlternateReadIsTemp = true;

  if (currentReadMode == READ_MODE_OLED_DISABLED) {
    setOledPower(false);
  } else {
    setOledPower(true);
    oledPageStartedAtMs = millis();
    transitionStartedAtMs = millis();
    isTransitionActive = false;
  }

  Serial.print(F("MODE_SET="));
  Serial.println(currentReadMode);
}

void applyDisplayMode(uint8_t mode) {
  if (mode > DISPLAY_MODE_SINGLE_SCREEN) {
    return;
  }

  currentDisplayMode = mode;
  oledPageStartedAtMs = millis();
  transitionStartedAtMs = millis();
  isTransitionActive = false;

  Serial.print(F("DISP_SET="));
  Serial.println(currentDisplayMode);
}

void printCalibrationStatus() {
  Serial.print(F("CAL="));
  Serial.print(calibrationState);
  Serial.print(F(";CAL_GAIN="));
  Serial.println(tdsCalibrationGain, 5);
}

void processSerialCommand(const char* cmdRaw) {
  if (strncmp(cmdRaw, "MODE=", 5) == 0) {
    int mode = atoi(cmdRaw + 5);
    if (mode >= 0 && mode < READ_MODE_COUNT) {
      applyReadMode((uint8_t)mode);
    } else {
      Serial.println(F("ERR=MODE_INVALID"));
    }
    return;
  }

  if (strncmp(cmdRaw, "DISP=", 5) == 0) {
    int mode = atoi(cmdRaw + 5);
    if (mode >= 0 && mode <= DISPLAY_MODE_SINGLE_SCREEN) {
      applyDisplayMode((uint8_t)mode);
    } else {
      Serial.println(F("ERR=DISP_INVALID"));
    }
    return;
  }

  if (strcmp(cmdRaw, "CAL=ON") == 0) {
    calibrationState = CAL_STATE_WAIT_REFERENCE;
    Serial.println(F("CAL_MODE=WAIT_REFERENCE"));
    printCalibrationStatus();
    return;
  }

  if (strcmp(cmdRaw, "CAL=OFF") == 0) {
    calibrationState = CAL_STATE_IDLE;
    Serial.println(F("CAL_MODE=OFF"));
    printCalibrationStatus();
    return;
  }

  if (strncmp(cmdRaw, "CAL=APPLY:", 10) == 0) {
    float referencePpm = atof(cmdRaw + 10);
    if (referencePpm <= 0) {
      Serial.println(F("ERR=CAL_REFERENCE_INVALID"));
      return;
    }
    if (latestRawTdsPpm < TDS_MIN_RAW_PPM_FOR_CAL) {
      Serial.println(F("ERR=CAL_RAW_TOO_LOW"));
      return;
    }

    tdsCalibrationGain = referencePpm / latestRawTdsPpm;
    if (tdsCalibrationGain < 0.05 || tdsCalibrationGain > 20.0) {
      Serial.println(F("ERR=CAL_GAIN_OUT_OF_RANGE"));
      tdsCalibrationGain = 1.0;
      calibrationState = CAL_STATE_IDLE;
      return;
    }

    calibrationState = CAL_STATE_APPLIED_NOT_SAVED;
    Serial.print(F("CAL_APPLIED_REF="));
    Serial.println(referencePpm, 2);
    printCalibrationStatus();
    return;
  }

  if (strcmp(cmdRaw, "CAL=SAVE") == 0) {
    saveCalibrationToEeprom();
    Serial.println(F("CAL_SAVED=1"));
    printCalibrationStatus();
    return;
  }

  if (strcmp(cmdRaw, "CAL=RESET") == 0) {
    resetCalibration();
    Serial.println(F("CAL_RESET=1"));
    printCalibrationStatus();
    return;
  }

  if (strcmp(cmdRaw, "MODE?") == 0 || strcmp(cmdRaw, "STATUS") == 0) {
    Serial.print(F("MODE="));
    Serial.println(currentReadMode);
    Serial.print(F("DISP="));
    Serial.println(currentDisplayMode);
    printCalibrationStatus();
    return;
  }

  if (strcmp(cmdRaw, "DISP?") == 0) {
    Serial.print(F("DISP="));
    Serial.println(currentDisplayMode);
    return;
  }

  if (strcmp(cmdRaw, "CAL?") == 0) {
    printCalibrationStatus();
    return;
  }

  if (strcmp(cmdRaw, "HELP") == 0) {
    Serial.println(F("CMD: MODE=0..4 | DISP=0..1 | CAL=ON|OFF|APPLY:ppm|SAVE|RESET | MODE? | DISP? | CAL? | STATUS | HELP"));
    return;
  }

  Serial.println(F("ERR=CMD_UNKNOWN"));
}

void handleSerialCommands() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();

    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      if (serialCmdPos > 0) {
        serialCmdBuffer[serialCmdPos] = '\0';
        processSerialCommand(serialCmdBuffer);
        serialCmdPos = 0;
      }
      continue;
    }

    if (serialCmdPos < SERIAL_CMD_BUFFER_SIZE - 1) {
      serialCmdBuffer[serialCmdPos++] = c;
    }
  }
}

void formatFloatValue(char* out, size_t outSize, float value, uint8_t decimals, const char* suffix) {
  char number[16];
  dtostrf(value, 0, decimals, number);
  snprintf(out, outSize, "%s%s", number, suffix);
}

void drawCenteredText(const char* text, int y, uint8_t size) {
  int16_t x1, y1;
  uint16_t width, height;
  oled.setTextSize(size);
  oled.getTextBounds(text, 0, y, &x1, &y1, &width, &height);
  int16_t x = (OLED_WIDTH - (int16_t)width) / 2;
  if (x < 0) x = 0;
  oled.setCursor(x, y);
  oled.print(text);
}

void drawValuePage(const __FlashStringHelper* title, const char* value) {
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.print(title);
  drawCenteredText(value, 22, 3);
  oled.display();
}

void drawFishAnimationPage(unsigned long elapsedInPageMs) {
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.print(F("IARA"));

  int travel = OLED_WIDTH + 30;
  int fishX = OLED_WIDTH - ((int)((elapsedInPageMs * travel) / OLED_PAGE_INTERVAL_MS));
  int fishY = 34 + (((elapsedInPageMs / 150) % 2) ? 3 : -3);

  oled.fillCircle(fishX, fishY, 8, SSD1306_WHITE);
  oled.fillTriangle(fishX + 8, fishY, fishX + 16, fishY - 5, fishX + 16, fishY + 5, SSD1306_WHITE);
  oled.fillCircle(fishX - 3, fishY - 2, 1, SSD1306_BLACK);

  int bubbleX = fishX - 16;
  int bubbleY = fishY - (int)((elapsedInPageMs / 80) % 20);
  oled.drawCircle(bubbleX, bubbleY, 2, SSD1306_WHITE);
  oled.drawCircle(bubbleX - 6, bubbleY - 8, 1, SSD1306_WHITE);

  oled.display();
}

void drawWaterTransition(unsigned long elapsedTransitionMs) {
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.print(F("IARA"));

  int phase = (elapsedTransitionMs / 80) % 8;
  for (int x = 0; x < OLED_WIDTH; x += 8) {
    int offset = ((x / 8 + phase) % 2 == 0) ? 0 : 2;
    oled.drawLine(x, 16 + offset, x + 7, 16 + (2 - offset), SSD1306_WHITE);
    oled.drawLine(x, 22 + (2 - offset), x + 7, 22 + offset, SSD1306_WHITE);
  }

  int level = map((int)elapsedTransitionMs, 0, (int)OLED_TRANSITION_MS, OLED_HEIGHT + 4, 24);
  if (level < 24) level = 24;
  if (level > OLED_HEIGHT) level = OLED_HEIGHT;
  oled.fillRect(0, level, OLED_WIDTH, OLED_HEIGHT - level, SSD1306_WHITE);

  int bubbleStep = (elapsedTransitionMs / 60) % 10;
  for (int i = 0; i < 4; i++) {
    int bx = 18 + i * 28;
    int by = OLED_HEIGHT - ((bubbleStep * 3 + i * 5) % 30);
    if (by > level + 2) {
      oled.drawCircle(bx, by, 2, SSD1306_BLACK);
    }
  }

  oled.display();
}

void drawSingleScreenOverview() {
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);

  oled.setCursor(0, 0);
  oled.print(F("IARA ONE-SCREEN"));

  oled.setCursor(0, 14);
  oled.print(F("Temp: "));
  if (latestTempOk) {
    oled.print(latestTempC, 2);
    oled.print(F(" C"));
  } else {
    oled.print(F("OFF"));
  }

  oled.setCursor(0, 26);
  oled.print(F("TDS : "));
  oled.print(latestTdsPpm, 1);
  oled.print(F(" ppm"));

  oled.setCursor(0, 38);
  oled.print(F("TDS Volt.: "));
  oled.print(latestTdsVoltage, 3);
  oled.print(F(" V"));

  oled.setCursor(0, 50);
  oled.print(F("Mode "));
  oled.print(currentReadMode);
  oled.print(F(" / Disp "));
  oled.print(currentDisplayMode);

  oled.setCursor(0, 58);
  oled.print(F("CAL "));
  oled.print(calibrationState);
  oled.print(F(" G"));
  oled.print(tdsCalibrationGain, 2);

  oled.display();
}

void updateOledPresentation(unsigned long nowMs) {
  if (!isOledReady || currentReadMode == READ_MODE_OLED_DISABLED) {
    return;
  }

  if (isSensorReadInProgress && currentReadMode == READ_MODE_PAUSE_OLED_DURING_READ) {
    return;
  }

  if (currentDisplayMode == DISPLAY_MODE_SINGLE_SCREEN) {
    if (nowMs - lastOledRefreshAtMs < OLED_REFRESH_MS) {
      return;
    }
    lastOledRefreshAtMs = nowMs;

    if (!hasSensorData) {
      drawValuePage(F("AGUARDE DADOS"), "...");
      return;
    }

    drawSingleScreenOverview();
    return;
  }

  if (!isTransitionActive && (nowMs - oledPageStartedAtMs >= OLED_PAGE_INTERVAL_MS)) {
    isTransitionActive = true;
    transitionStartedAtMs = nowMs;
  }

  if (isTransitionActive && (nowMs - transitionStartedAtMs >= OLED_TRANSITION_MS)) {
    isTransitionActive = false;
    currentOledPage = (currentOledPage + 1) % OLED_PAGE_COUNT;
    oledPageStartedAtMs = nowMs;
  }

  if (nowMs - lastOledRefreshAtMs < OLED_REFRESH_MS) {
    return;
  }
  lastOledRefreshAtMs = nowMs;

  if (isTransitionActive) {
    drawWaterTransition(nowMs - transitionStartedAtMs);
    return;
  }

  if (!hasSensorData && currentOledPage != OLED_PAGE_FISH) {
    drawValuePage(F("AGUARDE DADOS"), "...");
    return;
  }

  char valueText[22];
  switch (currentOledPage) {
    case OLED_PAGE_TEMP:
      if (latestTempOk) {
        formatFloatValue(valueText, sizeof(valueText), latestTempC, 1, " C");
      } else {
        snprintf(valueText, sizeof(valueText), "SENSOR OFF");
      }
      drawValuePage(F("TEMPERATURA"), valueText);
      break;

    case OLED_PAGE_TDS_PPM:
      formatFloatValue(valueText, sizeof(valueText), latestTdsPpm, 0, " PPM");
      drawValuePage(F("TDS"), valueText);
      break;

    case OLED_PAGE_TDS_VOLTAGE:
      formatFloatValue(valueText, sizeof(valueText), latestTdsVoltage, 2, " V");
      drawValuePage(F("TDS Volt."), valueText);
      break;

    case OLED_PAGE_FISH:
      drawFishAnimationPage(nowMs - oledPageStartedAtMs);
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(800);
  temperatureSensors.begin();
  pinMode(PIN_TDS_SENSOR, INPUT);
  loadCalibrationFromEeprom();

  isOledReady = oled.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR);
  if (isOledReady) {
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(1);
    oled.setCursor(0, 0);
    oled.println(F("MaeDagua iniciando"));
    oled.setCursor(0, 12);
    oled.println(DEVICE_ID);
    oled.setCursor(0, 24);
    oled.print(F("FW "));
    oled.println(FW_VERSION);
    oled.display();

    oledPageStartedAtMs = millis();
    lastOledRefreshAtMs = 0;
  }

  Serial.println(F("MaeDagua - Arduino Uno pronto"));
  Serial.println(F("DS18B20 em D3 | TDS em A1 | OLED I2C 0x3C | USB Serial"));
  Serial.println(isOledReady ? F("OLED: OK") : F("OLED: nao detectada"));
  Serial.println(F("Use MODE=0..4 para testes"));
  Serial.println(F("Use DISP=0 rotativo | DISP=1 tela unica"));
  Serial.println(F("Use CAL=ON|APPLY:ppm|SAVE|RESET"));
  printCalibrationStatus();
}

void loop() {
  unsigned long nowMs = millis();
  handleSerialCommands();

  if (millis() - lastSendAtMs >= SEND_INTERVAL_MS) {
    lastSendAtMs = nowMs;
    packetSequence++;

    isSensorReadInProgress = true;
    if (currentReadMode == READ_MODE_DISPLAY_OFF_DURING_READ) {
      setOledPower(false);
    }

    tempSampledThisCycle = false;
    tdsSampledThisCycle = false;

    bool tempOk = false;
    float tempC = latestTempC;
    int tdsAdc = latestTdsAdc;

    bool readTempNow = true;
    bool readTdsNow = true;
    if (currentReadMode == READ_MODE_ALTERNATE_SENSORS) {
      if (!hasSensorData) {
        readTempNow = true;
        readTdsNow = true;
      } else if (nextAlternateReadIsTemp) {
        readTempNow = true;
        readTdsNow = false;
      } else {
        readTempNow = false;
        readTdsNow = true;
      }
      nextAlternateReadIsTemp = !nextAlternateReadIsTemp;
    }

    if (readTempNow) {
      tempC = readTemperatureCelsius(tempOk);
      tempSampledThisCycle = true;
    } else {
      tempOk = hasLastValidTemp;
    }

    if (readTdsNow) {
      tdsAdc = readTdsMedianAdc();
      tdsSampledThisCycle = true;
    }

    float tdsV = convertAdcToVoltage(tdsAdc);
    float rawTdsPpm = convertAdcToTdsPpm(tdsAdc);
    float tdsPpm = rawTdsPpm * tdsCalibrationGain;
    if (tdsPpm < 0) tdsPpm = 0;
    unsigned long uptimeMs = nowMs;

    if (tempOk) {
      lastValidTempC = tempC;
      hasLastValidTemp = true;
    }

    float tempToSendC = (tempOk || !hasLastValidTemp) ? tempC : lastValidTempC;
    bool tempValueAvailable = tempOk || hasLastValidTemp;

    latestTempOk = tempValueAvailable;
    latestTempC = tempToSendC;
    latestTdsAdc = tdsAdc;
    latestRawTdsPpm = rawTdsPpm;
    latestTdsVoltage = tdsV;
    latestTdsPpm = tdsPpm;
    hasSensorData = true;

    if (currentReadMode == READ_MODE_DISPLAY_OFF_DURING_READ) {
      setOledPower(true);
    }
    isSensorReadInProgress = false;

    Serial.print(F("DEVICE="));
    Serial.print(DEVICE_ID);
    Serial.print(F(";SEQ="));
    Serial.print(packetSequence);
    Serial.print(F(";TS_MS="));
    Serial.print(uptimeMs);
    Serial.print(F(";TEMP_C="));
    if (tempValueAvailable) Serial.print(tempToSendC, 2);
    else Serial.print(F("NaN"));
    Serial.print(F(";TDS_PPM="));
    Serial.print(tdsPpm, 1);
    Serial.print(F(";TDS_RAW_PPM="));
    Serial.print(rawTdsPpm, 1);
    Serial.print(F(";TDS_ADC="));
    Serial.print(tdsAdc);
    Serial.print(F(";TDS_V="));
    Serial.print(tdsV, 3);
    Serial.print(F(";TEMP_OK="));
    Serial.print(tempOk ? 1 : 0);
    Serial.print(F(";TEMP_SAMPLE="));
    Serial.print(tempSampledThisCycle ? 1 : 0);
    Serial.print(F(";TDS_SAMPLE="));
    Serial.print(tdsSampledThisCycle ? 1 : 0);
    Serial.print(F(";MODE="));
    Serial.print(currentReadMode);
    Serial.print(F(";DISP="));
    Serial.print(currentDisplayMode);
    Serial.print(F(";CAL="));
    Serial.print(calibrationState);
    Serial.print(F(";CAL_GAIN="));
    Serial.print(tdsCalibrationGain, 5);
    Serial.print(F(";TDS_OK=1;FW="));
    Serial.println(FW_VERSION);
  }

  updateOledPresentation(nowMs);
}
