#pragma once

#include "types.hpp"

#include <vector>

namespace rtree {

/**
 * @brief Construye un R-tree estatico usando el metodo Nearest-X
 * @param entries Entradas iniciales o de nivel intermedio a empaquetar
 * @return Arbol construido en RAM, con la raiz en nodes[0]
 */
BuildResult buildNearestX(std::vector<Entry> entries);

/**
 * @brief Construye un R-tree estatico usando Sort-Tile-Recursive
 * @param entries Entradas iniciales o de nivel intermedio a empaquetar
 * @return Arbol construido en RAM, con la raiz en nodes[0]
 */
BuildResult buildSTR(std::vector<Entry> entries);

} // namespace rtree
