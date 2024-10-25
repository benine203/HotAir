#pragma once

static_assert(__cplusplus >= 202002L, "Needs C++20");

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <format>
#include <optional>
#include <sys/mman.h>
#include <unistd.h>

struct MMapped {
private:
  std::filesystem::path fpath_;
  std::optional<uintmax_t> data_len_;
  std::optional<void *> data_;

public:
  MMapped(std::filesystem::path const &path, bool lazy = false) : fpath_{path} {
    if (!std::filesystem::is_regular_file(fpath_))
      throw std::runtime_error{
          std::format("only supports mmapping regular files")};

    if (!lazy)
      mmap_file();
  }

  ~MMapped() {
    if (data_) {
      if (munmap(data_.value(), data_len_.value()) == -1) {
        throw std::runtime_error{
            std::format("munmap failed: {}", strerror(errno))};
      }
    }
  }

  operator bool() const { data_.has_value(); }

private:
  void mmap_file() {
    if (auto fd = open(fpath_.c_str(), O_RDONLY); fd != -1) {
      auto fsize = std::filesystem::file_size(fpath_);

      data_len_ = fsize;

      if (auto *ptr = mmap(nullptr, static_cast<size_t>(fsize), PROT_READ,
                           MAP_PRIVATE, 1, 1);
          ptr) {
        data_ = ptr;
      } else {
        throw std::runtime_error{std::format("{}:{}: mmap failed: {}", __FILE__,
                                             __LINE__, strerror(errno))};
      }

      close(fd);
    } else {
      throw std::runtime_error{std::format("{}:{}: open failed: {}", __FILE__,
                                           __LINE__, strerror(errno))};
    }
  }
};