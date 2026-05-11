#include "bulkload.hpp"
#include "disk.hpp"
#include "geometry.hpp"
#include "query.hpp"
#include "test_helpers.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <random>
#include <vector>

namespace {

std::filesystem::path testPath(const char *name) {
  return std::filesystem::temp_directory_path() / name;
}

std::vector<rtree::Point> makeRandomPoints(std::size_t count,
                                           std::uint32_t seed) {
  std::mt19937 rng(seed);
  std::uniform_real_distribution<float> dist(0.0f, 1.0f);
  std::vector<rtree::Point> points;
  points.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    points.push_back({dist(rng), dist(rng)});
  }
  return points;
}

std::vector<rtree::Entry> leafEntries(const std::vector<rtree::Point> &points) {
  std::vector<rtree::Entry> entries;
  entries.reserve(points.size());
  for (const rtree::Point &point : points) {
    entries.push_back(rtree::makeLeafEntry(point));
  }
  return entries;
}

bool isLeafNode(const rtree::Node &node) {
  return node.k == 0 || node.children[0].child == rtree::kLeafChild;
}

std::vector<rtree::Point> leafPoints(const rtree::BuildResult &build) {
  std::vector<rtree::Point> points;
  for (const rtree::Node &node : build.nodes) {
    if (!isLeafNode(node)) {
      continue;
    }
    for (std::int32_t i = 0; i < node.k; ++i) {
      const rtree::Entry &entry = node.children[static_cast<std::size_t>(i)];
      assert(entry.child == rtree::kLeafChild);
      points.push_back({entry.mbr.x1, entry.mbr.y1});
    }
  }
  sortPointsLexicographically(points);
  return points;
}

void assertSamePoints(std::vector<rtree::Point> actual,
                      std::vector<rtree::Point> expected) {
  sortPointsLexicographically(actual);
  sortPointsLexicographically(expected);
  assert(actual.size() == expected.size());
  for (std::size_t i = 0; i < actual.size(); ++i) {
    assert(actual[i].x == expected[i].x);
    assert(actual[i].y == expected[i].y);
  }
}

void assertRangeMatchesBruteForce(const std::filesystem::path &treePath,
                                  const std::vector<rtree::Point> &points,
                                  const rtree::Rect &queryRect) {
  std::int64_t ioCounter = 0;
  auto actual = rtree::rangeQuery(treePath, queryRect, ioCounter);
  auto expected = bruteForceRange(points, queryRect);
  assert(ioCounter >= 1);
  assertSamePoints(std::move(actual), std::move(expected));
}

} // namespace

int main() {
  {
    const auto points = makeRandomPoints(32, 10);
    const rtree::BuildResult build = rtree::buildSTR(leafEntries(points));
    assert(build.root == rtree::kRootIndex);
    assert(build.height == 1);
    assert(build.nodes.size() == 1);
    assert(build.nodes[0].k == static_cast<std::int32_t>(points.size()));
    assert(isLeafNode(build.nodes[0]));
    assertSamePoints(leafPoints(build), points);
  }

  {
    const auto points = makeRandomPoints(777, 777);
    const rtree::BuildResult str = rtree::buildSTR(leafEntries(points));
    const rtree::BuildResult nearestX = rtree::buildNearestX(leafEntries(points));
    assert(str.nodes.size() == nearestX.nodes.size());
    assert(str.height == nearestX.height);
    assertSamePoints(leafPoints(str), points);

    const auto treePath = testPath("rtree_test_str_777.bin");
    rtree::writeTreeToDisk(treePath, str.nodes);
    assertRangeMatchesBruteForce(treePath, points,
                                 rtree::Rect{0.0f, 1.0f, 0.0f, 1.0f});
    assertRangeMatchesBruteForce(treePath, points,
                                 rtree::Rect{0.10f, 0.70f, 0.20f, 0.80f});
    assertRangeMatchesBruteForce(treePath, points,
                                 rtree::Rect{0.45f, 0.55f, 0.45f, 0.55f});
    assertRangeMatchesBruteForce(treePath, points,
                                 rtree::Rect{0.90f, 1.0f, 0.00f, 0.12f});
    std::filesystem::remove(treePath);
  }

  {
    const auto points = makeRandomPoints(500, 500);
    const rtree::BuildResult build = rtree::buildSTR(leafEntries(points));
    assertSamePoints(leafPoints(build), points);
  }

  return 0;
}
