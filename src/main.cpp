#include "bulkload.hpp"
#include "disk.hpp"
#include "experiments.hpp"
#include "geometry.hpp"
#include "query.hpp"
#include "types.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

/**
 * @brief Imprime la ayuda de uso del ejecutable de linea de comandos
 */
void usage() {
  std::cerr
      << "Usage:\n"
      << "  rtree_cli build --dataset PATH --n N --method nearestx|str --out "
         "TREE.bin\n"
      << "  rtree_cli query --tree TREE.bin --rect xmin xmax ymin ymax\n"
      << "  rtree_cli experiment-build --dataset PATH --dataset-name "
         "random|europa --method nearestx|str --n N --csv "
         "results/build_results.csv [--out TREE.bin]\n"
      << "  rtree_cli experiment-query --dataset PATH --tree TREE.bin "
         "--dataset-name random|europa --method nearestx|str --queries Q "
         "--side S --csv results/query_results.csv --summary "
         "results/query_summary.csv\n";
}

/**
 * @brief Obtiene el valor de una opcion obligatoria
 * @param argc Cantidad de argumentos de CLI
 * @param argv Argumentos de CLI
 * @param name Nombre de la opcion buscada
 * @return Valor textual asociado a la opcion
 */
std::string requireOption(int argc, char **argv, const std::string &name) {
  for (int i = 2; i < argc; ++i) {
    if (argv[i] == name) {
      if (i + 1 >= argc) {
        throw std::runtime_error("missing value for " + name);
      }
      return argv[i + 1];
    }
  }
  throw std::runtime_error("missing required option " + name);
}

/**
 * @brief Verifica si una opcion aparece en la linea de comandos
 * @param argc Cantidad de argumentos de CLI
 * @param argv Argumentos de CLI
 * @param name Nombre de la opcion buscada
 * @return true si la opcion esta presente
 */
bool hasOption(int argc, char **argv, const std::string &name) {
  for (int i = 2; i < argc; ++i) {
    if (argv[i] == name) {
      return true;
    }
  }
  return false;
}

/**
 * @brief Obtiene el valor de una opcion opcional
 * @param argc Cantidad de argumentos de CLI
 * @param argv Argumentos de CLI
 * @param name Nombre de la opcion buscada
 * @param fallback Valor a retornar si la opcion no existe
 * @return Valor de la opcion o fallback
 */
std::string optionalOption(int argc, char **argv, const std::string &name,
                           const std::string &fallback = "") {
  for (int i = 2; i < argc; ++i) {
    if (argv[i] == name) {
      if (i + 1 >= argc) {
        throw std::runtime_error("missing value for " + name);
      }
      return argv[i + 1];
    }
  }
  return fallback;
}

/**
 * @brief Convierte texto a entero sin aceptar caracteres residuales
 * @param value Texto a convertir
 * @param name Nombre logico usado en mensajes de error
 * @return Valor convertido a size_t
 */
std::size_t parseSize(const std::string &value, const std::string &name) {
  std::size_t consumed = 0;
  const unsigned long long parsed = std::stoull(value, &consumed);
  if (consumed != value.size()) {
    throw std::runtime_error("invalid integer for " + name + ": " + value);
  }
  return static_cast<std::size_t>(parsed);
}

/**
 * @brief Convierte texto a float sin aceptar caracteres residuales
 * @param value Texto a convertir
 * @param name Nombre logico usado en mensajes de error
 * @return Valor convertido a float
 */
float parseFloat(const std::string &value, const std::string &name) {
  std::size_t consumed = 0;
  const float parsed = std::stof(value, &consumed);
  if (consumed != value.size()) {
    throw std::runtime_error("invalid float for " + name + ": " + value);
  }
  return parsed;
}

/**
 * @brief Lee la opcion --rect y crea el rectangulo de consulta
 * @param argc Cantidad de argumentos de CLI
 * @param argv Argumentos de CLI
 * @return Rectangulo en formato x1,x2,y1,y2
 */
rtree::Rect parseRect(int argc, char **argv) {
  for (int i = 2; i < argc; ++i) {
    if (std::string(argv[i]) == "--rect") {
      if (i + 4 >= argc) {
        throw std::runtime_error("missing values for --rect");
      }
      const float xmin = parseFloat(argv[i + 1], "xmin");
      const float xmax = parseFloat(argv[i + 2], "xmax");
      const float ymin = parseFloat(argv[i + 3], "ymin");
      const float ymax = parseFloat(argv[i + 4], "ymax");
      if (xmax < xmin || ymax < ymin) {
        throw std::runtime_error("invalid rect bounds");
      }
      return rtree::Rect{xmin, xmax, ymin, ymax};
    }
  }
  throw std::runtime_error("missing required option --rect");
}

