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

#include "PerInputOutputPlanner.h"
#include "ValidateTabFile.h"

namespace {

class ScopedTempDir {
 public:
  ScopedTempDir() {
    path_ = boost::filesystem::temp_directory_path() /
            boost::filesystem::unique_path("percolator-validate-%%%%-%%%%");
    boost::filesystem::create_directories(path_);
  }

  ~ScopedTempDir() { boost::filesystem::remove_all(path_); }

  boost::filesystem::path path() const { return path_; }

 private:
  boost::filesystem::path path_;
};

void writePin(const boost::filesystem::path& path, const std::string& idValue) {
  std::ofstream out(path.string().c_str());
  ASSERT_TRUE(out.is_open());
  out << "SpecId\tLabel\tScanNr\tf1\tPeptide\tProteins\n";
  out << idValue << "\t1\t1\t0.1\tA.AAA.A\tP1\n";
  out.close();
}

}  // namespace

TEST(ValidateTabFileTest, ConcatenationCanAnnotateSourceTags) {
  ScopedTempDir tempDir;
  boost::filesystem::path pinA = tempDir.path() / "a.pin";
  boost::filesystem::path pinB = tempDir.path() / "b.pin";
  writePin(pinA, "idA");
  writePin(pinB, "idB");

  std::vector<std::string> files;
  files.push_back(pinA.string());
  files.push_back(pinB.string());

  std::vector<std::string> sourceKeys;
  sourceKeys.push_back("srcA");
  sourceKeys.push_back("srcB");

  ValidateTabFile validateTab;
  std::string merged = validateTab.concatenateMultiplePINs(files, sourceKeys);

  std::ifstream in(merged.c_str());
  ASSERT_TRUE(in.is_open());
  std::string all;
  std::string line;
  while (std::getline(in, line)) {
    all += line;
    all += "\n";
  }

  EXPECT_NE(std::string::npos,
            all.find(PerInputOutputPlanner::tagPsmId("srcA", "idA")));
  EXPECT_NE(std::string::npos,
            all.find(PerInputOutputPlanner::tagPsmId("srcB", "idB")));
}
