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
#include <vector>

#include <boost/filesystem.hpp>
#include <zlib.h>

#include "InputFileResolver.h"
#include "MyException.h"

namespace {

class ScopedTempDir {
 public:
  ScopedTempDir() {
    path_ = boost::filesystem::temp_directory_path() /
            boost::filesystem::unique_path("percolator-resolver-%%%%-%%%%");
    boost::filesystem::create_directories(path_);
  }

  ~ScopedTempDir() { boost::filesystem::remove_all(path_); }

  boost::filesystem::path path() const { return path_; }

 private:
  boost::filesystem::path path_;
};

void writeTextFile(const boost::filesystem::path& path,
                   const std::string& content) {
  std::ofstream out(path.string().c_str(), std::ios::binary);
  ASSERT_TRUE(out.is_open());
  out << content;
  out.close();
}

}  // namespace

TEST(InputFileResolverTest, ReadInputFileListParsesCommentsAndWhitespace) {
  ScopedTempDir tempDir;
  boost::filesystem::path listPath = tempDir.path() / "inputs.txt";

  writeTextFile(listPath,
                "   # comment\n"
                "\n"
                "  a.pin.gz  \n"
                "sub dir/file 2.pin\n"
                " \t # another comment\n"
                "pattern/*.pin.gz\n");

  std::vector<std::string> entries =
      InputFileResolver::readInputFileList(listPath.string());
  ASSERT_EQ(static_cast<size_t>(3), entries.size());
  EXPECT_EQ("a.pin.gz", entries[0]);
  EXPECT_EQ("sub dir/file 2.pin", entries[1]);
  EXPECT_EQ("pattern/*.pin.gz", entries[2]);
}

TEST(InputFileResolverTest, ExpandPatternsMatchesWildcardEntries) {
  ScopedTempDir tempDir;
  boost::filesystem::create_directories(tempDir.path() / "a");
  boost::filesystem::create_directories(tempDir.path() / "b");
  writeTextFile(tempDir.path() / "a" / "one.pin.gz", "x");
  writeTextFile(tempDir.path() / "b" / "two.pin.gz", "x");

  std::string pattern = (tempDir.path() / "*" / "*.pin.gz").string();
  std::vector<std::string> expanded =
      InputFileResolver::expandPatterns(std::vector<std::string>(1, pattern));

  ASSERT_EQ(static_cast<size_t>(2), expanded.size());
  EXPECT_EQ((tempDir.path() / "a" / "one.pin.gz").string(), expanded[0]);
  EXPECT_EQ((tempDir.path() / "b" / "two.pin.gz").string(), expanded[1]);
}

TEST(InputFileResolverTest, ExpandPatternsThrowsOnUnmatchedWildcard) {
  ScopedTempDir tempDir;
  std::string pattern = (tempDir.path() / "*" / "*.pin.gz").string();
  EXPECT_THROW(
      InputFileResolver::expandPatterns(std::vector<std::string>(1, pattern)),
      MyException);
}

TEST(InputFileResolverTest, DeduplicateKeepFirstPreservesOrder) {
  ScopedTempDir tempDir;
  boost::filesystem::path dir = tempDir.path() / "data";
  boost::filesystem::create_directories(dir);
  boost::filesystem::path filePath = dir / "in.pin";
  writeTextFile(filePath, "x");

  boost::filesystem::path equivalent = dir / "." / "in.pin";
  std::vector<std::string> input;
  input.push_back(filePath.string());
  input.push_back(equivalent.string());
  input.push_back(filePath.string());

  std::vector<std::string> deduped =
      InputFileResolver::deduplicateKeepFirst(input);

  ASSERT_EQ(static_cast<size_t>(1), deduped.size());
  EXPECT_EQ(filePath.string(), deduped[0]);
}

TEST(InputFileResolverTest, MaterializeGzipInputsCreatesTabReadableFile) {
  ScopedTempDir tempDir;
  boost::filesystem::path gzPath = tempDir.path() / "input.pin.gz";

  std::string contents =
      "id\tLabel\tFeature\tPeptide\tProtein\n"
      "id1\t1\t0\t\tP\n";
  gzFile out = gzopen(gzPath.string().c_str(), "wb");
  ASSERT_TRUE(out != NULL);
  ASSERT_GT(gzwrite(out, contents.c_str(), static_cast<unsigned int>(contents.size())), 0);
  ASSERT_EQ(Z_OK, gzclose(out));

  std::vector<std::string> materialized =
      InputFileResolver::materializeGzipInputs(
          std::vector<std::string>(1, gzPath.string()));
  ASSERT_EQ(static_cast<size_t>(1), materialized.size());
  EXPECT_NE(gzPath.string(), materialized[0]);

  std::ifstream in(materialized[0].c_str());
  ASSERT_TRUE(in.is_open());
  std::string firstLine;
  ASSERT_TRUE(static_cast<bool>(std::getline(in, firstLine)));
  EXPECT_NE(std::string::npos, firstLine.find('\t'));
}
