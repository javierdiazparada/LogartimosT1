#include "experiments.hpp"

#include "bulkload.hpp"
#include "disk.hpp"
#include "geometry.hpp"
#include "query.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ios>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace rtree {

namespace {

constexpr std::uint64_t kQuerySeed = 0x5eed1234ULL;

/**
 * @brief Convierte puntos en entradas hoja para iniciar el bulk-loading
 * @param points Puntos leidos desde un dataset binario
 * @return Entradas con MBR degenerado y child igual a -1
 */
std::vector<Entry> makeLeafEntries(const std::vector<Point> &points) {
  std::vector<Entry> entries;
  entries.reserve(points.size());
  for (const Point &point : points) {
    entries.push_back(makeLeafEntry(point));
  }
  return entries;
}

/**
 * @brief Despacha la construccion al metodo de bulk-loading seleccionado
 * @param entries Entradas iniciales del arbol
 * @param method Metodo de construccion solicitado
 * @return Arbol construido en RAM
 */
BuildResult buildFromEntries(std::vector<Entry> entries, BuildMethod method) {
  switch (method) {
  case BuildMethod::NearestX:
    return buildNearestX(std::move(entries));
  case BuildMethod::STR:
    return buildSTR(std::move(entries));
  }
  throw std::runtime_error("unknown build method");
}

/**
 * @brief Calcula milisegundos transcurridos entre dos instantes
 * @param begin Inicio de la medicion
 * @param end Fin de la medicion
 * @return Duracion en milisegundos
 */
double elapsedMilliseconds(std::chrono::steady_clock::time_point begin,
                           std::chrono::steady_clock::time_point end) {
  return std::chrono::duration<double, std::milli>(end - begin).count();
}

/**
 * @brief Crea el directorio padre de una ruta si existe uno
 * @param path Ruta cuyo directorio padre debe existir
 */
void ensureParentDirectory(const std::filesystem::path &path) {
  const auto parent = path.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent);
  }
}

/**
 * @brief Escapa un valor para escribirlo de forma segura en CSV
 * @param value Texto original
 * @return Texto escapado si contiene comas, comillas o saltos de linea
 */
std::string csvEscape(const std::string &value) {
  if (value.find_first_of(",\"\n\r") == std::string::npos) {
    return value;
  }

  std::string escaped = "\"";
  for (const char ch : value) {
    if (ch == '"') {
      escaped += "\"\"";
    } else {
      escaped += ch;
    }
  }
  escaped += '"';
  return escaped;
}

/**
 * @brief Determina si un CSV necesita que se escriba su header
 * @param path Ruta del CSV
 * @return true si el archivo no existe o esta vacio
 */
bool needsHeader(const std::filesystem::path &path) {
  return !std::filesystem::exists(path) || std::filesystem::file_size(path) == 0;
}

/**
 * @brief Abre un CSV en modo append y escribe header si corresponde
 * @param path Ruta del CSV
 * @param header Primera linea esperada del CSV
 * @return Stream de salida listo para escribir filas
 */
std::ofstream openCsvAppend(const std::filesystem::path &path,
                            const std::string &header) {
  ensureParentDirectory(path);
  const bool writeHeader = needsHeader(path);
  std::ofstream out(path, std::ios::app);
  if (!out) {
    throw std::runtime_error("cannot open csv for append: " + path.string());
  }
  if (writeHeader) {
    out << header << '\n';
  }
  return out;
}

/**
 * @brief Genera un cuadrado aleatorio dentro de [0,1]x[0,1]
 * @param rng Generador pseudoaleatorio con semilla fija
 * @param side Largo del lado del cuadrado
 * @return Rectangulo de consulta en formato x1,x2,y1,y2
 */
Rect makeSquareQuery(std::mt19937_64 &rng, float side) {
  if (side < 0.0f || side > 1.0f) {
    throw std::runtime_error("query side must be in [0,1]");
  }
  std::uniform_real_distribution<float> dist(0.0f, 1.0f - side);
  const float x = dist(rng);
  const float y = dist(rng);
  return Rect{x, x + side, y, y + side};
}

/**
 * @brief Crea la ruta por defecto para un arbol de experimento
 * @param options Opciones de construccion
 * @return Ruta bajo el directorio trees
 */
