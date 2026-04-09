#!/usr/bin/env python3
"""
Assistente de calibração TDS para firmware IARA.

Fluxo:
1) Coloque a sonda em solução padrão conhecida (ex.: 342 ppm).
2) Rode este script com --ref 342.
3) O script envia CAL=ON, lê alguns segundos e aplica CAL=APPLY:<ref>.
4) Se --save estiver ativo, envia CAL=SAVE.

Exemplo:
  python3 tools/calibrar_tds.py --port /dev/ttyUSB0 --ref 342 --save
"""

import argparse
import time
import serial


def parse_line(line: str):
    data = {}
    for part in line.strip().split(";"):
        if "=" in part:
            k, v = part.split("=", 1)
            data[k] = v
    return data


def main():
    parser = argparse.ArgumentParser(description="Calibração TDS via Serial")
    parser.add_argument("--port", required=True, help="Porta serial (ex.: /dev/ttyUSB0)")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--ref", type=float, required=True, help="PPM de referência da solução padrão")
    parser.add_argument("--sample-seconds", type=int, default=10, help="Tempo de observação antes do APPLY")
    parser.add_argument("--save", action="store_true", help="Salvar calibração na EEPROM")
    args = parser.parse_args()

    with serial.Serial(args.port, args.baud, timeout=1.2) as ser:
        time.sleep(2)
        ser.reset_input_buffer()

        ser.write(b"CAL=ON\n")
        ser.flush()
        print("CAL=ON enviado.")

        end = time.time() + args.sample_seconds
        print("Aguardando amostras...")
        last = None
        while time.time() < end:
            line = ser.readline().decode(errors="ignore").strip()
            if not line.startswith("DEVICE="):
                continue
            payload = parse_line(line)
            raw = payload.get("TDS_RAW_PPM")
            corr = payload.get("TDS_PPM")
            print(f"RAW={raw} | CORR={corr} | CAL={payload.get('CAL')} | G={payload.get('CAL_GAIN')}")
            last = payload

        if not last:
            print("Sem dados de DEVICE recebidos. Abortando.")
            return

        cmd = f"CAL=APPLY:{args.ref:.2f}\n".encode()
        ser.write(cmd)
        ser.flush()
        print(f"Enviado: {cmd.decode().strip()}")

        time.sleep(1.0)
        ser.write(b"CAL?\n")
        ser.flush()

        for _ in range(8):
            line = ser.readline().decode(errors="ignore").strip()
            if line:
                print(line)

        if args.save:
            ser.write(b"CAL=SAVE\n")
            ser.flush()
            time.sleep(0.6)
            ser.write(b"CAL?\n")
            ser.flush()
            print("CAL=SAVE enviado.")
            for _ in range(8):
                line = ser.readline().decode(errors="ignore").strip()
                if line:
                    print(line)


if __name__ == "__main__":
    main()
