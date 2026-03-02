#include "TmpDir.h"

#include <cstdlib>
#include <mutex>
#include <vector>

namespace {
std::vector<std::string>& tempDirectories() {
  static std::vector<std::string> dirs;
  return dirs;
}

std::mutex& tempDirectoriesMutex() {
  static std::mutex mutex;
  return mutex;
}

std::once_flag& cleanupRegistrationFlag() {
  static std::once_flag flag;
  return flag;
}

std::string& configuredTempRoot() {
  static std::string path;
  return path;
}
}  // namespace

void TmpDir::setTempRoot(const std::string& tempRoot) {
  std::lock_guard<std::mutex> lock(tempDirectoriesMutex());
  configuredTempRoot() = tempRoot;
}

void TmpDir::registerForCleanup(const std::string& tempDir) {
  if (tempDir.empty()) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock(tempDirectoriesMutex());
    tempDirectories().push_back(tempDir);
  }
  std::call_once(cleanupRegistrationFlag(), []() {
    std::atexit(&TmpDir::cleanupAll);
  });
}

void TmpDir::cleanupAll() {
  std::vector<std::string> dirs;
  {
    std::lock_guard<std::mutex> lock(tempDirectoriesMutex());
    dirs.swap(tempDirectories());
  }

  for (std::vector<std::string>::const_iterator it = dirs.begin();
       it != dirs.end(); ++it) {
    try {
      if (boost::filesystem::is_directory(*it)) {
        boost::filesystem::remove_all(*it);
      }
    } catch (const boost::filesystem::filesystem_error&) {
      // Best-effort cleanup for temporary files.
    }
  }
}

void TmpDir::createTempFile(std::string& tcf, std::string& tcd) {
  //TODO it would be nice to somehow avoid these declararions and therefore avoid the linking to
  //boost filesystem when we don't use them      
  tcf.clear();
  tcd.clear();

  const boost::filesystem::path file("converters-tmp.tcb");
  const boost::filesystem::path uniqueName =
      boost::filesystem::unique_path(".percolator-tmp-%%%%-%%%%-%%%%");

  auto tryCreateInRoot = [&](const boost::filesystem::path& root) -> bool {
    try {
      const boost::filesystem::path dir = root / uniqueName;
      if (boost::filesystem::is_directory(dir)) {
        boost::filesystem::remove_all(dir);
      }
      boost::filesystem::create_directory(dir);
      tcf = std::string((dir / file).string());
      tcd = dir.string();
      registerForCleanup(tcd);
      return true;
    } catch (const boost::filesystem::filesystem_error&) {
      return false;
    }
  };

  std::string tempRootOverride;
  {
    std::lock_guard<std::mutex> lock(tempDirectoriesMutex());
    tempRootOverride = configuredTempRoot();
  }

  if (!tempRootOverride.empty()) {
    try {
      boost::filesystem::path configuredRoot(tempRootOverride);
      if (!configuredRoot.is_absolute()) {
        configuredRoot =
            boost::filesystem::absolute(configuredRoot).lexically_normal();
      }
      if (!boost::filesystem::exists(configuredRoot)) {
        boost::filesystem::create_directories(configuredRoot);
      }
      if (tryCreateInRoot(configuredRoot)) {
        return;
      }
      std::cerr << "Failed to create temporary directory under --temp-folder "
                << configuredRoot.string() << std::endl;
      return;
    } catch (const boost::filesystem::filesystem_error& e) {
      std::cerr << e.what() << std::endl;
      return;
    }
  }

  boost::system::error_code ec;
  const boost::filesystem::path cwd = boost::filesystem::current_path(ec);
  if (!ec && tryCreateInRoot(cwd)) {
    return;
  }

  try {
    if (!tryCreateInRoot(boost::filesystem::temp_directory_path())) {
      std::cerr << "Failed to create temporary directory in current working "
                   "directory and system temporary directory."
                << std::endl;
    }
  } catch (const boost::filesystem::filesystem_error& e) {
    std::cerr << e.what() << std::endl;
  }
}
