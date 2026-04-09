# MaeDagua - Arduino Uno

Projeto simples e fácil de usar.

## O que ele faz

- Lê temperatura com DS18B20 no pino D3
- Lê TDS no pino A1
- Mostra status em OLED I2C 128x64 (0x3C)
- Envia tudo pela USB Serial

## O que faz no notebook

O Arduino só manda os dados.
O seu webapp no notebook pode ler a serial, sincronizar e guardar os logs.

## Arquivos

- `firmware/iara_arduino_uno_v1.ino` → código do Arduino
- `docs/USO_RAPIDO.md` → passo a passo curto
- `README.md` → visão geral

## Como usar

1. Abra `firmware/iara_arduino_uno_v1.ino`
2. Envie para o Arduino Uno
3. Abra o monitor serial em `115200`
4. Veja as linhas chegando
5. Conecte seu webapp para ler a serial

## Modos de diagnóstico de leitura (Serial)

O firmware aceita comandos por Serial para testar interferência da OLED:

- `MODE=0` normal
- `MODE=1` pausa atualização OLED durante leitura
- `MODE=2` OLED desligada durante leitura
- `MODE=3` OLED desligada sempre
- `MODE=4` leitura alternada de sensores (temp/tds)

Consulta de modo:

- `MODE?` ou `STATUS`

## Calibração TDS (CAL)

O firmware agora publica as variáveis de calibração em cada linha:

- `CAL` (estado da calibração)
- `CAL_GAIN` (ganho aplicado)
- `TDS_RAW_PPM` (valor antes da calibração)

Estados de `CAL`:

- `0` idle
- `1` aguardando referência
- `2` aplicado e não salvo
- `3` salvo em EEPROM

Comandos:

- `CAL=ON`
- `CAL=APPLY:<ppm>` (ex.: `CAL=APPLY:342`)
- `CAL=SAVE`
- `CAL=RESET`
- `CAL=OFF`
- `CAL?`

Também disponível no payload: `DISP` (0 rotativo, 1 tela única).

### Solução de referência 1413 µS/cm

Para 1413 µS/cm, no padrão 500-scale ($ppm = EC \times 0.5$):

- $1413 \times 0.5 = 706.5\,ppm$

Ou seja, a calibração recomendada é `CAL=APPLY:706.5`.

No webapp refatorado, isso já aparece com preset de 1413 µS/cm e fator selecionável (0.5, 0.64, 0.7).

## Script de comparação automática

Arquivo: `tools/diagnostico_leituras.py`

Ele testa os modos e mostra estatísticas de estabilidade de `TEMP_C` e `TDS_PPM`, indicando um modo recomendado.

## Script de calibração rápida

Arquivo: `tools/calibrar_tds.py`

Exemplo:

- `python3 tools/calibrar_tds.py --port /dev/ttyUSB0 --ref 342 --save`

## Exemplo de linha

```text
DEVICE=iara-uno-0001;SEQ=1;TS_MS=1034;TEMP_C=25.19;TDS_PPM=131;TDS_ADC=287;TDS_V=1.404;TEMP_OK=1;TDS_OK=1;FW=1.0.0
```

## Ligações

- **DS18B20**: D3, GND, 5V
- **TDS**: A1, GND, 5V
- **OLED SSD1306 I2C**: SDA (A4), SCL (A5), GND, 5V

## Bibliotecas Arduino

- `OneWire`
- `DallasTemperature`
- `Adafruit GFX Library`
- `Adafruit SSD1306`

## Pronto

Sem servidor. Sem painel. Só Arduino enviando dados por USB.

## Página pública do projeto

A pasta `docs/` contém uma página pronta para GitHub Pages com:

- leitura em tempo real
- seleção do endpoint da API
- histórico cronológico dos envios do iara
- cards com todas as variáveis detectadas automaticamente

Para publicar no GitHub Pages:

1. Ative Pages apontando para a pasta `docs/`
2. Abra `docs/index.html`
3. Informe a URL HTTPS da API do painel no campo "API base"

Exemplo de uso local:

- `http://127.0.0.1:8000`

Exemplo para página publicada:

- `https://seu-servidor.com`

Se a página estiver no GitHub Pages, a API também precisa estar acessível por HTTPS para o navegador permitir a leitura em tempo real.
