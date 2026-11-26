#include "TmpDir.h"

#include <filesystem>
#include <iomanip>
#include <memory>
#include <mutex>
#include <random>
#include <sstream>
#include <vector>

#include "MyException.h"

namespace {
struct TempState {
  std::mutex mutex;
  std::string root;
  std::vector<std::unique_ptr<TmpDir>> directories;
};
TempState& tempState() {
  static TempState state;
  return state;
}
}  // namespace

void TmpDir::setTempRoot(const std::string& root) {
  std::lock_guard<std::mutex> lock(tempState().mutex);
  tempState().root = root;
}

TmpDir::TmpDir() {
  namespace fs = std::filesystem;
  std::string configuredRoot;
  {
    std::lock_guard<std::mutex> lock(tempState().mutex);
    configuredRoot = tempState().root;
  }
  try {
    std::vector<fs::path> roots;
    if (configuredRoot.empty()) {
      std::error_code ec;
      const auto cwd = fs::current_path(ec);
      if (!ec) roots.push_back(cwd);
      roots.push_back(fs::temp_directory_path());
    } else {
      roots.push_back(fs::absolute(configuredRoot).lexically_normal());
    }
    std::random_device seed;
    std::mt19937_64 generator(seed());
    std::error_code lastError;
    for (const auto& root : roots) {
      fs::create_directories(root, lastError);
      if (lastError) continue;
      for (;;) {
        std::ostringstream name;
        name << ".percolator-tmp-" << std::hex << std::setw(16)
             << std::setfill('0') << generator();
        const auto dir = root / name.str();
        if (fs::create_directory(dir, lastError)) {
          path_ = dir.string();
          return;
        }
        if (lastError) break;
      }
    }
    throw MyException("ERROR: Could not create a temporary directory: " +
                      lastError.message() + "\n");
  } catch (const fs::filesystem_error& e) {
    throw MyException(std::string("ERROR: Could not create a temporary ") +
                      "directory: " + e.what() + "\n");
  }
}

TmpDir::~TmpDir() {
  std::error_code ec;
  std::filesystem::remove_all(path_, ec);
}

void TmpDir::createTempFile(std::string& file, std::string& directory) {
  auto owner = std::make_unique<TmpDir>();
  directory = owner->getPath();
  file = (std::filesystem::path(directory) / "converters-tmp.tcb").string();
  std::lock_guard<std::mutex> lock(tempState().mutex);
  tempState().directories.push_back(std::move(owner));
}

void TmpDir::cleanupAll() {
  std::vector<std::unique_ptr<TmpDir>> directories;
  {
    std::lock_guard<std::mutex> lock(tempState().mutex);
    directories.swap(tempState().directories);
  }
}
