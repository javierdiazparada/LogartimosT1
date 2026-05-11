#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

#if __has_include(<bit>)
#include <bit>
#endif

namespace rtree {

/**
 * @brief Indice de un nodo dentro del archivo binario del R-tree
 */
using NodeIndex = std::int32_t;

inline constexpr std::int32_t kBlockSize = 4096;
inline constexpr std::int32_t kMaxChildren = 204;
inline constexpr NodeIndex kLeafChild = -1;
inline constexpr NodeIndex kRootIndex = 0;

/**
 * @brief Punto bidimensional guardado como dos floats consecutivos
 */
struct Point {
  float x = 0.0f; ///< Coordenada X del punto
  float y = 0.0f; ///< Coordenada Y del punto
};

/**
 * @brief Rectangulo minimo envolvente en el orden binario pedido por la tarea
 */
struct Rect {
  float x1 = 0.0f; ///< Limite inferior en X
  float x2 = 0.0f; ///< Limite superior en X
  float y1 = 0.0f; ///< Limite inferior en Y
  float y2 = 0.0f; ///< Limite superior en Y
};

/**
 * @brief Par llave-valor de un nodo R-tree
 *
 * La llave es el MBR. El valor es el indice del nodo hijo en disco, o -1 si la
 * entrada representa un punto en una hoja.
 */
struct Entry {
  Rect mbr{}; ///< Minimum Bounding Rectangle asociado a la entrada
  // Empty slots stay zeroed; leaves use makeLeafEntry for kLeafChild.
  NodeIndex child = 0; ///< Indice del hijo, o kLeafChild si es hoja
};

/**
 * @brief Nodo persistente del R-tree con tamano exacto de un bloque
 */
struct Node {
  std::int32_t k = 0;              ///< Cantidad de entradas validas
  Entry children[kMaxChildren]{};  ///< Arreglo fijo de entradas
  // Padding fixes every persisted node at one 4096-byte block.
  std::byte pad[12]{};             ///< Relleno para llegar a 4096 bytes
};

/**
 * @brief Resultado de construir un R-tree en memoria
 */
struct BuildResult {
  std::vector<Node> nodes{};       ///< Nodos en orden de serializacion
  NodeIndex root = kRootIndex;     ///< Indice de la raiz, siempre 0
  std::int32_t height = 0;         ///< Altura del arbol, 1 si solo hay raiz
};

static_assert(sizeof(Point) == 8);
static_assert(sizeof(float) == 4);
static_assert(sizeof(std::int32_t) == 4);
static_assert(sizeof(Rect) == 16);
static_assert(sizeof(Entry) == 20);
static_assert(sizeof(Node) == kBlockSize);

static_assert(std::is_trivially_copyable_v<Point>);
static_assert(std::is_trivially_copyable_v<Rect>);
static_assert(std::is_trivially_copyable_v<Entry>);
static_assert(std::is_trivially_copyable_v<Node>);

#if __cpp_lib_endian >= 201907L
static_assert(std::endian::native == std::endian::little,
              "Raw node serialization assumes little-endian storage.");
#endif

} // namespace rtree
