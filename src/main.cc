#include <argparse/argparse.hpp>
#include <spdlog/spdlog.h>

#include "changelog.h"

int main(int argc, char* argv[]) {
  argparse::ArgumentParser program("changelog", CHANGELOG_VERSION);

  program.add_argument("-r", "--repo")
      .default_value(std::string{"."})
      .help("Path to git repository");

  program.add_argument("-o", "--output")
      .default_value(std::string{"CHANGELOG.md"})
      .help("Output changelog file path");

  program.add_argument("-u", "--url")
      .default_value(std::string())
      .help(
          "Remote repository URL (e.g., "
          "https://github.com/joshiayush/changelog)");

  program.add_argument("-f", "--follow")
      .nargs(argparse::nargs_pattern::any)
      .default_value(std::vector<std::string>{})
      .help("Paths to filter commits by");

  program.add_argument("-v", "--verbose")
      .default_value(false)
      .implicit_value(true)
      .help("Enable verbose logging");

  try {
    program.parse_args(argc, argv);
  } catch (const std::exception& err) {
    spdlog::error("{}", err.what());
    std::cerr << program;
    return EXIT_FAILURE;
  }

  if (program.get<bool>("--verbose")) {
    spdlog::set_level(spdlog::level::debug);
  }

  Changelog::Config config;
  config.repo = program.get<std::string>("--repo");
  config.output = program.get<std::string>("--output");
  config.url = program.get<std::string>("--url");
  config.follow = program.get<std::vector<std::string>>("--follow");

  if (!config.url.empty() && config.url.back() == '/') {
    config.url.pop_back();
  }

  try {
    Changelog changelog(std::move(config));
    changelog.Generate();
  } catch (const std::exception& err) {
    spdlog::error("Failed to generate changelog: {}", err.what());
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
