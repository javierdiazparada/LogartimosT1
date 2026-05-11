#include "bulkload.hpp"
#include "disk.hpp"
#include "geometry.hpp"
#include "query.hpp"
#include "test_helpers.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace {

std::filesystem::path testPath(const char *name) {
  return std::filesystem::temp_directory_path() / name;
}

std::vector<rtree::Entry> leafEntries(const std::vector<rtree::Point> &points) {
  std::vector<rtree::Entry> entries;
  entries.reserve(points.size());
  for (const rtree::Point &point : points) {
    entries.push_back(rtree::makeLeafEntry(point));
  }
  return entries;
}

void assertQueryMatchesLinearScan(const std::filesystem::path &treePath,
                                  const std::vector<rtree::Point> &points,
                                  const rtree::Rect &queryRect,
                                  std::size_t expectedCount) {
  std::int64_t ioCounter = 0;
  auto actual = rtree::rangeQuery(treePath, queryRect, ioCounter);
  auto expected = bruteForceRange(points, queryRect);
  sortPointsLexicographically(actual);
  sortPointsLexicographically(expected);

  assert(expected.size() == expectedCount);
  assert(actual.size() == expected.size());
  for (std::size_t i = 0; i < actual.size(); ++i) {
    assert(actual[i].x == expected[i].x);
    assert(actual[i].y == expected[i].y);
  }
  assert(ioCounter == 1);
}

void assertQueryMatchesLinearScanWithIo(
    const std::filesystem::path &treePath,
    const std::vector<rtree::Point> &points, const rtree::Rect &queryRect,
    std::size_t expectedCount, std::int64_t expectedIoReads) {
  std::int64_t ioCounter = 0;
  auto actual = rtree::rangeQuery(treePath, queryRect, ioCounter);
  auto expected = bruteForceRange(points, queryRect);
  sortPointsLexicographically(actual);
  sortPointsLexicographically(expected);

  assert(expected.size() == expectedCount);
  assert(actual.size() == expected.size());
  for (std::size_t i = 0; i < actual.size(); ++i) {
    assert(actual[i].x == expected[i].x);
    assert(actual[i].y == expected[i].y);
  }
  assert(ioCounter == expectedIoReads);
}

} // namespace

int main() {
  const std::vector<rtree::Point> points{
      {0.10f, 0.10f}, {0.12f, 0.11f}, {0.50f, 0.50f}, {0.51f, 0.49f},
      {0.80f, 0.90f}, {0.81f, 0.91f}, {0.20f, 0.80f}, {0.90f, 0.20f}};

  const auto pointsPath = testPath("rtree_query_points.bin");
  writeTestPointsBin(pointsPath, points);
  const auto pointsFromDisk = rtree::readPoints(pointsPath);
  assert(pointsFromDisk.size() == points.size());

  const rtree::BuildResult build =
      rtree::buildNearestX(leafEntries(pointsFromDisk));
  assert(build.nodes.size() == 1);

  const auto treePath = testPath("rtree_query_tree.bin");
  rtree::writeTreeToDisk(treePath, build.nodes);

  assertQueryMatchesLinearScan(treePath, pointsFromDisk,
                               rtree::Rect{0.00f, 0.20f, 0.00f, 0.20f}, 2);
  assertQueryMatchesLinearScan(treePath, pointsFromDisk,
                               rtree::Rect{0.40f, 0.60f, 0.40f, 0.60f}, 2);
  assertQueryMatchesLinearScan(treePath, pointsFromDisk,
                               rtree::Rect{0.78f, 0.82f, 0.88f, 0.92f}, 2);
  assertQueryMatchesLinearScan(treePath, pointsFromDisk,
                               rtree::Rect{0.00f, 0.05f, 0.00f, 0.05f}, 0);

  std::filesystem::remove(treePath);
  std::filesystem::remove(pointsPath);

  {
    std::vector<rtree::Point> clusteredPoints;
    clusteredPoints.reserve(300);
    for (int i = 0; i < 204; ++i) {
      clusteredPoints.push_back({static_cast<float>(i) / 1020.0f,
                                 static_cast<float>(i % 100) / 100.0f});
    }
    for (int i = 0; i < 96; ++i) {
      clusteredPoints.push_back({0.80f + static_cast<float>(i) / 480.0f,
                                 static_cast<float>(i % 100) / 100.0f});
    }

    const rtree::BuildResult selectiveBuild =
        rtree::buildNearestX(leafEntries(clusteredPoints));
    assert(selectiveBuild.nodes.size() == 3);
    assert(selectiveBuild.nodes[0].k == 2);

    const auto selectiveTreePath = testPath("rtree_query_selective_tree.bin");
    rtree::writeTreeToDisk(selectiveTreePath, selectiveBuild.nodes);

    assertQueryMatchesLinearScanWithIo(
        selectiveTreePath, clusteredPoints,
        rtree::Rect{0.00f, 0.30f, 0.00f, 1.00f}, 204, 2);
    assertQueryMatchesLinearScanWithIo(
        selectiveTreePath, clusteredPoints,
        rtree::Rect{0.40f, 0.60f, 0.00f, 1.00f}, 0, 1);
    assertQueryMatchesLinearScanWithIo(
        selectiveTreePath, clusteredPoints,
        rtree::Rect{0.00f, 1.00f, 0.00f, 1.00f}, 300, 3);

    std::filesystem::remove(selectiveTreePath);
  }
  return 0;
}
