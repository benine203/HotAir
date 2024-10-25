#pragma once

static_assert(__cplusplus >= 202002L, "C++20 required");

#include <getopt.h>

#include <array>
#include <format>
#include <iostream>
#include <numeric>

class Args {
  struct GlobalState {
    int verbose;
    GlobalState() : verbose(0) {}
  };

 protected:
  static inline GlobalState global;

 public:
  static auto &verbose() { return global.verbose; }

  static void parse(int argc, char **argv) {
    static auto const long_opts =
        std::array{option{"verbose", no_argument, nullptr, 'v'},
                   option{"help", no_argument, nullptr, 'h'},
                   option{nullptr, 0, nullptr, 0}};

    static auto const short_opts =
        std::accumulate(long_opts.begin(), long_opts.end(), std::string{},
                        [](std::string acc, const option &opt) {
                          if (!opt.name) return acc;
                          return acc + (char)opt.val;
                        });

    static auto const usage =
        std::format("Usage: {} -[{}]\n", argv[0], short_opts);

    static auto const help = std::format(
        "{}\n"
        "Options:\n"
        "  -h, --help     display this help and exit\n"
        "  -v, --verbose  increase verbosity\n",
        usage);

    for (int opt; (opt = getopt_long(argc, argv, short_opts.c_str(),
                                     long_opts.data(), nullptr)) != -1;) {
      switch (opt) {
        case 'v':
          verbose()++;
          break;
        case 'h':
          std::cout << help;
          exit(EXIT_SUCCESS);
        default:
          std::cerr << help;
          exit(EXIT_FAILURE);
      }
    }
  }
};
