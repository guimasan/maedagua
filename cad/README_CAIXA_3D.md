# Caixa 3D — IARA V1

Este diretório contém um modelo paramétrico em OpenSCAD para a caixa do sistema IARA V1.

## Arquivo

- `caixa_iara_v1.scad`

## Componentes previstos

- Arduino Uno
- Módulo TDS Keyestudio v1.0 (dimensões aproximadas)
- OLED 0.96" I2C
- Saída lateral para cabo do TDS
- Saída lateral para cabo do sensor de temperatura
- Recortes para USB-B e jack DC do Uno

## Saída do display (OLED) — revisão aplicada

A janela do display fica na **tampa superior** e agora é totalmente ajustável por parâmetros:

- `oled_margin_top` → distância da borda superior da tampa até o centro da janela
- `oled_offset_x` → deslocamento horizontal da janela

Também foram incluídos:

- postes internos para fixação do OLED (`lid_oled_posts`)
- canal para passagem do cabo do OLED (`oled_cable_notch_*`)

Se quiser mover a janela para alinhar exatamente ao seu módulo, ajuste esses dois parâmetros e gere apenas a tampa (`part = "lid"`) para teste rápido.

## Como gerar STL

No OpenSCAD:

1. Abra `caixa_iara_v1.scad`
2. Ajuste `part` para:
   - `"base"` para exportar a base
   - `"lid"` para exportar a tampa
3. Renderize (`F6`)
4. Exporte STL

## Configuração inicial de impressão (FDM)

- Bico: 0.4 mm
- Altura de camada: 0.2 mm
- Paredes: 3 linhas
- Infill: 20–30%
- Material sugerido:
  - PLA para protótipo rápido
  - PETG para uso em ambiente úmido/externo
- Suportes: normalmente não necessários

## Parafusos recomendados

- Fixação de placas: parafuso M3 (autoatarraxante leve ou porca M3, conforme ajuste)
- Fechamento da tampa: M3 x 8 a M3 x 12

## Ajustes esperados (importante)

As dimensões do módulo TDS e do OLED podem variar entre lotes/fabricantes. Recomenda-se:

1. Medir sua placa real com paquímetro
2. Ajustar no SCAD os parâmetros:
   - `tds_x`, `tds_y`, `tds_holes`
   - `oled_*` (janela, furos, postes e canal de cabo)
3. Imprimir um teste rápido da tampa para validar a janela do OLED

## Observação

O modelo foi feito para ser fácil de ajustar em campo e prioriza montagem prática.
