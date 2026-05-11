#pragma once

#include "types.hpp"

namespace rtree {

/**
 * @brief Calcula la coordenada X del centro de un rectangulo
 * @param rect Rectangulo a evaluar
 * @return Coordenada X central
 */
float centerX(const Rect &rect) noexcept;

/**
 * @brief Calcula la coordenada Y del centro de un rectangulo
 * @param rect Rectangulo a evaluar
 * @return Coordenada Y central
 */
float centerY(const Rect &rect) noexcept;

/**
 * @brief Verifica si dos rectangulos se intersectan de forma inclusiva
 * @param lhs Primer rectangulo
 * @param rhs Segundo rectangulo
 * @return true si los rectangulos se intersectan o tocan borde
 */
bool intersects(const Rect &lhs, const Rect &rhs) noexcept;

/**
 * @brief Verifica si un punto esta contenido en un rectangulo
 * @param rect Rectangulo de consulta
 * @param point Punto a evaluar
 * @return true si el punto cae dentro del rectangulo, incluyendo bordes
 */
bool contains(const Rect &rect, const Point &point) noexcept;

/**
 * @brief Combina dos MBR en el rectangulo minimo que contiene ambos
 * @param lhs Primer MBR
 * @param rhs Segundo MBR
 * @return MBR combinado
 */
Rect combineMBR(const Rect &lhs, const Rect &rhs) noexcept;

/**
 * @brief Crea una entrada hoja a partir de un punto
 * @param point Punto original
 * @return Entrada con MBR degenerado y child igual a kLeafChild
 */
Entry makeLeafEntry(const Point &point) noexcept;

} // namespace rtree
