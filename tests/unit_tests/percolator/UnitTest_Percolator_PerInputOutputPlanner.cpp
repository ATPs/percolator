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

#include "PerInputOutputPlanner.h"

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
  ScopedTempDir() : path_(makeUniqueTempDir("percolator-output-plan-")) {}

  ~ScopedTempDir() {
    std::error_code error;
    fs::remove_all(path_, error);
  }

  fs::path path() const { return path_; }

 private:
  fs::path path_;
};

void writeText(const fs::path& path, const std::string& text) {
  std::ofstream out(path.string().c_str());
  ASSERT_TRUE(out.is_open());
  out << text;
  out.close();
}

}  // namespace

TEST(PerInputOutputPlannerTest, BuildEntriesGeneratesUniquePrefixes) {
  ScopedTempDir tempDir;
  std::filesystem::create_directories(tempDir.path() / "sub");
  writeText(tempDir.path() / "dup.pin", "x");
  writeText(tempDir.path() / "sub" / "dup.pin.gz", "x");

  std::vector<std::string> original;
  original.push_back((tempDir.path() / "dup.pin").string());
  original.push_back((tempDir.path() / "sub" / "dup.pin.gz").string());

  std::vector<std::string> materialized = original;
  std::vector<PerInputOutputPlanner::Entry> entries =
      PerInputOutputPlanner::buildEntries(original, materialized);

  ASSERT_EQ(static_cast<size_t>(2), entries.size());
  EXPECT_NE(entries[0].prefix, entries[1].prefix);
  EXPECT_NE(std::string::npos, entries[0].prefix.find("dup"));
  EXPECT_NE(std::string::npos, entries[1].prefix.find("dup"));
  EXPECT_FALSE(entries[1].prefix.empty());
  EXPECT_EQ(entries[0].prefix, entries[0].sourceKey);
  EXPECT_EQ(entries[1].prefix, entries[1].sourceKey);
}

TEST(PerInputOutputPlannerTest, EnsureOutputFolderCreatesDirectory) {
  ScopedTempDir tempDir;
  fs::path outputDir = tempDir.path() / "results" / "nested";

  bool created = false;
  std::string error;
  ASSERT_TRUE(PerInputOutputPlanner::ensureOutputFolder(outputDir.string(),
                                                        created, error));
  EXPECT_TRUE(created);
  EXPECT_TRUE(std::filesystem::exists(outputDir));
  EXPECT_TRUE(std::filesystem::is_directory(outputDir));
}

TEST(PerInputOutputPlannerTest, ReserveOutputFileCreatesDirectoryAndFile) {
  ScopedTempDir tempDir;
  fs::path outputDir = tempDir.path() / "results" / "weights";

  bool createdDir = false;
  std::string outputPath;
  std::string error;
  ASSERT_TRUE(PerInputOutputPlanner::reserveOutputFile(
      outputDir.string(), "percolator.model.weights", outputPath, createdDir,
      error));
  EXPECT_TRUE(createdDir);
  EXPECT_EQ((outputDir / "percolator.model.weights").string(), outputPath);
  EXPECT_TRUE(std::filesystem::exists(outputPath));
}

TEST(PerInputOutputPlannerTest, ReserveOutputFileFailsWhenFileExists) {
  ScopedTempDir tempDir;
  fs::path outputDir = tempDir.path() / "results";
  std::filesystem::create_directories(outputDir);
  fs::path outputFile = outputDir / "percolator.model.weights";
  writeText(outputFile, "existing");

  bool createdDir = false;
  std::string outputPath;
  std::string error;
  ASSERT_FALSE(PerInputOutputPlanner::reserveOutputFile(
      outputDir.string(), "percolator.model.weights", outputPath, createdDir,
      error));
  EXPECT_NE(std::string::npos, error.find("already exists"));
}

TEST(PerInputOutputPlannerTest, BuildFixedOutputsUsesRequestedNaming) {
  PerInputOutputPlanner::Entry entry;
  entry.prefix = "file1";

  PerInputOutputPlanner::FixedOutputs out =
      PerInputOutputPlanner::buildFixedOutputs("/tmp/out", entry, true, true,
                                               true, true);

  EXPECT_EQ("/tmp/out/file1.target.peptides.tsv", out.targetPeptides);
  EXPECT_EQ("/tmp/out/file1.decoy.peptides.tsv", out.decoyPeptides);
  EXPECT_EQ("/tmp/out/file1.target.psms.tsv", out.targetPsms);
  EXPECT_EQ("/tmp/out/file1.decoy.psms.tsv", out.decoyPsms);
}

TEST(PerInputOutputPlannerTest, TemplateOutputUsesPrefixAndBasename) {
  PerInputOutputPlanner::Entry entry;
  entry.prefix = "file1";

  std::string out = PerInputOutputPlanner::buildTemplateOutput(
      "/tmp/out", entry, "/random/path/weights.txt");
  EXPECT_EQ("/tmp/out/file1.weights.txt", out);
}

TEST(PerInputOutputPlannerTest, TaggedPsmIdRoundTrip) {
  std::string tagged = PerInputOutputPlanner::tagPsmId("sourceA", "psm-1");
  std::string source;
  std::string original;
  ASSERT_TRUE(PerInputOutputPlanner::parseTaggedPsmId(tagged, source, original));
  EXPECT_EQ("sourceA", source);
  EXPECT_EQ("psm-1", original);
}
