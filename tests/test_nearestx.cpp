#include "bulkload.hpp"
#include "geometry.hpp"
#include "test_helpers.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

std::size_t ceilDiv(std::size_t value, std::size_t divisor) {
  return (value + divisor - 1) / divisor;
}

std::vector<rtree::Entry> makeEntries(std::size_t count) {
  std::vector<rtree::Entry> entries;
  entries.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    const float x = static_cast<float>((count - i) % 997);
    const float y = static_cast<float>((i * 37) % 541);
    entries.push_back(rtree::makeLeafEntry({x, y}));
  }
  return entries;
}

bool isLeafNode(const rtree::Node &node) {
  return node.k == 0 || node.children[0].child == rtree::kLeafChild;
}

rtree::Rect exactNodeMBR(const rtree::Node &node) {
  assert(node.k > 0);
  rtree::Rect mbr = node.children[0].mbr;
  for (std::int32_t i = 1; i < node.k; ++i) {
    mbr = rtree::combineMBR(mbr, node.children[static_cast<std::size_t>(i)].mbr);
  }
  return mbr;
}

bool sameRect(const rtree::Rect &lhs, const rtree::Rect &rhs) {
  return lhs.x1 == rhs.x1 && lhs.y1 == rhs.y1 && lhs.x2 == rhs.x2 &&
         lhs.y2 == rhs.y2;
}

std::vector<std::pair<float, float>> leafPoints(const rtree::BuildResult &build) {
  std::vector<std::pair<float, float>> points;
  for (const rtree::Node &node : build.nodes) {
    if (!isLeafNode(node)) {
      continue;
    }
    for (std::int32_t i = 0; i < node.k; ++i) {
      const rtree::Entry &entry = node.children[static_cast<std::size_t>(i)];
      assert(entry.child == rtree::kLeafChild);
      assert(entry.mbr.x1 == entry.mbr.x2);
      assert(entry.mbr.y1 == entry.mbr.y2);
      points.push_back({entry.mbr.x1, entry.mbr.y1});
    }
  }
  std::sort(points.begin(), points.end());
  return points;
}

std::vector<std::pair<float, float>>
entryPoints(const std::vector<rtree::Entry> &entries) {
  std::vector<std::pair<float, float>> points;
  points.reserve(entries.size());
  for (const rtree::Entry &entry : entries) {
    points.push_back({entry.mbr.x1, entry.mbr.y1});
  }
  std::sort(points.begin(), points.end());
  return points;
}

void assertParentMBRs(const rtree::BuildResult &build) {
  for (const rtree::Node &node : build.nodes) {
    if (isLeafNode(node)) {
      continue;
    }
    for (std::int32_t i = 0; i < node.k; ++i) {
      const rtree::Entry &entry = node.children[static_cast<std::size_t>(i)];
      assert(entry.child >= 0);
      const rtree::Node &child = build.nodes[static_cast<std::size_t>(entry.child)];
      assert(sameRect(entry.mbr, exactNodeMBR(child)));
    }
  }
}

std::size_t expectedNodeCount(std::size_t entryCount) {
  std::size_t total = 1;
  while (entryCount > static_cast<std::size_t>(rtree::kMaxChildren)) {
    const std::size_t levelNodes =
        ceilDiv(entryCount, static_cast<std::size_t>(rtree::kMaxChildren));
    total += levelNodes;
    entryCount = levelNodes;
  }
  return total;
}

std::int32_t expectedHeight(std::size_t entryCount) {
  std::int32_t height = 1;
  while (entryCount > static_cast<std::size_t>(rtree::kMaxChildren)) {
    entryCount =
        ceilDiv(entryCount, static_cast<std::size_t>(rtree::kMaxChildren));
    ++height;
  }
  return height;
}

} // namespace

int main() {
  {
    const auto entries = makeEntries(17);
    const rtree::BuildResult build = rtree::buildNearestX(entries);
    assert(build.root == rtree::kRootIndex);
    assert(build.height == 1);
    assert(build.nodes.size() == 1);
    assert(build.nodes[0].k == 17);
    assert(isLeafNode(build.nodes[0]));
    assert(leafPoints(build) == entryPoints(entries));
  }

  {
    const auto entries = makeEntries(300);
    const rtree::BuildResult build = rtree::buildNearestX(entries);
    assert(build.root == rtree::kRootIndex);
    assert(build.height == 2);
    assert(build.nodes.size() == 3);
    assert(build.nodes[0].k == 2);
    assert(!isLeafNode(build.nodes[0]));
    assert(leafPoints(build) == entryPoints(entries));
    assertParentMBRs(build);
    assert(build.nodes.size() == expectedNodeCount(entries.size()));
    assert(build.height == expectedHeight(entries.size()));
  }

  {
    const std::size_t count =
        static_cast<std::size_t>(rtree::kMaxChildren) *
            static_cast<std::size_t>(rtree::kMaxChildren) +
        1;
    const auto entries = makeEntries(count);
    const rtree::BuildResult build = rtree::buildNearestX(entries);
    assert(build.nodes.size() == expectedNodeCount(entries.size()));
    assert(build.height == expectedHeight(entries.size()));
    assert(build.height == 3);
    assertParentMBRs(build);
  }

  return 0;
}
