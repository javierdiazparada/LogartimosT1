#pragma once

#include "geometry.hpp"
#include "types.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

inline void writeTestPointsBin(const std::filesystem::path &path,
                               const std::vector<rtree::Point> &points) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) {
    throw std::runtime_error("cannot create test point file: " + path.string());
  }
  if (!points.empty()) {
    output.write(reinterpret_cast<const char *>(points.data()),
                 static_cast<std::streamsize>(points.size() *
                                              sizeof(rtree::Point)));
  }
  if (!output) {
    throw std::runtime_error("cannot write test point file: " + path.string());
  }
}

inline std::vector<rtree::Point>
bruteForceRange(const std::vector<rtree::Point> &points,
                const rtree::Rect &rect) {
  std::vector<rtree::Point> result;
  for (const rtree::Point &point : points) {
    if (rtree::contains(rect, point)) {
      result.push_back(point);
    }
  }
  return result;
}

inline void sortPointsLexicographically(std::vector<rtree::Point> &points) {
  std::sort(points.begin(), points.end(), [](const auto &lhs, const auto &rhs) {
    if (lhs.x != rhs.x) {
      return lhs.x < rhs.x;
    }
    return lhs.y < rhs.y;
  });
}
