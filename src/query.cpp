#include "query.hpp"

#include "disk.hpp"
#include "geometry.hpp"

#include <cstddef>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace rtree {

namespace {

/**
 * @brief Extrae el punto guardado en una entrada hoja
 * @param entry Entrada hoja con MBR degenerado
 * @return Punto representado por la entrada
 */
Point pointFromLeafEntry(const Entry &entry) {
  return Point{entry.mbr.x1, entry.mbr.y1};
}

/**
 * @brief Recorre recursivamente un nodo ya leido desde disco
 * @param input Archivo binario del arbol abierto para lectura
 * @param node Nodo actual
 * @param queryRect Rectangulo de consulta
 * @param ioCounter Contador de lecturas de nodos desde disco
 * @param result Vector donde se agregan los puntos encontrados
 */
void visitNode(std::ifstream &input, const Node &node, const Rect &queryRect,
               std::int64_t &ioCounter, std::vector<Point> &result) {
  for (std::int32_t i = 0; i < node.k; ++i) {
    const Entry &entry = node.children[static_cast<std::size_t>(i)];
    // Si el MBR no intersecta la consulta, toda la rama se descarta.
    if (!intersects(entry.mbr, queryRect)) {
      continue;
    }

    if (entry.child == kLeafChild) {
      const Point point = pointFromLeafEntry(entry);
      if (contains(queryRect, point)) {
        result.push_back(point);
      }
      continue;
    }

    const Node child = readNode(input, entry.child, ioCounter);
    visitNode(input, child, queryRect, ioCounter, result);
  }
}

} // namespace

/**
 * @brief Ejecuta una consulta por rango leyendo el arbol desde archivo
 * @param treePath Ruta del archivo binario del R-tree
 * @param queryRect Rectangulo de consulta en formato x1,x2,y1,y2
 * @param ioCounter Contador acumulado de lecturas logicas de nodos
 * @return Puntos contenidos en el rectangulo
 */
std::vector<Point> rangeQuery(const std::filesystem::path &treePath,
                              const Rect &queryRect,
                              std::int64_t &ioCounter) {
  std::ifstream input(treePath, std::ios::binary);
  if (!input) {
    throw std::runtime_error("cannot open tree file for binary read: " +
                             treePath.string());
  }

  std::vector<Point> result;
  const Node root = readNode(input, kRootIndex, ioCounter);
  visitNode(input, root, queryRect, ioCounter, result);
  return result;
}

} // namespace rtree
