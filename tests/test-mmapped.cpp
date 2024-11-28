#include <format>
#include <fstream>
#include <gtest/gtest.h>

#include "../src/mmappedFile.hpp"

TEST(TestMMappedFile, NonExistentFile) {
  EXPECT_THROW(MMapped<char>{"/path/to/non-existent-file"}, std::runtime_error);
}

TEST(TestMMappedFile, LazyNonExistentFile) {
  auto mmapped = MMapped<char>{"/path/to/non-existent-file", true};
  ASSERT_FALSE(mmapped);
}

TEST(TestMMappedFile, RegularFile) {
  auto const path =
      std::filesystem::temp_directory_path() /
      std::format("mmapped-file-{}",
                  std::chrono::system_clock::now().time_since_epoch().count());

  {
    std::ofstream{path} << "hello, world\n";
  }

  auto mmapped = MMapped<char>{path};
  ASSERT_TRUE(mmapped);

  auto const data = mmapped.data();
  ASSERT_TRUE(data);

  EXPECT_EQ(*data.get(), 'h');

  EXPECT_EQ(mmapped.size(), 13);

  std::filesystem::remove(path);
}

TEST(TestMMappedFile, LazyRegularFile) {
  auto const path =
      std::filesystem::temp_directory_path() /
      std::format("mmapped-file-{}",
                  std::chrono::system_clock::now().time_since_epoch().count());

  {
    std::ofstream{path} << "hello, world\n";
  }

  auto mmapped = MMapped<char>{path, true};
  ASSERT_FALSE(mmapped);

  EXPECT_THROW(mmapped.data(), std::runtime_error);

  EXPECT_THROW(mmapped.size(), std::runtime_error);

  mmapped.mmap_file();

  ASSERT_TRUE(mmapped);

  EXPECT_TRUE(mmapped.data());

  EXPECT_EQ(*mmapped.data().get(), 'h');

  EXPECT_EQ(mmapped.size(), 13);

  std::filesystem::remove(path);
}