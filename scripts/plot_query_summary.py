#!/usr/bin/env python3
"""Grafica resumenes de consultas R-tree desde results/query_summary.csv."""

from __future__ import annotations

import argparse
import csv
import matplotlib
import matplotlib.pyplot as plt
from collections import defaultdict
from pathlib import Path
from typing import Dict, List, Optional, Tuple


def parse_args() -> argparse.Namespace:
    # Los valores por defecto coinciden con el CSV de rtree_cli experiment-query.
    parser = argparse.ArgumentParser(description="Grafica resumenes de consultas R-tree.")
    parser.add_argument(
        "--csv",
        default="results/query_summary.csv",
        help="CSV de entrada producido por rtree_cli experiment-query.",
    )
    parser.add_argument(
        "--out-dir",
        default="results",
        help="Directorio donde se escribiran los graficos PNG.",
    )
    return parser.parse_args()


def read_rows(path: Path) -> Dict[Tuple[str, str], List[Tuple[float, float, float, float]]]:
    # Guarda lado, I/O promedio, puntos promedio y desviacion para cada curva.
    data: Dict[Tuple[str, str], List[Tuple[float, float, float, float]]] = defaultdict(list)
    with path.open(newline="") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            side = float(row["side"])
            avg_io = float(row["avg_io_reads"])
            avg_points = float(row["avg_points_found"])
            std_points = float(row.get("std_points_found") or 0.0)
            key = (row["dataset_name"], row["method"])
            data[key].append((side, avg_io, avg_points, std_points))

    for values in data.values():
        values.sort()
    return data


def plot_metric(
    data,
    out_dir: Path,
    metric_index: int,
    ylabel: str,
    suffix: str,
    error_index: Optional[int] = None,
) -> None:

    datasets = sorted({dataset for dataset, _method in data})
    for dataset_name in datasets:
        plt.figure()
        for method in ("nearestx", "str"):
            key = (dataset_name, method)
            if key not in data:
                continue
            xs = [item[0] for item in data[key]]
            ys = [item[metric_index] for item in data[key]]
            if error_index is None:
                plt.plot(xs, ys, marker="o", label=method)
            else:
                # El enunciado pide barras de error en el grafico de puntos.
                yerr = [item[error_index] for item in data[key]]
                plt.errorbar(xs, ys, yerr=yerr, marker="o", capsize=4, label=method)
        plt.xlabel("Lado del cuadrado")
        plt.ylabel(ylabel)
        plt.title(f"{ylabel} - {dataset_name}")
        plt.legend()
        plt.tight_layout()
        out_path = out_dir / f"query_{suffix}_{dataset_name}.png"
        plt.savefig(out_path)
        plt.close()
        # Imprime las rutas generadas para que los scripts puedan registrarlas.
        print(out_path)


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
    # Grafica lecturas promedio de nodos sin barras de error.
    plot_metric(data, out_dir, 1, "Promedio de lecturas de nodo", "io")
    # Grafica puntos promedio encontrados con barras de desviacion estandar.
    plot_metric(
        data,
        out_dir,
        2,
        "Promedio de puntos reportados",
        "points",
        error_index=3,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
