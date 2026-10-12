cmake_minimum_required(VERSION 3.20)
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)
project(fnr_provisioning_observer C CXX ASM)
set(CMAKE_C_STANDARD 17)
if(NOT FNR_SOURCE_ID OR NOT FNR_PROVISIONING_SOURCE)
 message(FATAL_ERROR "Exact source ID and explicit public provisioning source required")
endif()
get_filename_component(FNR_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
pico_sdk_init()
find_package(picotool 2.2.0 REQUIRED CONFIG)
set(RP ${FNR_ROOT}/firmware/rp2040)
set(SH ${FNR_ROOT}/shared/src)
add_executable(fnr_provisioning_observer main.c usb_observer.c ${RP}/receiver_adapter.c ${RP}/provisioning_adapter.c
 ${FNR_PROVISIONING_SOURCE} ${SH}/datagram.c ${SH}/transport.c ${SH}/uart_buffer.c ${SH}/hid.c
 ${SH}/hid_relative_mouse.c ${SH}/envelope.c ${SH}/receiver.c ${SH}/profile_relative_mouse.c ${SH}/provisioning.c)
target_include_directories(fnr_provisioning_observer PRIVATE . ${RP} ${FNR_ROOT}/shared/include)
target_compile_definitions(fnr_provisioning_observer PRIVATE
 CFG_TUSB_CONFIG_FILE="${CMAKE_CURRENT_LIST_DIR}/tusb_observer_config.h"
 FNR_PRODUCTION_MAIN="${RP}/main.c" FNR_PRODUCTION_USB="${RP}/usb_adapter.c"
 FNR_PRODUCTION_TRANSPORT="${RP}/transport_adapter.c"
 FNR_SOURCE_ID="${FNR_SOURCE_ID}")
target_link_libraries(fnr_provisioning_observer PRIVATE pico_stdlib tinyusb_device hardware_uart hardware_irq pico_unique_id)
pico_enable_stdio_usb(fnr_provisioning_observer 0)
pico_enable_stdio_uart(fnr_provisioning_observer 0)
pico_add_extra_outputs(fnr_provisioning_observer)