std::filesystem::path defaultTreePath(const BuildExperimentOptions &options) {
  return std::filesystem::path("trees") /
         (options.datasetName + "_" + buildMethodName(options.method) + "_" +
          std::to_string(options.n) + ".tree");
}

/**
 * @brief Cuenta puntos recorriendo recursivamente nodos de un arbol en disco
 * @param input Archivo binario del arbol abierto para lectura
 * @param node Nodo actual ya leido
 * @param ioCounter Contador local de lecturas de nodos
 * @return Cantidad de puntos bajo el nodo actual
 */
std::uint64_t countPointsInNode(std::ifstream &input, const Node &node,
                                std::int64_t &ioCounter) {
  std::uint64_t total = 0;
  for (std::int32_t i = 0; i < node.k; ++i) {
    const Entry &entry = node.children[static_cast<std::size_t>(i)];
    if (entry.child == kLeafChild) {
      ++total;
    } else {
      const Node child = readNode(input, entry.child, ioCounter);
      total += countPointsInNode(input, child, ioCounter);
    }
  }
  return total;
}

/**
 * @brief Cuenta los puntos guardados en un arbol serializado
 * @param treePath Ruta del archivo binario del arbol
 * @return Cantidad total de puntos en las hojas
 */
std::uint64_t countPointsInTree(const std::filesystem::path &treePath) {
  std::ifstream input(treePath, std::ios::binary);
  if (!input) {
    throw std::runtime_error("cannot open tree file for binary read: " +
                             treePath.string());
  }
  std::int64_t ioCounter = 0;
  const Node root = readNode(input, kRootIndex, ioCounter);
  return countPointsInNode(input, root, ioCounter);
}

/**
 * @brief Calcula el promedio de un vector de valores
 * @param values Valores numericos
 * @return Promedio, o 0 si el vector esta vacio
 */
double average(const std::vector<double> &values) {
  if (values.empty()) {
    return 0.0;
  }
  double total = 0.0;
  for (const double value : values) {
    total += value;
  }
  return total / static_cast<double>(values.size());
}

/**
 * @brief Calcula la desviacion estandar poblacional de un vector
 * @param values Valores numericos
 * @param avg Promedio previamente calculado
 * @return Desviacion estandar, o 0 si no hay datos
 */
double standardDeviation(const std::vector<double> &values, double avg) {
  if (values.empty()) {
    return 0.0;
  }
  double squared = 0.0;
  for (const double value : values) {
    const double delta = value - avg;
    squared += delta * delta;
  }
  return std::sqrt(squared / static_cast<double>(values.size()));
}

} // namespace

/**
 * @brief Convierte el nombre textual de un metodo a enum
 * @param name Texto recibido desde CLI o scripts
 * @return Metodo de bulk-loading correspondiente
 */
BuildMethod parseBuildMethod(const std::string &name) {
  if (name == "nearestx") {
    return BuildMethod::NearestX;
  }
  if (name == "str") {
    return BuildMethod::STR;
  }
  throw std::runtime_error("unknown build method: " + name);
}

/**
 * @brief Convierte un metodo de bulk-loading a texto
 * @param method Metodo a convertir
 * @return Nombre usado en CLI y CSV
 */
std::string buildMethodName(BuildMethod method) {
  switch (method) {
  case BuildMethod::NearestX:
    return "nearestx";
  case BuildMethod::STR:
    return "str";
  }
  throw std::runtime_error("unknown build method");
}

/**
 * @brief Construye un arbol desde los primeros n puntos de un dataset
 * @param dataset Ruta del archivo binario de puntos
 * @param n Cantidad de puntos a leer; 0 lee todo el archivo
 * @param method Metodo de bulk-loading a usar
 * @return Arbol construido en RAM
 */
BuildResult buildTreeFromDataset(const std::filesystem::path &dataset,
                                  std::size_t n, BuildMethod method) {
  std::vector<Point> points = readPoints(dataset, n);
  return buildFromEntries(makeLeafEntries(points), method);
}

/**
 * @brief Ejecuta y registra un experimento de construccion
 * @param options Opciones con dataset, metodo, N, CSV y salida del arbol
 */
