#pragma once

#include "types.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace rtree {

/**
 * @brief Metodo de bulk-loading disponible para construir el R-tree
 */
enum class BuildMethod {
  NearestX,
  STR,
};

/**
 * @brief Opciones para medir y registrar una construccion de R-tree
 */
struct BuildExperimentOptions {
  std::filesystem::path dataset;   ///< Archivo binario de puntos
  std::string datasetName;         ///< Nombre logico del dataset
  BuildMethod method = BuildMethod::NearestX; ///< Metodo de construccion
  std::size_t n = 0;               ///< Cantidad de puntos a leer
  std::filesystem::path csv;       ///< CSV donde se agregan resultados
  std::filesystem::path treeOut;   ///< Ruta opcional del arbol generado
};

/**
 * @brief Opciones para medir y registrar consultas por rango
 */
struct QueryExperimentOptions {
  std::filesystem::path dataset;   ///< Dataset usado para validar existencia
  std::filesystem::path tree;      ///< Archivo binario del arbol a consultar
  std::string datasetName;         ///< Nombre logico del dataset
  BuildMethod method = BuildMethod::NearestX; ///< Metodo usado por el arbol
  std::size_t queries = 0;         ///< Cantidad de consultas aleatorias
  float side = 0.0f;               ///< Largo del lado de cada cuadrado
  std::filesystem::path csv;       ///< CSV detallado por consulta
  std::filesystem::path summary;   ///< CSV con promedios y desviaciones
};

/**
 * @brief Convierte texto de CLI a metodo de bulk-loading
 * @param name Nombre del metodo: nearestx o str
 * @return Metodo de bulk-loading correspondiente
 */
BuildMethod parseBuildMethod(const std::string &name);

/**
 * @brief Convierte un metodo de bulk-loading a texto para CSV/CLI
 * @param method Metodo a convertir
 * @return Nombre textual del metodo
 */
std::string buildMethodName(BuildMethod method);

/**
 * @brief Lee un dataset y construye un R-tree con el metodo indicado
 * @param dataset Archivo binario de puntos
 * @param n Cantidad de puntos a leer; 0 lee todo el archivo
 * @param method Metodo de bulk-loading
 * @return Arbol construido en RAM
 */
BuildResult buildTreeFromDataset(const std::filesystem::path &dataset,
                                  std::size_t n, BuildMethod method);

/**
 * @brief Ejecuta un experimento de construccion y agrega una fila al CSV
 * @param options Parametros del experimento de construccion
 */
void runBuildExperiment(const BuildExperimentOptions &options);

/**
 * @brief Ejecuta consultas aleatorias y agrega filas al CSV y resumen
 * @param options Parametros del experimento de consulta
 */
void runQueryExperiment(const QueryExperimentOptions &options);

} // namespace rtree
