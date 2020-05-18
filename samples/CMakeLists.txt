cmake_minimum_required(VERSION 3.13.0 FATAL_ERROR)

# See https://cmake.org/cmake/help/latest/policy/CMP0079.html for more info
cmake_policy(SET CMP0079 NEW)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Project name, version and languages
project(mimxrt1060_iot_security_sample
    VERSION 1.0.0
    LANGUAGES C ASM
)

set(LINKER_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/app/startup/MIMXRT1062xxxxx_flexspi_nor.ld")

# Include helper modules
include(utilities)

set(THREADX_ARCH "cortex_m7")
set(THREADX_TOOLCHAIN "gnu")

# Provide custom *_user.h files for configuration overrides
# set(TX_USER_FILE "${CMAKE_CURRENT_LIST_DIR}/app/tx_user.h")
set(NX_USER_FILE "${CMAKE_CURRENT_LIST_DIR}/app/nx_user.h")
# set(FX_USER_FILE "${CMAKE_CURRENT_LIST_DIR}/app/fx_user.h")
# set(UX_USER_FILE "${CMAKE_CURRENT_LIST_DIR}/app/ux_user.h")
# set(GX_USER_FILE "${CMAKE_CURRENT_LIST_DIR}/app/gx_user.h")

add_executable(${PROJECT_NAME}
    app/board/board.c
    app/board/clock_config.c
    app/board/peripherals.c
    app/board/pin_mux.c

    app/demo_iot_security_module_mqtt/demo_iot_security_module_mqtt.c

    app/newlibc/console.c
    app/newlibc/syscalls.c

    app/startup/startup_MIMXRT1062.S
    app/startup/tx_initialize_low_level.S

    app/board_init.c
    app/main.c
    app/networking.c
    app/nx_azure_iot_cert.c
    app/sntp_client.c
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
    app
    app/board
)

# Build Libraries
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../azure_iot_security_module azure_iot_security_module)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/azure-iot-preview/nx_cloud nx_cloud)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/netx_driver)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/MIMXRT1060-evk)

# Pick up the required Azure RTOS components
set(NXD_ENABLE_FILE_SERVERS OFF CACHE BOOL "Disable fileX dependency by netxduo.")
add_azrtos_component_dir(threadx)
add_azrtos_component_dir(netxduo)

# The linker output should be .elf so we can keep track of it
set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".elf")

# Apply the linker scripts to the app target
message(STATUS "Using linker script: ${LINKER_SCRIPT}")
target_link_options(${PROJECT_NAME} INTERFACE -T ${LINKER_SCRIPT})

# Must use the PUBLIC section for the app target since the dependencies are
# just pushing source to us
target_link_libraries(${PROJECT_NAME}
    PUBLIC
    
    ${azrtos_targets}
    netx_driver
    MIMXRT1060-evk
    azrtos::azure_iot_security_module
)

target_link_libraries(netxduo PRIVATE azrtos::nx_cloud)

# If not Linux or Windows, create the BIN and HEX files for flashing
add_custom_command( TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary ${PROJECT_NAME}.elf ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_OBJCOPY} -I binary -O ihex ${PROJECT_NAME}.bin ${PROJECT_NAME}.hex
    COMMENT "Building .hex and .bin file"
)
