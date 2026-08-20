cmake_minimum_required(VERSION 3.15)
project(GoldMapper LANGUAGES C)
set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
if(MSVC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
  add_compile_options(/W3)
else()
  add_compile_options(-Wall -Wextra)
endif()
add_subdirectory(third_party/minhook)
add_library(GoldMapperLib SHARED
    src/GoldMapperLib/dllmain.c
    src/GoldMapperLib/gm_config.c
    src/GoldMapperLib/gm_hook.c
    third_party/cJSON/cJSON.c
)

target_include_directories(GoldMapperLib PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/minhook/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cJSON
)

target_link_libraries(GoldMapperLib PRIVATE minhook xinput9_1_0 user32 ole32)
add_executable(GoldMapperLauncher
    src/GoldMapperLauncher/main.c
)

target_link_libraries(GoldMapperLauncher PRIVATE kernel32 advapi32)
