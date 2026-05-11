#include "bulkload.hpp"

#include "geometry.hpp"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace rtree {

namespace {

/**
 * @brief Calcula la division entera hacia arriba
 * @param value Numerador
 * @param divisor Denominador positivo
 * @return ceil(value / divisor)
 */
std::size_t ceilDiv(std::size_t value, std::size_t divisor) {
  return (value + divisor - 1) / divisor;
}

/**
 * @brief Estima cuantos nodos necesitara el arbol para reservar memoria
 * @param entryCount Cantidad de entradas del nivel inicial
 * @return Cantidad total aproximada de nodos, incluyendo raiz
 */
std::size_t reserveNodeCount(std::size_t entryCount) {
  std::size_t total = 1; // nodes[0] is reserved for the root.
  while (entryCount > static_cast<std::size_t>(kMaxChildren)) {
    const std::size_t levelNodes =
        ceilDiv(entryCount, static_cast<std::size_t>(kMaxChildren));
    total += levelNodes;
    entryCount = levelNodes;
  }
  return total;
}

/**
 * @brief Compara entradas por centro X con desempates deterministas
 * @param lhs Primera entrada
 * @param rhs Segunda entrada
 * @return true si lhs debe quedar antes que rhs
 */
bool entryLessNearestX(const Entry &lhs, const Entry &rhs) {
  if (centerX(lhs.mbr) != centerX(rhs.mbr)) {
    return centerX(lhs.mbr) < centerX(rhs.mbr);
  }
  if (centerY(lhs.mbr) != centerY(rhs.mbr)) {
    return centerY(lhs.mbr) < centerY(rhs.mbr);
  }
  if (lhs.mbr.x1 != rhs.mbr.x1) {
    return lhs.mbr.x1 < rhs.mbr.x1;
  }
  if (lhs.mbr.y1 != rhs.mbr.y1) {
    return lhs.mbr.y1 < rhs.mbr.y1;
  }
  if (lhs.mbr.x2 != rhs.mbr.x2) {
    return lhs.mbr.x2 < rhs.mbr.x2;
  }
  if (lhs.mbr.y2 != rhs.mbr.y2) {
    return lhs.mbr.y2 < rhs.mbr.y2;
  }
  return lhs.child < rhs.child;
}

/**
 * @brief Compara entradas por centro Y y reutiliza el orden X para empates
 * @param lhs Primera entrada
 * @param rhs Segunda entrada
 * @return true si lhs debe quedar antes que rhs
 */
bool entryLessCenterY(const Entry &lhs, const Entry &rhs) {
  if (centerY(lhs.mbr) != centerY(rhs.mbr)) {
    return centerY(lhs.mbr) < centerY(rhs.mbr);
  }
  return entryLessNearestX(lhs, rhs);
}

/**
 * @brief Convierte un indice size_t a NodeIndex validando overflow
 * @param index Indice calculado en el vector de nodos
 * @return Indice convertido a int32
 */
NodeIndex checkedNodeIndex(std::size_t index) {
  if (index > static_cast<std::size_t>(std::numeric_limits<NodeIndex>::max())) {
    throw std::runtime_error("too many nodes to address with NodeIndex");
  }
  return static_cast<NodeIndex>(index);
}

/**
 * @brief Calcula el MBR que cubre todas las entradas validas de un nodo
 * @param node Nodo cuyas entradas se combinan
 * @return MBR del nodo completo
 */
Rect nodeMBR(const Node &node) {
  if (node.k <= 0) {
    return Rect{};
  }

  Rect mbr = node.children[0].mbr;
  for (std::int32_t i = 1; i < node.k; ++i) {
    mbr = combineMBR(mbr, node.children[static_cast<std::size_t>(i)].mbr);
  }
  return mbr;
}

/**
 * @brief Crea un nodo copiando un rango de entradas
 * @param entries Vector fuente de entradas
 * @param first Primer indice incluido
 * @param last Primer indice excluido
 * @return Nodo zero-initialized con k y sus entradas validas
 */
Node makeNode(const std::vector<Entry> &entries, std::size_t first,
              std::size_t last) {
  Node node{};
  node.k = static_cast<std::int32_t>(last - first);
  for (std::size_t i = first; i < last; ++i) {
    node.children[i - first] = entries[i];
  }
  return node;
}

/**
 * @brief Empaqueta un nivel completo en nodos de hasta b entradas
 * @param entries Entradas ya ordenadas para el metodo de bulk-loading
 * @param nodes Vector global donde se agregan los nodos creados
 * @return Entradas padre que apuntan a los nodos recien agregados
 */
std::vector<Entry> appendPackedLevel(const std::vector<Entry> &entries,
                                     std::vector<Node> &nodes) {
  std::vector<Entry> parents;
  parents.reserve(ceilDiv(entries.size(), static_cast<std::size_t>(kMaxChildren)));

  for (std::size_t first = 0; first < entries.size();
       first += static_cast<std::size_t>(kMaxChildren)) {
    const std::size_t last =
        std::min(first + static_cast<std::size_t>(kMaxChildren), entries.size());
    Node node = makeNode(entries, first, last);
    const NodeIndex childIndex = checkedNodeIndex(nodes.size());
    const Rect mbr = nodeMBR(node);
    // El indice del hijo es la posicion que tendra al serializar el vector.
    nodes.push_back(node);
    parents.push_back(Entry{mbr, childIndex});
  }

  return parents;
}

/**
 * @brief Empaqueta un nivel usando la estrategia 2D de STR
 * @param entries Entradas del nivel actual
 * @param nodes Vector global donde se agregan los nodos creados
 * @return Entradas padre del nuevo nivel
 */
std::vector<Entry> packSTRLevel(std::vector<Entry> entries,
                                std::vector<Node> &nodes) {
  const std::size_t pageCount =
      ceilDiv(entries.size(), static_cast<std::size_t>(kMaxChildren));
  const std::size_t sliceCount =
      static_cast<std::size_t>(std::ceil(std::sqrt(static_cast<double>(pageCount))));
  const std::size_t sliceSize = ceilDiv(entries.size(), sliceCount);

  // Adaptacion 2D de STR: n/b no siempre es un cuadrado perfecto.
  std::sort(entries.begin(), entries.end(), entryLessNearestX);
  for (std::size_t first = 0; first < entries.size(); first += sliceSize) {
    const std::size_t last = std::min(first + sliceSize, entries.size());
    std::sort(entries.begin() + static_cast<std::ptrdiff_t>(first),
              entries.begin() + static_cast<std::ptrdiff_t>(last),
              entryLessCenterY);
  }

  return appendPackedLevel(entries, nodes);
}

} // namespace

