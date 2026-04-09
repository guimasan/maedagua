# IARA V1 — Monitoramento de Qualidade da Água (Arduino Uno)

Projeto para aquisição de dados de **temperatura** e **TDS/condutividade estimada**, com transmissão serial, painel local e página pública para visualização em tempo real.

## Visão geral

O IARA V1 realiza:

- leitura de temperatura com **DS18B20**
- leitura analógica de **TDS** em `A1`
- exibição local em **OLED SSD1306 I2C (128x64)**
- envio contínuo de telemetria via **USB Serial**

O repositório inclui firmware, backend de painel e dashboard web para uso local e publicação.

## Estrutura do repositório

- `firmware/iara_arduino_uno_v1.ino` — firmware do Arduino Uno
- `_painel_iot_server/` — backend + dashboards locais
- `docs/index.html` — dashboard público (GitHub Pages)
- `docs/USO_RAPIDO.md` — guia curto de operação
- `tools/calibrar_tds.py` — calibração rápida via serial
- `tools/diagnostico_leituras.py` — diagnóstico comparativo de modos

## Arquitetura de dados

1. O Arduino coleta dados dos sensores.
2. A telemetria é enviada pela serial (`115200`).
3. O bridge (`_painel_iot_server/bridge_arduino.py`) converte e publica para a API.
4. Dashboards consomem API/WS para visualização em tempo real e histórico.

Exemplo de payload serial:

```text
DEVICE=iara-uno-0001;SEQ=1;TS_MS=1034;TEMP_C=25.19;TDS_PPM=131;TDS_ADC=287;TDS_V=1.404;TEMP_OK=1;TDS_OK=1;FW=1.0.0
```

## Equipamentos e sensores utilizados

- **Microcontrolador:** Arduino Uno (ATmega328P)
- **Temperatura:** DS18B20 (OneWire)
- **TDS/EC:** módulo TDS analógico compatível com entrada ADC do Uno
- **Display:** OLED SSD1306 I2C 128x64 (`0x3C`)
- **Conexão host:** USB serial

## Ligações elétricas (padrão do projeto)

- **DS18B20:** `D3`, `5V`, `GND`
- **TDS analógico:** `A1`, `5V`, `GND`
- **OLED SSD1306:** `SDA(A4)`, `SCL(A5)`, `5V`, `GND`

## Medidas, faixas e interpretação

### Temperatura (DS18B20)

- resolução configurável pelo sensor: 9–12 bits
- precisão típica de referência: aproximadamente $\pm 0.5\,^{\circ}C$ na faixa central de operação (ver datasheet)
- saída no projeto: `TEMP_C`

### TDS/EC (módulo analógico)

- leitura bruta no ADC de 10 bits do Uno (`0..1023`): `TDS_ADC`
- tensão estimada: `TDS_V`
- concentração estimada: `TDS_PPM`
- valor bruto antes de ganho de calibração: `TDS_RAW_PPM`

> Observação: TDS depende de temperatura, tipo de sonda, circuito e solução de referência. Tratar os valores como **medição estimada**, com calibração periódica.

## Calibração TDS

Campos publicados:

- `CAL` — estado da calibração
- `CAL_GAIN` — ganho aplicado
- `TDS_RAW_PPM` — base sem ganho

Estados de `CAL`:

- `0`: idle
- `1`: aguardando referência
- `2`: aplicado e não salvo
- `3`: salvo em EEPROM

Comandos serial:

- `CAL=ON`
- `CAL=APPLY:<ppm>`
- `CAL=SAVE`
- `CAL=RESET`
- `CAL=OFF`
- `CAL?`

Referência comum de condutividade:

- solução: **1413 µS/cm**
- escala 500: $ppm = EC \times 0.5$
- resultado: $1413 \times 0.5 = 706.5\,ppm$

Aplicação típica: `CAL=APPLY:706.5` seguido de `CAL=SAVE`.

## Modos de diagnóstico de leitura

Comandos para avaliar impacto de OLED e sequência de amostragem:

- `MODE=0` normal
- `MODE=1` pausa OLED durante leitura
- `MODE=2` OLED desligada durante leitura
- `MODE=3` OLED sempre desligada
- `MODE=4` leitura alternada por sensor
- consulta: `MODE?` ou `STATUS`

## Bibliotecas Arduino

- `OneWire`
- `DallasTemperature`
- `Adafruit GFX Library`
- `Adafruit SSD1306`

## Operação rápida

1. Gravar `firmware/iara_arduino_uno_v1.ino` no Arduino Uno.
2. Abrir monitor serial em `115200`.
3. Validar chegada de telemetria.
4. Opcional: executar scripts em `tools/` para diagnóstico/calibração.
5. Subir backend em `_painel_iot_server` para dashboard local e integração web.

## Dashboard público (GitHub Pages)

`docs/index.html` fornece:

- visualização em tempo real
- histórico cronológico
- detecção automática de variáveis
- seleção de endpoint da API

Para publicar:

1. Ativar GitHub Pages apontando para `main/docs`.
2. Abrir a página publicada.
3. Configurar a URL da API (preferencialmente HTTPS).

## Boas práticas de medição

- Aguarde estabilização térmica da amostra antes de registrar.
- Evite encostar a sonda nas paredes do recipiente.
- Faça agitação leve e padronizada da amostra.
- Registre temperatura junto com TDS para comparação entre coletas.
- Recalibre periodicamente com solução padrão conhecida.

## Referências técnicas

- Arduino Uno Rev3: https://docs.arduino.cc/hardware/uno-rev3
- ATmega328P (microcontrolador do Uno): https://www.microchip.com/en-us/product/ATmega328P
- DS18B20 (Maxim/Analog): https://www.analog.com/en/products/ds18b20.html
- Biblioteca DallasTemperature: https://github.com/milesburton/Arduino-Temperature-Control-Library
- Biblioteca OneWire (Paul Stoffregen): https://github.com/PaulStoffregen/OneWire
- Adafruit SSD1306: https://github.com/adafruit/Adafruit_SSD1306
- Adafruit GFX: https://github.com/adafruit/Adafruit-GFX-Library
