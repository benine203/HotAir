#pragma once

static_assert(__cplusplus >= 202002L, "C++20 required");

#include <array>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <numeric>
#include <unordered_map>
#include <variant>

#include <getopt.h>

#include <nlohmann/json.hpp>

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

    // dervie short_opts from long_opts to minimize inconsistency
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

struct Config {
  enum class Key {
    FULLSCREEN,
    GFX_WIDTH,
    GFX_HEIGHT,
  };

  using Value = std::variant<bool, int64_t, double, std::string>;

  static inline std::string app_name = "HotAir";

private:
  static inline nlohmann::json config_doc;

  /**
   * Maps JSON Ptr keys to Config::Key and default values.
   */
  inline static std::unordered_map<Key, std::pair<std::string, Value>>
      jsonp_keymap = {{Key::FULLSCREEN, {"/display/fullscreen", false}},
                      {Key::GFX_WIDTH, {"/display/width", 800}},
                      {Key::GFX_HEIGHT, {"/display/height", 600}}};

public:
  static auto get_config_dir() {
#ifdef _WIN32
    if (auto const *appdata_dir = std::getenv("LOCALAPPDATA"); appdata_dir)
      return std::filesystem::path(appdata_dir) / app_name;

    PWSTR path;

    Co_Initialize(0);
    if (SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path) != S_OK)
      throw std::runtime_error("failed to get APPDATA directory");

    return std::filesystem::path(path) / app_name;
#endif

#if __APPLE__
    return std::filesystem::path(getenv("HOME")) /
           "Library/Application Support" / app_name;
#endif

#if __linux__
    auto *xdg_config_home = std::getenv("XDG_CONFIG_HOME");
    if (xdg_config_home)
      return std::filesystem::path(xdg_config_home) / app_name;

    return std::filesystem::path(getenv("HOME")) / ".config" / app_name;
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
    // return cached config doc
    if (!config_doc.is_null())
      return config_doc;

    auto const config_file = get_config_file();
    if (Args::verbose() > 0)
      std::cerr << std::format("loading config file: {}\n",
                               config_file.native());

    // create default config file if it doesn't exist
    if (!std::filesystem::exists(config_file)) {
      if (Args::verbose() > 0)
        std::cerr << std::format("warning: config file does not exist: {}\n",
                                 config_file.native());

      std::cerr << std::format("writing default config file: {}\n",
                               config_file.native());

      config_doc = nlohmann::json::object();

      for (auto &[key, map_entry] : jsonp_keymap) {
        set(key, map_entry.second);
      }
    }

    // (re-)load the config file
    std::ifstream ifs(config_file);
    if (!ifs)
      throw std::runtime_error("failed to open config file for reading");

    ifs >> config_doc;

    return config_doc;
  }

  static void write_out() {
    auto const config_file = get_config_file();

    std::ofstream ofs(config_file);
    if (!ofs)
      throw std::runtime_error("failed to open config file for writing");

    ofs << config_doc.dump();
  }

  static Value get(Key key) {
    if (config_doc.is_null())
      load();

    // our JSONptr mapping entry for this key
    auto map_entry = jsonp_keymap.at(key);

    // if this key isn't in the config (e.g. because it was added in a newer
    // version) write it out to config
    if (!config_doc.contains(nlohmann::json::json_pointer(map_entry.first))) {
      std::cerr << std::format(
          "warning: key {} not found in config. defaulting\n", map_entry.first);

      set(key, map_entry.second);
    }

    // lookup entry in JSON
    auto cfg_item = config_doc[nlohmann::json::json_pointer(map_entry.first)];

    switch (cfg_item.type()) {
    case nlohmann::json::value_t::boolean:
      return cfg_item.get<bool>();
    case nlohmann::json::value_t::number_integer:
    case nlohmann::json::value_t::number_unsigned:
      return cfg_item.get<int64_t>();
    case nlohmann::json::value_t::number_float:
      return cfg_item.get<double>();
    case nlohmann::json::value_t::string:
      return cfg_item.get<std::string>();
    default:
      throw std::runtime_error(
          std::format("{}:{}: unsupported config value for key {}: {}\n",
                      __FILE__, __LINE__, map_entry.first, cfg_item.dump()));
    }
  }

  static void set(Key key, Value value) {
    if (config_doc.is_null())
      load();

    std::visit(
        [&](auto &&arg) {
          auto map_entry = jsonp_keymap.at(key);
          auto const json_key = map_entry.first;

          using T = std::decay_t<decltype(arg)>;

          if constexpr (std::is_same_v<T, bool>) {
            if (std::get_if<bool>(&map_entry.second) == nullptr)
              throw std::runtime_error(
                  std::format("attempt to set bool value for key {} with "
                              "non-bool default value\n",
                              json_key));
            config_doc[nlohmann::json::json_pointer(json_key)] = arg;
          } else if constexpr (std::is_same_v<T, int64_t>) {
            if (std::get_if<int64_t>(&map_entry.second) == nullptr)
              throw std::runtime_error(
                  std::format("attempt to set int64_t value for key {} with "
                              "non-int64_t default value\n",
                              json_key));
            config_doc[nlohmann::json::json_pointer(json_key)] = arg;
          } else if constexpr (std::is_same_v<T, double>) {
            if (std::get_if<double>(&map_entry.second) == nullptr)
              throw std::runtime_error(
                  std::format("attempt to set double value for key {} with "
                              "non-double default value\n",
                              json_key));
            config_doc[nlohmann::json::json_pointer(json_key)] = arg;
          } else if constexpr (std::is_same_v<T, std::string>) {
            if (std::get_if<std::string>(&map_entry.second) == nullptr)
              throw std::runtime_error(
                  std::format("attempt to set string value for key {} with "
                              "non-string default value\n",
                              json_key));
            config_doc[nlohmann::json::json_pointer(json_key)] = arg;
          } else
            static_assert(std::false_type::value, "non-exhaustive visitor");
        },
        value);

    write_out();
  }
};