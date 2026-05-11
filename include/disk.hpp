#pragma once

#include "types.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <vector>

namespace rtree {

/**
 * @brief Lee puntos desde un archivo binario de pares float32 X,Y
 * @param path Ruta del archivo binario de puntos
 * @param limit Cantidad maxima de puntos a leer; 0 lee todo el archivo
 * @return Vector con los puntos leidos en el mismo orden del archivo
 */
std::vector<Point> readPoints(const std::filesystem::path &path,
                              std::size_t limit = 0);

/**
 * @brief Escribe un nodo en una posicion especifica del stream binario
 * @param out Stream de salida abierto en modo binario
 * @param index Indice logico del nodo dentro del archivo
 * @param node Nodo completo de 4096 bytes a escribir
 */
void writeNode(std::ostream &out, NodeIndex index, const Node &node);

/**
 * @brief Lee un nodo desde una posicion especifica del stream binario
 * @param in Stream de entrada abierto en modo binario
 * @param index Indice logico del nodo dentro del archivo
 * @param ioCounter Contador que aumenta en 1 por cada lectura de nodo exitosa
 * @return Nodo leido desde disco
 */
Node readNode(std::istream &in, NodeIndex index, std::int64_t &ioCounter);

/**
 * @brief Serializa todos los nodos de un R-tree a un archivo binario
 * @param path Ruta de salida del archivo del arbol
 * @param nodes Nodos en orden secuencial, con la raiz en la posicion 0
 */
void writeTreeToDisk(const std::filesystem::path &path,
                     const std::vector<Node> &nodes);

} // namespace rtree
