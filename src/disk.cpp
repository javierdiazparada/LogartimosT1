#include "disk.hpp"

#include <cstddef>
#include <fstream>
#include <istream>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <string>

namespace rtree {

namespace {

/**
 * @brief Calcula el offset binario de un nodo dentro del archivo
 * @param index Indice logico del nodo
 * @return Offset en bytes desde el inicio del archivo
 */
std::streamoff nodeOffset(NodeIndex index) {
  if (index < 0) {
    throw std::runtime_error("negative node index: " + std::to_string(index));
  }
  return std::streamoff(index) * sizeof(Node);
}

/**
 * @brief Convierte una ruta a texto para mensajes de error
 * @param path Ruta a mostrar
 * @return Ruta convertida con la representacion nativa del sistema
 */
std::string displayPath(const std::filesystem::path &path) {
  return path.string();
}

} // namespace

/**
 * @brief Lee puntos desde un archivo binario de pares float32 X,Y
 * @param path Ruta del archivo binario de puntos
 * @param limit Cantidad maxima de puntos a leer; 0 significa leer todo
 * @return Vector de puntos leidos desde el archivo
 */
std::vector<Point> readPoints(const std::filesystem::path &path,
                              std::size_t limit) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("cannot open point file for binary read: " +
                             displayPath(path));
  }

  const std::uintmax_t bytes = std::filesystem::file_size(path);
  // Cada punto son exactamente dos floats; bytes residuales indican corrupcion.
  if (bytes % sizeof(Point) != 0) {
    throw std::runtime_error(
        "corrupt point file has trailing bytes not forming a float32 pair: " +
        displayPath(path));
  }

  const std::size_t available =
      static_cast<std::size_t>(bytes / sizeof(Point));
  const std::size_t wanted = limit == 0 ? available : limit;
  if (available < wanted) {
    throw std::runtime_error("point file does not contain requested float32 "
                             "pairs: requested " +
                             std::to_string(wanted) + ", available " +
                             std::to_string(available));
  }

  std::vector<Point> points(wanted);
  if (points.empty()) {
    return points;
  }

  input.read(reinterpret_cast<char *>(points.data()),
             static_cast<std::streamsize>(points.size() * sizeof(Point)));
  if (!input) {
    throw std::runtime_error("failed to read float32 point pairs from: " +
                             displayPath(path));
  }
  return points;
}

/**
 * @brief Escribe un nodo completo en la posicion indicada del stream
 * @param out Stream binario de salida
 * @param index Indice logico donde se escribira el nodo
 * @param node Nodo de 4096 bytes a escribir
 */
void writeNode(std::ostream &out, NodeIndex index, const Node &node) {
  out.seekp(nodeOffset(index), std::ios::beg);
  if (!out) {
    throw std::runtime_error("failed to seek output stream for node index " +
                             std::to_string(index));
  }

  out.write(reinterpret_cast<const char *>(&node),
            static_cast<std::streamsize>(sizeof(Node)));
  if (!out) {
    throw std::runtime_error("failed to write node index " +
                             std::to_string(index));
  }
}

/**
 * @brief Lee un nodo completo desde la posicion indicada del stream
 * @param in Stream binario de entrada
 * @param index Indice logico del nodo a leer
 * @param ioCounter Contador de lecturas logicas de nodo
 * @return Nodo leido desde disco
 */
Node readNode(std::istream &in, NodeIndex index, std::int64_t &ioCounter) {
  in.clear();
  in.seekg(nodeOffset(index), std::ios::beg);
  if (!in) {
    throw std::runtime_error("failed to seek input stream for node index " +
                             std::to_string(index));
  }

  Node node{};
  in.read(reinterpret_cast<char *>(&node),
          static_cast<std::streamsize>(sizeof(Node)));
  if (!in) {
    throw std::runtime_error("failed to read complete node index " +
                             std::to_string(index));
  }

  // La tarea contabiliza una lectura por bloque/nodo leido correctamente.
  ++ioCounter;
  return node;
}

/**
 * @brief Escribe secuencialmente todos los nodos del arbol en disco
 * @param path Ruta de salida del archivo binario
 * @param nodes Vector de nodos con la raiz en la posicion 0
 */
void writeTreeToDisk(const std::filesystem::path &path,
                     const std::vector<Node> &nodes) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) {
    throw std::runtime_error("cannot open tree file for binary write: " +
                             displayPath(path));
  }

  for (std::size_t i = 0; i < nodes.size(); ++i) {
    if (i > static_cast<std::size_t>(std::numeric_limits<NodeIndex>::max())) {
      throw std::runtime_error("too many nodes to address with NodeIndex");
    }
    writeNode(output, static_cast<NodeIndex>(i), nodes[i]);
  }
  output.flush();
  if (!output) {
    throw std::runtime_error("failed to flush tree file: " + displayPath(path));
  }
}

} // namespace rtree
