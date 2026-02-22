cmake_minimum_required(VERSION 3.28)
project(changelog VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.15.1
)
FetchContent_Declare(
  argparse
  GIT_REPOSITORY https://github.com/p-ranav/argparse.git
  GIT_TAG v3.1
)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_CLI OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(USE_SSH OFF CACHE BOOL "" FORCE)
set(USE_HTTPS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  libgit2
  GIT_REPOSITORY https://github.com/libgit2/libgit2.git
  GIT_TAG v1.9.2
)
FetchContent_MakeAvailable(spdlog argparse libgit2)

add_executable(changelog src/main.cc src/changelog.cc)
target_link_libraries(changelog PRIVATE spdlog::spdlog argparse::argparse libgit2package)
target_include_directories(changelog PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${libgit2_SOURCE_DIR}/include
)

target_compile_definitions(changelog PRIVATE
  CHANGELOG_VERSION="${PROJECT_VERSION}"
)
target_compile_options(changelog PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)
