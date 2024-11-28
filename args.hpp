#pragma once

static_assert(__cplusplus >= 202002L, "C++20 required");

#include <array>
#include <filesystem>
#include <format>
#include <iostream>
#include <numeric>

#include <getopt.h>

#include <simdjson.h>

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
                          if (!opt.name)
                            return acc;
                          return acc + (char)opt.val;
                        });

    static auto const usage =
        std::format("Usage: {} -[{}]\n", argv[0], short_opts);

    static auto const help =
        std::format("{}\n"
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

class Config {
  static inline std::shared_ptr<simdjson::dom::element> config_doc = nullptr;

public:
  static auto get_config_dir() {
#ifdef _WIN32
    if (auto const *appdata_dir = std::getenv("LOCALAPPDATA"); appdata_dir)
      return std::filesystem::path(appdata_dir) / "HotAir";

    PWSTR path;

    Co_Initialize(0);
    if (SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path) != S_OK)
      throw std::runtime_error("failed to get APPDATA directory");

    return std::filesystem::path(path) / "HotAir";
#endif

#if __APPLE__
    return std::filesystem::path(getenv("HOME")) /
           "Library/Application Support" / "HotAir";
#endif

#if __linux__
    auto *xdg_config_home = std::getenv("XDG_CONFIG_HOME");
    if (xdg_config_home)
      return std::filesystem::path(xdg_config_home) / "HotAir";

    return std::filesystem::path(getenv("HOME")) / ".config" / "HotAir";
#endif

    throw std::runtime_error("unsupported platform");
  }

  static auto get_config_file() {
    auto const config_dir = get_config_dir();
    if (!std::filesystem::exists(config_dir))
      std::filesystem::create_directories(config_dir);

    if (!std::filesystem::is_directory(config_dir))
      throw std::runtime_error("config directory is not a directory");

    return config_dir / "config.json";
  }

  static auto load() {
    if (config_doc)
      return config_doc;

    config_doc =
        std::make_shared<simdjson::dom::element>(simdjson::dom::object{});

    auto const config_file = get_config_file();

    if (Args::verbose() > 0)
      std::cerr << std::format("loading config file: {}\n",
                               config_file.native());

    if (!std::filesystem::exists(config_file)) {
      if (Args::verbose() > 0)
        std::cerr << std::format("warning: config file does not exist: {}\n",
                                 config_file.native());
      return config_doc;
    }

    simdjson::dom::parser parser;
    simdjson::dom::element doc;
    if (auto result = parser.load(config_file); result.error())
      throw std::runtime_error(std::format("failed to load config file: {}\n",
                                           error_message(result.error())));
    else
      *config_doc = std::move(result.value());

    return config_doc;
  }
};