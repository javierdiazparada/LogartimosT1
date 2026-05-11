#!/usr/bin/env python3
"""Grafica tiempos de construccion de R-tree desde results/build_results.csv."""

from __future__ import annotations

import argparse
import csv
import matplotlib
import matplotlib.pyplot as plt
from collections import defaultdict
from pathlib import Path
from typing import Dict, List, Tuple


def parse_args() -> argparse.Namespace:
    # Los valores por defecto coinciden con el CSV del runner de experimentos.
    parser = argparse.ArgumentParser(description="Grafica tiempos de construccion de R-tree.")
    parser.add_argument(
        "--csv",
        default="results/build_results.csv",
        help="CSV de entrada producido por rtree_cli experiment-build.",
    )
    parser.add_argument(
        "--out-dir",
        default="results",
        help="Directorio donde se escribiran los graficos PNG.",
    )
    return parser.parse_args()


def read_rows(path: Path) -> Dict[Tuple[str, str], List[Tuple[int, float]]]:
    # Agrupa filas por dataset y metodo para que cada par sea una curva.
    data: Dict[Tuple[str, str], List[Tuple[int, float]]] = defaultdict(list)
    with path.open(newline="") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            # CSVs antiguos de prueba usaban "n" y solo "build_ms".
            n_points = int(row.get("n_points") or row.get("n") or 0)
            total_ms = float(row.get("total_ms") or row.get("build_ms") or 0.0)
            key = (row["dataset_name"], row["method"])
            data[key].append((n_points, total_ms))

    for values in data.values():
        values.sort()
    return data


def main() -> int:
    args = parse_args()
    csv_path = Path(args.csv)
    if not csv_path.exists():
        raise SystemExit(f"no existe el CSV de entrada: {csv_path}")

    data = read_rows(csv_path)
    if not data:
        raise SystemExit(f"el CSV de entrada no tiene filas: {csv_path}")

    # Agg renderiza PNGs sin abrir una ventana grafica.
    matplotlib.use("Agg")


    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    datasets = sorted({dataset for dataset, _method in data})
    for dataset_name in datasets:
        plt.figure()
        for method in ("nearestx", "str"):
            key = (dataset_name, method)
            if key not in data:
                continue
            xs = [item[0] for item in data[key]]
            ys = [item[1] for item in data[key]]
            plt.plot(xs, ys, marker="o", label=method)
        # Los N pedidos son potencias de dos, por eso el eje log2 es mas legible.
        plt.xscale("log", base=2)
        plt.xlabel("N puntos")
        plt.ylabel("Tiempo total build+write (ms)")
        plt.title(f"Construccion {dataset_name}")
        plt.legend()
        plt.tight_layout()
        out_path = out_dir / f"build_{dataset_name}.png"
        plt.savefig(out_path)
        plt.close()
        # Imprime las rutas generadas para que los scripts puedan registrarlas.
        print(out_path)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