void runBuildExperiment(const BuildExperimentOptions &options) {
  std::vector<Point> points = readPoints(options.dataset, options.n);
  std::vector<Entry> entries = makeLeafEntries(points);
  const std::filesystem::path treePath =
      options.treeOut.empty() ? defaultTreePath(options) : options.treeOut;

  // Se mide por separado la construccion en RAM y la escritura a disco.
  const auto begin = std::chrono::steady_clock::now();
  const BuildResult build = buildFromEntries(std::move(entries), options.method);
  const auto buildEnd = std::chrono::steady_clock::now();
  ensureParentDirectory(treePath);
  writeTreeToDisk(treePath, build.nodes);
  const auto writeEnd = std::chrono::steady_clock::now();

  const double buildMs = elapsedMilliseconds(begin, buildEnd);
  const double writeMs = elapsedMilliseconds(buildEnd, writeEnd);

  auto csv = openCsvAppend(
      options.csv,
      "dataset_name,method,n_points,node_count,height,build_ms,write_ms,"
      "total_ms,tree_path");
  csv << csvEscape(options.datasetName) << ','
      << buildMethodName(options.method) << ',' << points.size() << ','
      << build.nodes.size() << ',' << build.height << ','
      << std::fixed << std::setprecision(3) << buildMs << ',' << writeMs << ','
      << (buildMs + writeMs) << ',' << csvEscape(treePath.string()) << '\n';
}

/**
 * @brief Ejecuta y registra un conjunto de consultas aleatorias
 * @param options Opciones con arbol, dataset, cantidad de queries y CSVs
 */
void runQueryExperiment(const QueryExperimentOptions &options) {
  if (!std::filesystem::exists(options.dataset)) {
    throw std::runtime_error("dataset path does not exist: " +
                             options.dataset.string());
  }

  const std::uint64_t treePoints = countPointsInTree(options.tree);
  auto csv = openCsvAppend(
      options.csv,
      "dataset_name,method,n_points,query_id,side,xmin,xmax,ymin,ymax,"
      "io_reads,points_found,query_ms");

  std::mt19937_64 rng(kQuerySeed);
  std::vector<double> ioReadsValues;
  std::vector<double> pointsFoundValues;
  std::vector<double> queryMsValues;
  ioReadsValues.reserve(options.queries);
  pointsFoundValues.reserve(options.queries);
  queryMsValues.reserve(options.queries);

  for (std::size_t i = 0; i < options.queries; ++i) {
    const Rect rect = makeSquareQuery(rng, options.side);
    std::int64_t ioReads = 0;
    const auto begin = std::chrono::steady_clock::now();
    const std::vector<Point> points = rangeQuery(options.tree, rect, ioReads);
    const auto end = std::chrono::steady_clock::now();
    const double queryMs = elapsedMilliseconds(begin, end);

    ioReadsValues.push_back(static_cast<double>(ioReads));
    pointsFoundValues.push_back(static_cast<double>(points.size()));
    queryMsValues.push_back(queryMs);

    csv << csvEscape(options.datasetName) << ','
        << buildMethodName(options.method) << ',' << treePoints << ',' << i
        << ',' << options.side << ',' << rect.x1 << ',' << rect.x2 << ','
        << rect.y1 << ',' << rect.y2 << ',' << ioReads << ','
        << points.size() << ',' << std::fixed << std::setprecision(3)
        << queryMs << '\n';
  }

  // El resumen agrega los promedios y desviaciones pedidos por el informe.
  const double avgIoReads = average(ioReadsValues);
  const double avgPointsFound = average(pointsFoundValues);
  const double avgQueryMs = average(queryMsValues);

  auto summary = openCsvAppend(options.summary,
                               "dataset_name,method,n_points,side,num_queries,"
                               "avg_io_reads,avg_points_found,avg_query_ms,"
                               "std_io_reads,std_points_found,std_query_ms");
  summary << csvEscape(options.datasetName) << ','
          << buildMethodName(options.method) << ',' << treePoints << ','
          << options.side << ',' << options.queries << ',' << std::fixed
          << std::setprecision(3) << avgIoReads << ',' << avgPointsFound << ','
          << avgQueryMs << ',' << standardDeviation(ioReadsValues, avgIoReads)
          << ',' << standardDeviation(pointsFoundValues, avgPointsFound) << ','
          << standardDeviation(queryMsValues, avgQueryMs) << '\n';
}

} // namespace rtree
