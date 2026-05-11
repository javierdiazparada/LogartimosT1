#include "geometry.hpp"

#include <algorithm>

namespace rtree {

/**
 * @brief Calcula el centro horizontal de un MBR
 * @param rect Rectangulo en formato x1,x2,y1,y2
 * @return Coordenada X del centro del rectangulo
 */
float centerX(const Rect &rect) noexcept { return (rect.x1 + rect.x2) * 0.5f; }

/**
 * @brief Calcula el centro vertical de un MBR
 * @param rect Rectangulo en formato x1,x2,y1,y2
 * @return Coordenada Y del centro del rectangulo
 */
float centerY(const Rect &rect) noexcept { return (rect.y1 + rect.y2) * 0.5f; }

/**
 * @brief Determina si dos rectangulos se intersectan
 * @param lhs Primer rectangulo
 * @param rhs Segundo rectangulo
 * @return true si se intersectan, incluyendo contacto en bordes
 */
bool intersects(const Rect &lhs, const Rect &rhs) noexcept {
  return lhs.x1 <= rhs.x2 && rhs.x1 <= lhs.x2 && lhs.y1 <= rhs.y2 &&
         rhs.y1 <= lhs.y2;
}

/**
 * @brief Determina si un punto esta dentro de un rectangulo
 * @param rect Rectangulo de consulta
 * @param point Punto a evaluar
 * @return true si el punto esta contenido, incluyendo bordes
 */
bool contains(const Rect &rect, const Point &point) noexcept {
  return rect.x1 <= point.x && point.x <= rect.x2 && rect.y1 <= point.y &&
         point.y <= rect.y2;
}

/**
 * @brief Calcula el MBR minimo que contiene dos MBRs
 * @param lhs Primer MBR
 * @param rhs Segundo MBR
 * @return Rectangulo combinado en formato x1,x2,y1,y2
 */
Rect combineMBR(const Rect &lhs, const Rect &rhs) noexcept {
  return Rect{std::min(lhs.x1, rhs.x1), std::max(lhs.x2, rhs.x2),
              std::min(lhs.y1, rhs.y1), std::max(lhs.y2, rhs.y2)};
}

/**
 * @brief Convierte un punto en una entrada hoja del R-tree
 * @param point Punto original
 * @return Entrada con MBR degenerado y puntero hijo igual a -1
 */
Entry makeLeafEntry(const Point &point) noexcept {
  // En una hoja, el punto se representa como rectangulo de area cero.
  return Entry{Rect{point.x, point.x, point.y, point.y}, kLeafChild};
}

} // namespace rtree
