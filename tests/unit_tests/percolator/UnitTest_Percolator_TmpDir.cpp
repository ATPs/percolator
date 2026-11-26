/*
 * Copyright 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>
#include <system_error>

#include "TmpDir.h"

namespace {

namespace fs = std::filesystem;

fs::path makeUniqueTempDir(const std::string& prefix) {
  static std::mt19937_64 generator(std::random_device{}());
  for (int attempt = 0; attempt < 100; ++attempt) {
    fs::path path = fs::temp_directory_path() /
                    (prefix + std::to_string(generator()));
    std::error_code error;
    if (fs::create_directory(path, error)) {
      return path;
    }
  }
  throw std::runtime_error("Could not create a unique temporary directory");
}

class ScopedTempDir {
 public:
  ScopedTempDir() : path_(makeUniqueTempDir("percolator-tmpdir-")) {}

  ~ScopedTempDir() {
    std::error_code error;
    fs::remove_all(path_, error);
  }

  fs::path path() const { return path_; }

 private:
  fs::path path_;
};

class ScopedCurrentPath {
 public:
  explicit ScopedCurrentPath(const fs::path& newPath)
      : originalPath_(fs::current_path()) {
    fs::current_path(newPath);
  }

  ~ScopedCurrentPath() {
    std::error_code error;
    fs::current_path(originalPath_, error);
  }

 private:
  fs::path originalPath_;
};

}  // namespace

TEST(TmpDirTest, CreatesUnderCurrentDirectoryAndCleansUp) {
  ScopedTempDir tempDir;
  ScopedCurrentPath cwd(tempDir.path());
  TmpDir::setTempRoot("");

  std::string tempFilePath;
  std::string tempDirPath;
  TmpDir::createTempFile(tempFilePath, tempDirPath);

  ASSERT_FALSE(tempFilePath.empty());
  ASSERT_FALSE(tempDirPath.empty());

  fs::path createdDir(tempDirPath);
  fs::path expectedRoot = std::filesystem::current_path();
  EXPECT_EQ(expectedRoot, createdDir.parent_path());
  EXPECT_EQ("converters-tmp.tcb",
            fs::path(tempFilePath).filename().string());
  EXPECT_TRUE(std::filesystem::is_directory(createdDir));

  std::ofstream out(tempFilePath.c_str());
  ASSERT_TRUE(out.is_open());
  out << "tmp";
  out.close();
  EXPECT_TRUE(std::filesystem::exists(tempFilePath));

  TmpDir::cleanupAll();
  EXPECT_FALSE(std::filesystem::exists(createdDir));
}

TEST(TmpDirTest, UsesConfiguredTempFolderWhenSet) {
  ScopedTempDir cwdDir;
  ScopedTempDir configuredDir;
  ScopedCurrentPath cwd(cwdDir.path());

  TmpDir::setTempRoot(configuredDir.path().string());

  std::string tempFilePath;
  std::string tempDirPath;
  TmpDir::createTempFile(tempFilePath, tempDirPath);

  ASSERT_FALSE(tempFilePath.empty());
  ASSERT_FALSE(tempDirPath.empty());

  fs::path createdDir(tempDirPath);
  EXPECT_EQ(configuredDir.path(), createdDir.parent_path());
  EXPECT_EQ("converters-tmp.tcb",
            fs::path(tempFilePath).filename().string());
  EXPECT_TRUE(std::filesystem::is_directory(createdDir));

  TmpDir::cleanupAll();
  EXPECT_FALSE(std::filesystem::exists(createdDir));
  TmpDir::setTempRoot("");
}

TEST(TmpDirTest, ObjectLifetimeRemovesTemporaryDirectory) {
  fs::path createdDir;
  {
    TmpDir tempDir;
    createdDir = tempDir.getPath();
    ASSERT_TRUE(fs::is_directory(createdDir));
    std::ofstream out(createdDir / "contents.txt");
    ASSERT_TRUE(out.is_open());
    out << "temporary";
  }
  EXPECT_FALSE(fs::exists(createdDir));
}
