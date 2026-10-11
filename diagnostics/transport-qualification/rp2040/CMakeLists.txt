cmake_minimum_required(VERSION 3.20)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
project(fnr_transport_observer C CXX ASM)
set(CMAKE_C_STANDARD 17)
pico_sdk_init()
if(NOT FNR_SOURCE_ID MATCHES "^[0-9a-f]+$" OR NOT FNR_SOURCE_ID)
  message(FATAL_ERROR "Pass exact FNR_SOURCE_ID")
endif()
get_filename_component(FNR_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
add_executable(fnr_transport_observer main.c
  ${FNR_ROOT}/shared/src/datagram.c ${FNR_ROOT}/shared/src/transport.c
  ${FNR_ROOT}/shared/src/uart_buffer.c)
target_include_directories(fnr_transport_observer PRIVATE .. ${FNR_ROOT}/shared/include)
target_link_libraries(fnr_transport_observer pico_stdlib pico_unique_id hardware_uart hardware_irq)
target_compile_options(fnr_transport_observer PRIVATE -Wall -Wextra -Werror -Wconversion -Wshadow)
target_compile_definitions(fnr_transport_observer PRIVATE
  FNR_SOURCE_ID="${FNR_SOURCE_ID}"
  PICO_STDIO_USB_ENABLE_RESET_VIA_BAUD_RATE=0
  PICO_STDIO_USB_ENABLE_IRQ_BACKGROUND_TASK=0
  PICO_STDIO_USB_CONNECTION_WITHOUT_DTR=1)
pico_enable_stdio_usb(fnr_transport_observer 1)
pico_enable_stdio_uart(fnr_transport_observer 0)
pico_set_binary_type(fnr_transport_observer no_flash)
pico_set_program_name(fnr_transport_observer "FNR-005 production transport observer (SRAM)")
pico_add_extra_outputs(fnr_transport_observer)