/**
 * @brief Ejecuta el comando build del CLI
 * @param argc Cantidad de argumentos de CLI
 * @param argv Argumentos de CLI
 * @return Codigo de salida del comando
 */
int runBuild(int argc, char **argv) {
  const std::filesystem::path dataset = requireOption(argc, argv, "--dataset");
  const std::size_t n = parseSize(requireOption(argc, argv, "--n"), "--n");
  const rtree::BuildMethod method =
      rtree::parseBuildMethod(requireOption(argc, argv, "--method"));
  const std::filesystem::path out = requireOption(argc, argv, "--out");

  const rtree::BuildResult build = rtree::buildTreeFromDataset(dataset, n, method);
  const auto parent = out.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent);
  }
  rtree::writeTreeToDisk(out, build.nodes);
  std::cout << "nodes=" << build.nodes.size() << " height=" << build.height
            << " out=" << out.string() << '\n';
  return 0;
}

/**
 * @brief Ejecuta el comando query del CLI
 * @param argc Cantidad de argumentos de CLI
 * @param argv Argumentos de CLI
 * @return Codigo de salida del comando
 */
int runQuery(int argc, char **argv) {
  const std::filesystem::path tree = requireOption(argc, argv, "--tree");
  const rtree::Rect rect = parseRect(argc, argv);
  std::int64_t ioCounter = 0;
  const std::vector<rtree::Point> points =
      rtree::rangeQuery(tree, rect, ioCounter);
  std::cout << "points_found=" << points.size() << " io_reads=" << ioCounter
            << '\n';
  return 0;
}

/**
 * @brief Ejecuta una medicion de construccion desde el CLI
 * @param argc Cantidad de argumentos de CLI
 * @param argv Argumentos de CLI
 * @return Codigo de salida del comando
 */
int runExperimentBuild(int argc, char **argv) {
  rtree::BuildExperimentOptions options;
  options.dataset = requireOption(argc, argv, "--dataset");
  options.datasetName = requireOption(argc, argv, "--dataset-name");
  options.method = rtree::parseBuildMethod(requireOption(argc, argv, "--method"));
  options.n = parseSize(requireOption(argc, argv, "--n"), "--n");
  options.csv = requireOption(argc, argv, "--csv");
  options.treeOut = optionalOption(argc, argv, "--out");
  rtree::runBuildExperiment(options);
  return 0;
}

/**
 * @brief Ejecuta una medicion de consultas desde el CLI
 * @param argc Cantidad de argumentos de CLI
 * @param argv Argumentos de CLI
 * @return Codigo de salida del comando
 */
int runExperimentQuery(int argc, char **argv) {
  rtree::QueryExperimentOptions options;
  options.dataset = requireOption(argc, argv, "--dataset");
  options.tree = requireOption(argc, argv, "--tree");
  options.datasetName = requireOption(argc, argv, "--dataset-name");
  options.method = rtree::parseBuildMethod(requireOption(argc, argv, "--method"));
  options.queries =
      parseSize(requireOption(argc, argv, "--queries"), "--queries");
  options.side = parseFloat(requireOption(argc, argv, "--side"), "--side");
  options.csv = requireOption(argc, argv, "--csv");
  options.summary = requireOption(argc, argv, "--summary");
  rtree::runQueryExperiment(options);
  return 0;
}

} // namespace

/**
 * @brief Punto de entrada del ejecutable rtree_cli
 * @param argc Cantidad de argumentos de CLI
 * @param argv Argumentos de CLI
 * @return Codigo de salida del programa
 */
int main(int argc, char **argv) {
  try {
    if (argc < 2 || hasOption(argc, argv, "--help")) {
      usage();
      return argc < 2 ? 1 : 0;
    }

    const std::string command = argv[1];
    if (command == "build") {
      return runBuild(argc, argv);
    }
    if (command == "query") {
      return runQuery(argc, argv);
    }
    if (command == "experiment-build") {
      return runExperimentBuild(argc, argv);
    }
    if (command == "experiment-query") {
      return runExperimentQuery(argc, argv);
    }

    usage();
    throw std::runtime_error("unknown command: " + command);
  } catch (const std::exception &error) {
    std::cerr << "error: " << error.what() << '\n';
    return 2;
  }
}
