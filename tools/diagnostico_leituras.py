#!/usr/bin/env python3
"""
Diagnóstico de leitura para o firmware MaeDagua.

Compara estabilidade de TEMP_C e TDS_PPM em diferentes modos:
  0 = normal
  1 = pausa OLED durante leitura
  2 = display OFF durante leitura
  3 = OLED desabilitada
  4 = sensores alternados (um por ciclo)

Uso rápido:
  python3 tools/diagnostico_leituras.py --port /dev/ttyUSB0 --seconds 40
"""

import argparse
import statistics
import time
from typing import Dict, List, Tuple

import serial


MODE_NAMES = {
    0: "normal",
    1: "pausa_oled_durante_leitura",
    2: "oled_off_durante_leitura",
    3: "oled_desligada",
    4: "sensores_alternados",
}


def parse_payload(line: str) -> Dict[str, str]:
    data: Dict[str, str] = {}
    for part in line.strip().split(";"):
        if "=" in part:
            key, value = part.split("=", 1)
            data[key] = value
    return data


def as_float(value: str):
    if value is None:
        return None
    try:
        if value.upper() == "NAN":
            return None
        return float(value)
    except Exception:
        return None


def summarize(values: List[float]) -> Dict[str, float]:
    if not values:
        return {"count": 0, "min": None, "max": None, "mean": None, "stdev": None}
    return {
        "count": len(values),
        "min": min(values),
        "max": max(values),
        "mean": statistics.mean(values),
        "stdev": statistics.pstdev(values) if len(values) > 1 else 0.0,
    }


def run_mode_test(ser: serial.Serial, mode: int, seconds: int, warmup: float) -> Dict[str, object]:
    ser.reset_input_buffer()
    ser.write(f"MODE={mode}\n".encode())
    ser.flush()

    # Aguarda ACK e estabilização curta.
    t0 = time.time()
    while time.time() - t0 < warmup:
        ser.readline()

    end_at = time.time() + seconds
    raw_lines = 0
    temp_values: List[float] = []
    tds_values: List[float] = []
    temp_ok_count = 0
    temp_sample_count = 0
    tds_sample_count = 0

    while time.time() < end_at:
        line = ser.readline().decode(errors="ignore").strip()
        if not line or not line.startswith("DEVICE="):
            continue

        raw_lines += 1
        payload = parse_payload(line)

        temp = as_float(payload.get("TEMP_C"))
        tds = as_float(payload.get("TDS_PPM"))

        if temp is not None:
            temp_values.append(temp)
        if tds is not None:
            tds_values.append(tds)

        if payload.get("TEMP_OK") == "1":
            temp_ok_count += 1
        if payload.get("TEMP_SAMPLE") == "1":
            temp_sample_count += 1
        if payload.get("TDS_SAMPLE") == "1":
            tds_sample_count += 1

    return {
        "mode": mode,
        "mode_name": MODE_NAMES.get(mode, "desconhecido"),
        "lines": raw_lines,
        "temp_stats": summarize(temp_values),
        "tds_stats": summarize(tds_values),
        "temp_ok_ratio": (temp_ok_count / raw_lines) if raw_lines else 0.0,
        "temp_sample_ratio": (temp_sample_count / raw_lines) if raw_lines else 0.0,
        "tds_sample_ratio": (tds_sample_count / raw_lines) if raw_lines else 0.0,
    }


def print_result(result: Dict[str, object]):
    t = result["temp_stats"]
    d = result["tds_stats"]

    print(f"\n=== MODO {result['mode']} ({result['mode_name']}) ===")
    print(f"linhas: {result['lines']}")
    print(
        "TEMP_C -> "
        f"n={t['count']} min={t['min']} max={t['max']} media={t['mean']} desvio={t['stdev']} "
        f"temp_ok={result['temp_ok_ratio']:.2%} temp_sample={result['temp_sample_ratio']:.2%}"
    )
    print(
        "TDS_PPM -> "
        f"n={d['count']} min={d['min']} max={d['max']} media={d['mean']} desvio={d['stdev']} "
        f"tds_sample={result['tds_sample_ratio']:.2%}"
    )


def pick_recommendation(results: List[Dict[str, object]]) -> Tuple[int, str]:
    # Critério simples: menor desvio em TDS, penalizando falta de TEMP_OK.
    best_mode = None
    best_score = None

    for r in results:
        tds_stdev = r["tds_stats"]["stdev"]
        if tds_stdev is None:
            continue
        penalty = (1.0 - r["temp_ok_ratio"]) * 100.0
        score = tds_stdev + penalty
        if best_score is None or score < best_score:
            best_score = score
            best_mode = r["mode"]

    if best_mode is None:
        return -1, "Sem dados suficientes para recomendar."

    return best_mode, (
        f"Modo recomendado: {best_mode} ({MODE_NAMES.get(best_mode)}), "
        "pois apresentou melhor estabilidade de TDS com menor penalidade de TEMP_OK."
    )


def main():
    parser = argparse.ArgumentParser(description="Diagnóstico comparativo de leituras da placa.")
    parser.add_argument("--port", required=True, help="Porta serial (ex.: /dev/ttyUSB0)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (padrão 115200)")
    parser.add_argument("--seconds", type=int, default=30, help="Segundos por modo")
    parser.add_argument("--warmup", type=float, default=2.0, help="Aquecimento por modo")
    parser.add_argument(
        "--modes",
        default="0,1,2,3,4",
        help="Lista de modos separados por vírgula (ex.: 0,2,3)",
    )
    args = parser.parse_args()

    modes = [int(m.strip()) for m in args.modes.split(",") if m.strip()]

    print(f"Conectando em {args.port} @ {args.baud}...")
    with serial.Serial(args.port, args.baud, timeout=1.5) as ser:
        time.sleep(2.0)  # reset da placa
        ser.reset_input_buffer()

        results = []
        for mode in modes:
            if mode not in MODE_NAMES:
                print(f"Modo {mode} ignorado (inválido).")
                continue
            print(f"\nExecutando modo {mode} ({MODE_NAMES[mode]}) por {args.seconds}s...")
            result = run_mode_test(ser, mode, args.seconds, args.warmup)
            results.append(result)
            print_result(result)

        best_mode, recommendation = pick_recommendation(results)
        print("\n=== RECOMENDACAO ===")
        print(recommendation)
        if best_mode >= 0:
            ser.write(f"MODE={best_mode}\n".encode())
            ser.flush()
            print(f"Firmware deixado no modo recomendado: {best_mode}")


if __name__ == "__main__":
    main()
