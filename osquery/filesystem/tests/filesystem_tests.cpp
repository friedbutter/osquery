/*
 *  Copyright (c) 2014, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include <fstream>

#include <stdio.h>

#include <gtest/gtest.h>

#include <boost/property_tree/ptree.hpp>

#include <osquery/filesystem.h>
#include <osquery/logger.h>

#include "osquery/core/test_util.h"

namespace pt = boost::property_tree;

namespace osquery {
class FilesystemTests : public testing::Test {

 protected:
  void SetUp() { createMockFileStructure(); }

  void TearDown() { tearDownMockFileStructure(); }

  /// Helper method to check if a path was included in results.
  bool contains(const std::vector<std::string>& all, const std::string& n) {
    return !(std::find(all.begin(), all.end(), n) == all.end());
  }
};

TEST_F(FilesystemTests, test_plugin) {
  std::ofstream test_file(kTestWorkingDirectory + "fstests-file");
  test_file.write("test123\n", sizeof("test123"));
  test_file.close();

  std::string content;
  auto s = readFile(kTestWorkingDirectory + "fstests-file", content);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(s.toString(), "OK");
  EXPECT_EQ(content, "test123\n");

  remove(kTestWorkingDirectory + "fstests-file");
}

TEST_F(FilesystemTests, test_list_files_missing_directory) {
  std::vector<std::string> results;
  auto status = listFilesInDirectory("/foo/bar", results);
  EXPECT_FALSE(status.ok());
}

TEST_F(FilesystemTests, test_list_files_invalid_directory) {
  std::vector<std::string> results;
  auto status = listFilesInDirectory("/etc/hosts", results);
  EXPECT_FALSE(status.ok());
}

TEST_F(FilesystemTests, test_list_files_valid_directorty) {
  std::vector<std::string> results;
  auto s = listFilesInDirectory("/etc", results);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(s.toString(), "OK");
  EXPECT_TRUE(contains(results, "/etc/hosts"));
}

TEST_F(FilesystemTests, test_simple_globs) {
  std::vector<std::string> results;
  // Test the shell '*', we will support SQL's '%' too.
  auto status = resolveFilePattern(kFakeDirectory + "/*", results);
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(results.size(), 5);

  // Test the csh-style bracket syntax: {}.
  results.clear();
  resolveFilePattern(kFakeDirectory + "/{root,door}*", results);
  EXPECT_EQ(results.size(), 2);

  // Test a tilde, home directory expansion, make no asserts about contents.
  results.clear();
  resolveFilePattern("~", results);
  if (results.size() == 0) {
    LOG(WARNING) << "Tilde expansion failed.";
  }
}

TEST_F(FilesystemTests, test_wildcard_single_all) {
  // Use '%' as a wild card to glob files within the temporarily-created dir.
  std::vector<std::string> results;
  auto status = resolveFilePattern(kFakeDirectory + "/%", results, GLOB_ALL);
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(results.size(), 5);
  EXPECT_TRUE(contains(results, kFakeDirectory + "/roto.txt"));
  EXPECT_TRUE(contains(results, kFakeDirectory + "/deep11/"));
}

TEST_F(FilesystemTests, test_wildcard_single_files) {
  // Now list again with a restriction to only files.
  std::vector<std::string> results;
  resolveFilePattern(kFakeDirectory + "/%", results, GLOB_FILES);
  EXPECT_EQ(results.size(), 3);
  EXPECT_TRUE(contains(results, kFakeDirectory + "/roto.txt"));
}

TEST_F(FilesystemTests, test_wildcard_single_folders) {
  std::vector<std::string> results;
  resolveFilePattern(kFakeDirectory + "/%", results, GLOB_FOLDERS);
  EXPECT_EQ(results.size(), 2);
  EXPECT_TRUE(contains(results, kFakeDirectory + "/deep11/"));
}

TEST_F(FilesystemTests, test_wildcard_dual) {
  // Now test two directories deep with a single wildcard for each.
  std::vector<std::string> results;
  auto status = resolveFilePattern(kFakeDirectory + "/%/%", results);
  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(contains(results, kFakeDirectory + "/deep1/level1.txt"));
}

TEST_F(FilesystemTests, test_wildcard_double) {
  // TODO: this will fail.
  std::vector<std::string> results;
  auto status = resolveFilePattern(kFakeDirectory + "/%%", results);
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(results.size(), 14);
  EXPECT_TRUE(contains(results, kFakeDirectory + "/deep1/deep2/level2.txt"));
}

TEST_F(FilesystemTests, test_wildcard_double_folders) {
  std::vector<std::string> results;
  resolveFilePattern(kFakeDirectory + "/%%", results, GLOB_FOLDERS);
  EXPECT_EQ(results.size(), 5);
  EXPECT_TRUE(contains(results, kFakeDirectory + "/deep11/deep2/deep3/"));
}

TEST_F(FilesystemTests, test_wildcard_end_last_component) {
  std::vector<std::string> results;
  auto status = resolveFilePattern(kFakeDirectory + "/%11/%sh", results);
  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(contains(results, kFakeDirectory + "/deep11/not_bash"));
}

TEST_F(FilesystemTests, test_wildcard_middle_component) {
  std::vector<std::string> results;
  auto status = resolveFilePattern(kFakeDirectory + "/deep1%/%", results);
  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(contains(results, kFakeDirectory + "/deep1/level1.txt"));
  EXPECT_TRUE(contains(results, kFakeDirectory + "/deep11/level1.txt"));
}

TEST_F(FilesystemTests, test_wildcard_all_types) {
  std::vector<std::string> results;
  auto status = resolveFilePattern(kFakeDirectory + "/%p11/%/%%", results);
  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(
      contains(results, kFakeDirectory + "/deep11/deep2/deep3/level3.txt"));
}

TEST_F(FilesystemTests, test_wildcard_invalid_path) {
  std::vector<std::string> results;
  auto status = resolveFilePattern("/not_ther_abcdefz/%%", results);
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(results.size(), 0);
}

TEST_F(FilesystemTests, test_wildcard_dotdot_files) {
  std::vector<std::string> results;
  auto status = resolveFilePattern(
      kFakeDirectory + "/deep11/deep2/../../%", results, GLOB_FILES);
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(results.size(), 3);
  EXPECT_TRUE(
      contains(results, kFakeDirectory + "/deep11/deep2/../../door.txt"));
}

TEST_F(FilesystemTests, test_dotdot_relative) {
  std::vector<std::string> results;
  auto status = resolveFilePattern(kTestDataPath + "%", results);
  EXPECT_TRUE(status.ok());

  bool found = false;
  for (const auto& file : results) {
    if (file.find("test.config")) {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found);
}

TEST_F(FilesystemTests, test_no_wild) {
  std::vector<std::string> results;
  auto status =
      resolveFilePattern(kFakeDirectory + "/roto.txt", results, GLOB_FILES);
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(results.size(), 1);
  EXPECT_TRUE(contains(results, kFakeDirectory + "/roto.txt"));
}

TEST_F(FilesystemTests, test_safe_permissions) {
  // For testing we can request a different directory path.
  EXPECT_TRUE(safePermissions("/", kFakeDirectory + "/door.txt"));
  // A file with a directory.mode & 0x1000 fails.
  EXPECT_FALSE(safePermissions("/tmp", kFakeDirectory + "/door.txt"));
  // A directory for a file will fail.
  EXPECT_FALSE(safePermissions("/", kFakeDirectory + "/deep11"));
  // A root-owned file is appropriate
  EXPECT_TRUE(safePermissions("/", "/dev/zero"));
}
}
