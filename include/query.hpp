#pragma once

#include "types.hpp"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace rtree {

/**
 * @brief Ejecuta una consulta por rango leyendo nodos desde el archivo del arbol
 * @param treePath Ruta del archivo binario del R-tree
 * @param queryRect Rectangulo de consulta en formato x1,x2,y1,y2
 * @param ioCounter Contador que acumula lecturas logicas de nodos
 * @return Puntos contenidos en el rectangulo de consulta
 */
std::vector<Point> rangeQuery(const std::filesystem::path &treePath,
                              const Rect &queryRect,
                              std::int64_t &ioCounter);

} // namespace rtree
