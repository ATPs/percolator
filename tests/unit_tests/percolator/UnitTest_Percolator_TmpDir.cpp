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

#include <fstream>
#include <string>

#include <boost/filesystem.hpp>

#include "TmpDir.h"

namespace {

class ScopedTempDir {
 public:
  ScopedTempDir() {
    path_ = boost::filesystem::temp_directory_path() /
            boost::filesystem::unique_path("percolator-tmpdir-%%%%-%%%%");
    boost::filesystem::create_directories(path_);
  }

  ~ScopedTempDir() { boost::filesystem::remove_all(path_); }

  boost::filesystem::path path() const { return path_; }

 private:
  boost::filesystem::path path_;
};

class ScopedCurrentPath {
 public:
  explicit ScopedCurrentPath(const boost::filesystem::path& newPath)
      : originalPath_(boost::filesystem::current_path()) {
    boost::filesystem::current_path(newPath);
  }

  ~ScopedCurrentPath() { boost::filesystem::current_path(originalPath_); }

 private:
  boost::filesystem::path originalPath_;
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

  boost::filesystem::path createdDir(tempDirPath);
  boost::filesystem::path expectedRoot = boost::filesystem::current_path();
  EXPECT_EQ(expectedRoot, createdDir.parent_path());
  EXPECT_EQ("converters-tmp.tcb",
            boost::filesystem::path(tempFilePath).filename().string());
  EXPECT_TRUE(boost::filesystem::is_directory(createdDir));

  std::ofstream out(tempFilePath.c_str());
  ASSERT_TRUE(out.is_open());
  out << "tmp";
  out.close();
  EXPECT_TRUE(boost::filesystem::exists(tempFilePath));

  TmpDir::cleanupAll();
  EXPECT_FALSE(boost::filesystem::exists(createdDir));
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

  boost::filesystem::path createdDir(tempDirPath);
  EXPECT_EQ(configuredDir.path(), createdDir.parent_path());
  EXPECT_EQ("converters-tmp.tcb",
            boost::filesystem::path(tempFilePath).filename().string());
  EXPECT_TRUE(boost::filesystem::is_directory(createdDir));

  TmpDir::cleanupAll();
  EXPECT_FALSE(boost::filesystem::exists(createdDir));
  TmpDir::setTempRoot("");
}