/**
 * @brief Construye un R-tree estatico usando Nearest-X
 * @param entries Entradas iniciales o entradas padre de un nivel
 * @return Arbol completo en memoria con raiz en nodes[0]
 */
BuildResult buildNearestX(std::vector<Entry> entries) {
  BuildResult result;
  result.root = kRootIndex;
  result.height = 1;
  result.nodes.reserve(reserveNodeCount(entries.size()));
  result.nodes.push_back(Node{});

  if (entries.size() <= static_cast<std::size_t>(kMaxChildren)) {
    // Si todo cabe en un nodo, la raiz tambien es hoja.
    result.nodes[static_cast<std::size_t>(kRootIndex)] =
        makeNode(entries, 0, entries.size());
    return result;
  }

  std::sort(entries.begin(), entries.end(), entryLessNearestX);

  // Primero se crean las hojas; luego se sube nivel por nivel.
  std::vector<Entry> current = appendPackedLevel(entries, result.nodes);
  std::int32_t levelsBelowRoot = 1;

  while (current.size() > static_cast<std::size_t>(kMaxChildren)) {
    std::sort(current.begin(), current.end(), entryLessNearestX);
    current = appendPackedLevel(current, result.nodes);
    ++levelsBelowRoot;
  }

  result.nodes[static_cast<std::size_t>(kRootIndex)] =
      makeNode(current, 0, current.size());
  result.height = levelsBelowRoot + 1;
  return result;
}

/**
 * @brief Construye un R-tree estatico usando Sort-Tile-Recursive
 * @param entries Entradas iniciales o entradas padre de un nivel
 * @return Arbol completo en memoria con raiz en nodes[0]
 */
BuildResult buildSTR(std::vector<Entry> entries) {
  BuildResult result;
  result.root = kRootIndex;
  result.height = 1;
  result.nodes.reserve(reserveNodeCount(entries.size()));
  result.nodes.push_back(Node{});

  if (entries.size() <= static_cast<std::size_t>(kMaxChildren)) {
    // Si todo cabe en un nodo, no hace falta aplicar particion STR.
    result.nodes[static_cast<std::size_t>(kRootIndex)] =
        makeNode(entries, 0, entries.size());
    return result;
  }

  std::vector<Entry> current = packSTRLevel(std::move(entries), result.nodes);
  std::int32_t levelsBelowRoot = 1;

  while (current.size() > static_cast<std::size_t>(kMaxChildren)) {
    current = packSTRLevel(std::move(current), result.nodes);
    ++levelsBelowRoot;
  }

  result.nodes[static_cast<std::size_t>(kRootIndex)] =
      makeNode(current, 0, current.size());
  result.height = levelsBelowRoot + 1;
  return result;
}

} // namespace rtree
