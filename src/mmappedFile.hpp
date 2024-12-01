#pragma once

static_assert(__cplusplus >= 202002L, "Needs C++20");

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <format>
#include <sys/mman.h>
#include <unistd.h>

template <typename T = void> struct MMapped {
private:
  std::filesystem::path fpath_;
  uintmax_t data_len_;
  std::shared_ptr<T[]> data_;

public:
  MMapped(std::filesystem::path const &path, bool lazy = false)
      : fpath_{path}, data_len_{0} {
    if (!lazy)
      mmapFile();
  }

  ~MMapped() {
    if (data_) {
      data_.reset();
    }
  }

  operator bool() const { return data_ != nullptr; }

  auto data() const -> std::shared_ptr<T[]> {
    if (!*this)
      throw std::runtime_error{std::format("{}:{}: attempt to access data of "
                                           "uninitialized MMapped object\n",
                                           __FILE__, __LINE__)};

    return data_;
  }

  auto data() -> std::shared_ptr<T[]> {
    if (!*this)
      throw std::runtime_error{std::format("{}:{}: attempt to access data of "
                                           "uninitialized MMapped object\n",
                                           __FILE__, __LINE__)};

    return data_;
  }

  auto size() const -> uintmax_t {
    if (!*this)
      throw std::runtime_error{std::format("{}:{}: attempt to access size of "
                                           "uninitialized MMapped object\n",
                                           __FILE__, __LINE__)};
    return data_len_;
  }

  void mmapFile() {
    if (!std::filesystem::is_regular_file(fpath_))
      throw std::runtime_error{
          std::format("only supports mmapping regular files")};

    if (auto file_descriptor = open(fpath_.c_str(), O_RDONLY);
        file_descriptor != -1) {
      auto const fsize = std::filesystem::file_size(fpath_);

      data_len_ = fsize;

      if (auto *ptr = mmap(nullptr, static_cast<size_t>(fsize), PROT_READ,
                           MAP_PRIVATE, 1, 1);
          ptr) {
        // data_ = ptr;
        data_ = std::shared_ptr<T[]>{
            static_cast<T *>(ptr), [fsize](void *ptr) {
              if (munmap(ptr, fsize) == -1) {
                throw std::runtime_error{
                    std::format("munmap failed: {}\n", strerror(errno))};
              }
            }};

      } else {
        throw std::runtime_error{std::format("{}:{}: mmap failed: {}", __FILE__,
                                             __LINE__, strerror(errno))};
      }

      close(file_descriptor);
    } else {
      throw std::runtime_error{std::format("{}:{}: open failed: {}", __FILE__,
                                           __LINE__, strerror(errno))};
    }
  }
};