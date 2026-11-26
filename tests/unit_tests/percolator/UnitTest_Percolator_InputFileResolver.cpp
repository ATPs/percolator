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
#include <vector>
#include <zlib.h>

#include "InputFileResolver.h"
#include "MyException.h"
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
  ScopedTempDir() : path_(makeUniqueTempDir("percolator-resolver-")) {}

  ~ScopedTempDir() {
    std::error_code error;
    fs::remove_all(path_, error);
  }

  fs::path path() const { return path_; }

 private:
  fs::path path_;
};

class ScopedTmpRoot {
 public:
  explicit ScopedTmpRoot(const fs::path& path) : path_(path) {
    fs::create_directories(path_);
    TmpDir::setTempRoot(path_.string());
  }

  ~ScopedTmpRoot() {
    TmpDir::cleanupAll();
    TmpDir::setTempRoot("");
  }

 private:
  fs::path path_;
};

void writeTextFile(const fs::path& path,
                   const std::string& content) {
  std::ofstream out(path.string().c_str(), std::ios::binary);
  ASSERT_TRUE(out.is_open());
  out << content;
  out.close();
}

bool writeGzipFile(const fs::path& path, const std::string& content) {
  gzFile out = gzopen(path.string().c_str(), "wb");
  if (out == NULL) {
    return false;
  }
  const int bytesWritten =
      gzwrite(out, content.data(), static_cast<unsigned int>(content.size()));
  const int closeStatus = gzclose(out);
  return bytesWritten == static_cast<int>(content.size()) &&
         closeStatus == Z_OK;
}

void expectGzipFailureCleansTempFiles(const fs::path& source,
                                      const fs::path& tempRoot) {
  EXPECT_THROW(InputFileResolver::materializeGzipInputs(
                   std::vector<std::string>(1, source.string())),
               MyException);

  TmpDir::cleanupAll();
  EXPECT_TRUE(fs::exists(source));
  EXPECT_TRUE(fs::is_empty(tempRoot));
}

}  // namespace

TEST(InputFileResolverTest, ReadInputFileListParsesCommentsAndWhitespace) {
  ScopedTempDir tempDir;
  fs::path listPath = tempDir.path() / "inputs.txt";

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
  std::filesystem::create_directories(tempDir.path() / "a");
  std::filesystem::create_directories(tempDir.path() / "b");
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
  fs::path dir = tempDir.path() / "data";
  std::filesystem::create_directories(dir);
  fs::path filePath = dir / "in.pin";
  writeTextFile(filePath, "x");

  fs::path equivalent = dir / "." / "in.pin";
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
  fs::path gzPath = tempDir.path() / "input.pin.gz";

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

TEST(InputFileResolverTest, MalformedGzipCleansTemporaryDirectory) {
  ScopedTempDir tempDir;
  const fs::path tempRoot = tempDir.path() / "temp";
  ScopedTmpRoot configuredRoot(tempRoot);
  const fs::path source = tempDir.path() / "malformed.pin.gz";

  // A valid gzip header followed by a DEFLATE block with reserved BTYPE=3.
  const unsigned char malformedBytes[] = {0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00,
                                          0x00, 0x00, 0x00, 0x03, 0x07};
  std::ofstream out(source, std::ios::binary);
  ASSERT_TRUE(out.is_open());
  out.write(reinterpret_cast<const char*>(malformedBytes),
            sizeof(malformedBytes));
  out.close();
  ASSERT_TRUE(out.good());

  expectGzipFailureCleansTempFiles(source, tempRoot);
}

TEST(InputFileResolverTest, TruncatedGzipFooterCleansTemporaryDirectory) {
  ScopedTempDir tempDir;
  const fs::path tempRoot = tempDir.path() / "temp";
  ScopedTmpRoot configuredRoot(tempRoot);
  const fs::path source = tempDir.path() / "truncated.pin.gz";
  const std::string contents = "valid gzip contents before truncated footer\n";
  ASSERT_TRUE(writeGzipFile(source, contents));

  const auto size = fs::file_size(source);
  ASSERT_GT(size, 4u);
  fs::resize_file(source, size - 4u);

  expectGzipFailureCleansTempFiles(source, tempRoot);
}
