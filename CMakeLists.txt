# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(U2HTS C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

file(GLOB_RECURSE SOURCES "src/*.c")
list(REMOVE_ITEM SOURCES 
    ${CMAKE_CURRENT_LIST_DIR}/src/touch-controllers/controller_template.c
    # comment following line to enable dummy driver
    ${CMAKE_CURRENT_LIST_DIR}/src/touch-controllers/dummy_test.c
)
set(SOURCES ${SOURCES} u2hts_main.c)

add_executable(U2HTS ${SOURCES})

pico_set_program_name(U2HTS "U2HTS")
pico_set_program_version(U2HTS "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(U2HTS 1)
pico_enable_stdio_usb(U2HTS 0)

# Add the standard library to the build
target_link_libraries(U2HTS
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(U2HTS PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/include
)

# print memory usage after linking
target_link_options(U2HTS PRIVATE
    -Wl,--print-memory-usage
)

# Add any user requested libraries
target_link_libraries(U2HTS 
        hardware_i2c
        pico_unique_id
        tinyusb_device
        tinyusb_board
        )

target_compile_options(U2HTS PRIVATE -Wunused)

if(PICO_PLATFORM MATCHES "rp2350")
    pico_set_linker_script(U2HTS ${CMAKE_CURRENT_LIST_DIR}/memmap_rp2350.ld)
elseif(PICO_PLATFORM MATCHES "rp2040")
    pico_set_linker_script(U2HTS ${CMAKE_CURRENT_LIST_DIR}/memmap_rp2040.ld)
else()
    message(Unknown platform: ${PICO_PLATFORM})
endif()

pico_add_extra_outputs(U2HTS)