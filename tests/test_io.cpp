#include "disk.hpp"
#include "geometry.hpp"
#include "test_helpers.hpp"

#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::filesystem::path testPath(const char *name) {
  return std::filesystem::temp_directory_path() / name;
}

bool sameNode(const rtree::Node &lhs, const rtree::Node &rhs) {
  return std::memcmp(&lhs, &rhs, sizeof(rtree::Node)) == 0;
}

void writeFloat(std::ofstream &out, float value) {
  out.write(reinterpret_cast<const char *>(&value), sizeof(value));
}

} // namespace

int main() {
  assert(sizeof(rtree::Node) == 4096);

  rtree::Node first{};
  first.k = 2;
  first.children[0] = rtree::makeLeafEntry({1.0f, 2.0f});
  first.children[1] = rtree::Entry{rtree::Rect{0.0f, 3.0f, 1.0f, 4.0f}, 7};

  rtree::Node second{};
  second.k = 1;
  second.children[0] =
      rtree::Entry{rtree::Rect{-5.0f, -3.0f, -4.0f, -2.0f}, 11};

  const std::vector<rtree::Node> nodes{first, second};
  const auto treePath = testPath("rtree_test_round_trip.bin");
  rtree::writeTreeToDisk(treePath, nodes);

  assert(std::filesystem::file_size(treePath) ==
         nodes.size() * sizeof(rtree::Node));

  std::ifstream treeIn(treePath, std::ios::binary);
  assert(treeIn);
  std::int64_t ioCounter = 0;
  const rtree::Node readSecond = rtree::readNode(treeIn, 1, ioCounter);
  const rtree::Node readFirst = rtree::readNode(treeIn, 0, ioCounter);
  assert(ioCounter == 2);
  assert(sameNode(readFirst, first));
  assert(sameNode(readSecond, second));
  treeIn.close();
  std::filesystem::remove(treePath);

  const auto pointsPath = testPath("rtree_test_points.bin");
  {
    std::ofstream pointsOut(pointsPath, std::ios::binary | std::ios::trunc);
    assert(pointsOut);
    writeFloat(pointsOut, 1.5f);
    writeFloat(pointsOut, 2.5f);
    writeFloat(pointsOut, -3.0f);
    writeFloat(pointsOut, 4.25f);
  }

  const auto points = rtree::readPoints(pointsPath, 2);
  assert(points.size() == 2);
  assert(points[0].x == 1.5f);
  assert(points[0].y == 2.5f);
  assert(points[1].x == -3.0f);
  assert(points[1].y == 4.25f);

  bool sawShortFile = false;
  try {
    (void)rtree::readPoints(pointsPath, 3);
  } catch (const std::runtime_error &error) {
    sawShortFile =
        std::string(error.what()).find("requested") != std::string::npos;
  }
  assert(sawShortFile);
  std::filesystem::remove(pointsPath);

  const auto corruptPath = testPath("rtree_test_points_corrupt.bin");
  {
    std::ofstream corruptOut(corruptPath, std::ios::binary | std::ios::trunc);
    assert(corruptOut);
    writeFloat(corruptOut, 1.0f);
    writeFloat(corruptOut, 2.0f);
    const char trailing = 'x';
    corruptOut.write(&trailing, 1);
  }

  bool sawCorruptBytes = false;
  try {
    (void)rtree::readPoints(corruptPath);
  } catch (const std::runtime_error &error) {
    sawCorruptBytes =
        std::string(error.what()).find("trailing bytes") != std::string::npos;
  }
  assert(sawCorruptBytes);
  std::filesystem::remove(corruptPath);

  return 0;
}
