# Uso rápido

## Ligar

- DS18B20 no pino D3
- TDS no pino A1
- OLED I2C 128x64 em SDA(A4) e SCL(A5)
- Arduino Uno no USB

## Bibliotecas necessárias

- OneWire
- DallasTemperature
- Adafruit GFX Library
- Adafruit SSD1306

## Enviar o código

Abra:

- `firmware/iara_arduino_uno_v1.ino`

Depois envie para a placa.

## Ver os dados

Abra o monitor serial em:

- `115200`

Exemplo:

```text
DEVICE=iara-uno-0001;SEQ=1;TS_MS=1034;TEMP_C=25.19;TDS_PPM=131;TDS_ADC=287;TDS_V=1.404;TEMP_OK=1;TDS_OK=1;FW=1.0.0
```

Agora o payload inclui também:

- `TDS_RAW_PPM`
- `CAL`
- `CAL_GAIN`
- `DISP`

## Calibração rápida no Monitor Serial

1. Abra o Monitor Serial em `115200`
2. Envie `CAL=ON`
3. Com a sonda em solução padrão, envie o valor equivalente em ppm.
   - Exemplo para **1413 µS/cm** no padrão 500-scale: `CAL=APPLY:706.5`
4. Se estiver bom, envie `CAL=SAVE`
5. Para conferir, envie `CAL?`

## O que o notebook faz

Seu webapp pode:

- ler a porta serial
- sincronizar os dados
- salvar logs
- mostrar histórico
- fazer gráficos e alertas
