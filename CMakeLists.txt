cmake_minimum_required(VERSION 3.23 FATAL_ERROR)

project(FuzeHttp
  VERSION 0.1.4
  DESCRIPTION "Web framework"
  LANGUAGES C CXX
  # HOMEPAGE_URL "https://fuze.page/software/mediaboard"
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread") # Required on FreeBSD
set(CMAKE_EXPORT_COMPILE_COMMANDS 1) # writes compile_commands.json for clangd to use
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(SODIUM_DISABLE_TESTS ON)
option(FUZEDBI_USE_POSTGRES "Use PostgreSQL interface instead of SQLite" OFF)
include(GNUInstallDirs)

# Find dependencies sneeded by the project
cmake_policy(SET CMP0167 NEW) # use BoostConfig.cmake instead of legacy FindBoost, for CMake < 3.30
find_package(Boost 1.88 COMPONENTS json program_options REQUIRED)

add_library(FuzeHttp)

if(FUZEDBI_USE_POSTGRES)
  message("FuzeDBI will use PostgreSQL interface")

  find_package(PostgreSQL REQUIRED)
  include_directories (${PostgreSQL_INCLUDE_DIRS})
  link_directories(${PostgreSQL_LIBRARY_DIRS})

  target_compile_definitions(FuzeHttp PUBLIC FUZEDBI_POSTGRES)
  target_compile_definitions(FuzeHttp PUBLIC FUZEDBI_DB="postgresql")
  target_link_libraries(FuzeHttp PRIVATE PostgreSQL::PostgreSQL)
else()
  message("FuzeDBI will use SQLite interface")

  find_package(SQLite3) # CMake >= 4.3
  if(NOT TARGET SQLite3::SQLite3) # CMake < 4.3
    add_library(SQLite3::SQLite3 ALIAS SQLite::SQLite3)
  endif()

  target_compile_definitions(FuzeHttp PUBLIC FUZEDBI_SQLITE)
  target_compile_definitions(FuzeHttp PUBLIC FUZEDBI_DB="sqlite")
  target_link_libraries(FuzeHttp PRIVATE SQLite3::SQLite3)
endif()
target_link_libraries(FuzeHttp
  PRIVATE
    Boost::json
    Boost::program_options
    sodium
)
# install(
#   FILES
#     config.ini
#     database_template.sql
#   DESTINATION ${CMAKE_INSTALL_DATADIR}/FuzeMediaboard
# )
add_subdirectory(dependencies)
add_subdirectory(source)
add_subdirectory(example)
# install(
#   TARGETS MediaboardServer
#   FILE_SET HEADERS
#   # RUNTIME DESTINATION bin
# )
# unset(FUZEDBI_USE_POSTGRES CACHE)
