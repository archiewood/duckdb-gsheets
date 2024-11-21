cmake_minimum_required(VERSION 3.5)

# Set extension name here
set(TARGET_NAME gsheets)

# Find OpenSSL package
find_package(OpenSSL REQUIRED)

set(EXTENSION_NAME ${TARGET_NAME}_extension)
set(LOADABLE_EXTENSION_NAME ${TARGET_NAME}_loadable_extension)

project(${TARGET_NAME})
include_directories(src/include)
include_directories(third_party)

# Update the EXTENSION_SOURCES to include the new gsheets_auth.cpp file
set(EXTENSION_SOURCES 
    src/gsheets_extension.cpp
    src/gsheets_auth.cpp
    src/gsheets_copy.cpp
    src/gsheets_requests.cpp
    src/gsheets_read.cpp
    src/gsheets_utils.cpp
    src/gsheets_get_token.cpp
)

build_static_extension(${TARGET_NAME} ${EXTENSION_SOURCES})
build_loadable_extension(${TARGET_NAME} " " ${EXTENSION_SOURCES})

# Link OpenSSL in both the static library as the loadable extension
target_link_libraries(${EXTENSION_NAME} OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(${LOADABLE_EXTENSION_NAME} OpenSSL::SSL OpenSSL::Crypto)

install(
  TARGETS ${EXTENSION_NAME}
  EXPORT "${DUCKDB_EXPORT_SET}"
  LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
  ARCHIVE DESTINATION "${INSTALL_LIB_DIR}")
