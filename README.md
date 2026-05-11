# CC4102 - Tarea 1: Bulk-loading de R-trees

Implementacion en C++20 de un R-tree estatico para puntos 2D, con dos metodos
de bulk-loading:

1. **Nearest-X**
2. **Sort-Tile-Recursive (STR)**

El arbol se construye primero en RAM, se guarda como nodos binarios de 4096
bytes y las consultas leen nodos desde el archivo del arbol.

## Como ejecutar

### 1. Compilar

```powershell
cmake -S . -B build
cmake --build build
```

Tambien se puede usar:

```bash
make
```

### 2. Correr tests

```powershell
ctest --test-dir build --output-on-failure
```

Tambien se puede usar:

```bash
make test
```

### 3. Construir un arbol

```powershell
.\build\rtree_cli.exe build --dataset data\random.bin --n 100000 `
  --method nearestx --out trees\random_100000_nearestx.tree
```

Metodos validos:

- `nearestx`
- `str`

### 4. Consultar un arbol

El rectangulo se entrega como:

```text
xmin xmax ymin ymax
```

Ejemplo:

```powershell
.\build\rtree_cli.exe query --tree trees\random_100000_nearestx.tree `
  --rect 0.10 0.20 0.10 0.20
```

La salida muestra:

```text
points_found=<cantidad> io_reads=<lecturas_de_nodos>
```

### 5. Ejecutar todos los experimentos requeridos

Este comando corre la grilla completa pedida en `references/Tarea1.pdf`:

```powershell
.\scripts\run_required_experiments.ps1 -Fresh `
  -Cli .\build\rtree_cli.exe `
  -RandomDataset data\random.bin `
  -EuropaDataset data\europa.bin `
  -ResultsDir results `
  -TreesDir trees `
  -Log results\required_experiments_progress.log
```

Genera:

- `results/build_results.csv`
- `results/query_results.csv`
- `results/query_summary.csv`
- `results/system_info.txt`
- `results/build_random.png`
- `results/build_europa.png`
- `results/query_io_random.png`
- `results/query_io_europa.png`
- `results/query_points_random.png`
- `results/query_points_europa.png`
- 40 arboles en `trees/`

El script muestra progreso en PowerShell con `Write-Progress` y ademas escribe
un log textual en `results/required_experiments_progress.log`.

## Dependencias

### C++

- CMake 3.16 o superior
- Compilador con soporte C++20
- En Windows, Visual Studio Build Tools

### Python para graficos

Los graficos usan Matplotlib.

Si tienes Python instalado:

```powershell
python -m pip install -r requirements.txt
```

Si no tienes `python` disponible, usa `uv` sin instalar paquetes globales:

```powershell
uv run --python 3.12 --with matplotlib python scripts\plot_build_times.py --help
```

Para regenerar graficos manualmente:

```powershell
uv run --python 3.12 --with matplotlib python scripts\plot_build_times.py `
  --csv results\build_results.csv `
  --out-dir results

uv run --python 3.12 --with matplotlib python scripts\plot_query_summary.py `
  --csv results\query_summary.csv `
  --out-dir results
```

## Mapeo de requerimientos a codigo

| Requerimiento | Ubicacion | Estado |
|---|---|---|
| Layout de nodo de 4096 bytes | `include/types.hpp` | `static_assert(sizeof(Node) == 4096)` |
| Lectura/escritura binaria | `src/disk.cpp` | `readNode`, `writeNode`, `writeTreeToDisk` |
| Bulk-loading Nearest-X | `src/bulkload.cpp` | `buildNearestX` |
| Bulk-loading STR | `src/bulkload.cpp` | `buildSTR` |
| Consultas desde disco | `src/query.cpp` | `rangeQuery` lee nodos con `readNode` |
| CLI de construccion y busqueda | `src/main.cpp` | comandos `build` y `query` |
| Experimentos requeridos | `scripts/run_required_experiments.ps1` | grilla completa de `N` y `s` |
| Graficos | `scripts/plot_build_times.py`, `scripts/plot_query_summary.py` | PNGs desde CSV |
| Informe preliminar | `docs/informe_v0.1.md` | borrador con resultados |

## Estructura del proyecto

```text
.
├── include/                 # Interfaces publicas y structs
├── src/                     # Implementacion
├── tests/                   # Tests de I/O, bulk-loading y consultas
├── scripts/                 # Experimentos, graficos e info del sistema
├── data/                    # Datasets binarios
├── trees/                   # Arboles generados
├── results/                 # CSVs, graficos y system_info
├── docs/                    # Notas e informe preliminar
├── references/              # Enunciado y material de referencia
├── CMakeLists.txt
├── Makefile
└── README.md
```

## Archivos no versionados

El repositorio no incluye archivos generados o demasiado pesados:

- `build/`: salidas de CMake, ejecutables, librerias y archivos de debug.
- `data/*.bin`: datasets binarios completos. Se mantienen los archivos
  `*_sample.txt` como ejemplos livianos.
- `trees/*.tree`: arboles R-tree generados por los experimentos.
- `results/*`: CSVs, graficos, logs e informacion de sistema generados por
  `scripts/run_required_experiments.ps1`.

Para reproducir los resultados desde un clon limpio, copia los datasets
binarios completos en `data/`, compila el proyecto y ejecuta el script de
experimentos indicado arriba.

## Formato de resultados

### `results/build_results.csv`

```text
dataset_name,method,n_points,node_count,height,build_ms,write_ms,total_ms,tree_path
```

### `results/query_results.csv`

```text
dataset_name,method,n_points,query_id,side,xmin,xmax,ymin,ymax,io_reads,points_found,query_ms
```

### `results/query_summary.csv`

```text
dataset_name,method,n_points,side,num_queries,avg_io_reads,avg_points_found,avg_query_ms,std_io_reads,std_points_found,std_query_ms
```

## Supuestos de implementacion

- `b = 204`.
- Cada nodo mide exactamente 4096 bytes.
- La raiz siempre esta en el indice `0`.
- Las hojas guardan puntos como rectangulos degenerados.
- En hojas, `child = -1`.
- Los rectangulos se guardan como `(x1, x2, y1, y2)`.
- Las intersecciones y consultas incluyen bordes.
- El ultimo nodo de cada nivel puede tener menos de 204 entradas.
- La serializacion cruda asume `float` de 32 bits, `int32_t` de 32 bits y plataforma little-endian.
- No se implementan inserciones ni borrados individuales.
- No se implementa el bonus de `europa_bonus.bin`.

## Verificacion de correctitud

Los tests cubren:

- Tamano exacto de `Node`.
- Round-trip binario de nodos.
- Lectura de puntos desde `.bin`.
- Construccion Nearest-X.
- Construccion STR.
- Preservacion de todos los puntos en hojas.
- MBRs correctos en padres.
- Consultas comparadas contra busqueda lineal.
- Conteo de I/O en consultas selectivas.

Comando:

```powershell
ctest --test-dir build --output-on-failure
```

Resultado esperado:

```text
100% tests passed
```

## Notas utiles

- Para una ejecucion completa desde cero: compilar, correr tests y luego ejecutar `scripts/run_required_experiments.ps1`.
- Los archivos grandes generados quedan en `trees/`; se pueden borrar y regenerar con `-Fresh`.
- El README esta pensado para ejecutar el codigo.
