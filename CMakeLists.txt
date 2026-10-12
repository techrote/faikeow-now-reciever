cmake_minimum_required(VERSION 3.20)
project(fnr_native LANGUAGES C CXX)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(CTest)
add_library(fnr_shared STATIC
  shared/src/datagram.c
  shared/src/radio_ingress.c
  shared/src/transport.c
  shared/src/transport_tx.c
  shared/src/uart_buffer.c
  shared/src/hid.c
  shared/src/hid_relative_mouse.c
  shared/src/envelope.c
  shared/src/receiver.c
  shared/src/profile_relative_mouse.c
  shared/src/provisioning.c)
target_include_directories(fnr_shared PUBLIC shared/include)
target_compile_options(fnr_shared PRIVATE -Wall -Wextra -Werror -Wconversion -Wshadow)
add_executable(fnr_native_tests tests/native/test_datagram.cpp)
target_link_libraries(fnr_native_tests PRIVATE fnr_shared)
target_compile_options(fnr_native_tests PRIVATE -Wall -Wextra -Werror -Wconversion -Wshadow)
add_test(NAME platform_ownership_and_bounds COMMAND fnr_native_tests)
add_executable(fnr_radio_ingress_tests tests/native/test_radio_ingress.cpp)
target_link_libraries(fnr_radio_ingress_tests PRIVATE fnr_shared)
target_compile_options(fnr_radio_ingress_tests PRIVATE -Wall -Wextra -Werror -Wconversion -Wshadow)
add_test(NAME radio_ingress_bounds_ownership_loss COMMAND fnr_radio_ingress_tests)
add_executable(fnr_hid_tests tests/native/test_hid.cpp)
target_link_libraries(fnr_hid_tests PRIVATE fnr_shared)
target_compile_options(fnr_hid_tests PRIVATE -Wall -Wextra -Werror -Wconversion -Wshadow)
add_test(NAME hid_framework_and_relative_mouse COMMAND fnr_hid_tests)
add_executable(fnr_link_diag_tests tests/native/test_link_diag.cpp)
target_include_directories(fnr_link_diag_tests PRIVATE diagnostics/common)
target_compile_options(fnr_link_diag_tests PRIVATE -Wall -Wextra -Werror)
add_test(NAME diagnostic_link_crc_bounds_resync COMMAND fnr_link_diag_tests)
add_executable(fnr_rf_diag_tests tests/native/test_rf_diag.cpp)
target_include_directories(fnr_rf_diag_tests PRIVATE diagnostics/common)
target_compile_options(fnr_rf_diag_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
add_test(NAME diagnostic_rf_payload_integrity COMMAND fnr_rf_diag_tests)
add_executable(fnr_transport_tests tests/native/test_transport.cpp)
target_link_libraries(fnr_transport_tests PRIVATE fnr_shared)
target_compile_options(fnr_transport_tests PRIVATE -Wall -Wextra -Werror -Wconversion -Wshadow -UNDEBUG)
add_test(NAME transport_codec_stream_ownership_properties COMMAND fnr_transport_tests)
add_executable(fnr_transport_pipeline_tests tests/native/test_transport_pipeline.cpp)
target_link_libraries(fnr_transport_pipeline_tests PRIVATE fnr_shared)
target_compile_options(fnr_transport_pipeline_tests PRIVATE -Wall -Wextra -Werror -Wconversion -Wshadow -UNDEBUG)
add_test(NAME transport_tx_uart_pipeline COMMAND fnr_transport_pipeline_tests)
add_executable(fnr_transport_esp_adapter_tests
  tests/native/test_transport_esp_adapter.cpp
  firmware/esp8266/transport_adapter.cpp)
target_include_directories(fnr_transport_esp_adapter_tests PRIVATE
  tests/native/fake_esp firmware/esp8266)
target_link_libraries(fnr_transport_esp_adapter_tests PRIVATE fnr_shared)
target_compile_options(fnr_transport_esp_adapter_tests PRIVATE
  -Wall -Wextra -Werror -Wconversion -Wshadow -UNDEBUG)
add_test(NAME transport_esp_production_adapter COMMAND fnr_transport_esp_adapter_tests)
add_executable(fnr_transport_rp_adapter_tests tests/native/test_transport_rp_adapter.c)
target_include_directories(fnr_transport_rp_adapter_tests PRIVATE
  tests/native/fake_pico firmware/rp2040)
target_link_libraries(fnr_transport_rp_adapter_tests PRIVATE fnr_shared)
target_compile_options(fnr_transport_rp_adapter_tests PRIVATE
  -Wall -Wextra -Werror -Wconversion -Wshadow -UNDEBUG)
add_test(NAME transport_rp_production_adapter COMMAND fnr_transport_rp_adapter_tests)

add_executable(fnr_receiver_tests tests/native/test_receiver.cpp)
target_link_libraries(fnr_receiver_tests PRIVATE fnr_shared)
target_compile_options(fnr_receiver_tests PRIVATE -Wall -Wextra -Werror -Wconversion -Wshadow -UNDEBUG)
add_test(NAME receiver_envelope_order_freshness_relative_mouse COMMAND fnr_receiver_tests)

add_executable(fnr_receiver_rp_adapter_tests tests/native/test_receiver_rp_adapter.c)
target_include_directories(fnr_receiver_rp_adapter_tests PRIVATE
  tests/native/fake_pico firmware/rp2040)
target_link_libraries(fnr_receiver_rp_adapter_tests PRIVATE fnr_shared)
target_compile_options(fnr_receiver_rp_adapter_tests PRIVATE
  -Wall -Wextra -Werror -Wconversion -Wshadow -UNDEBUG)
add_test(NAME receiver_production_rp_consumer_hook COMMAND fnr_receiver_rp_adapter_tests)

add_executable(fnr_provisioning_tests tests/native/test_provisioning.cpp)
target_link_libraries(fnr_provisioning_tests PRIVATE fnr_shared)
target_compile_options(fnr_provisioning_tests PRIVATE
  -Wall -Wextra -Werror -Wconversion -Wshadow -UNDEBUG)
add_test(NAME provisioning_record_crc_corruption_properties COMMAND fnr_provisioning_tests)

add_executable(fnr_provisioning_rp_adapter_tests tests/native/test_provisioning_rp_adapter.c)
target_include_directories(fnr_provisioning_rp_adapter_tests PRIVATE firmware/rp2040)
target_link_libraries(fnr_provisioning_rp_adapter_tests PRIVATE fnr_shared)
target_compile_options(fnr_provisioning_rp_adapter_tests PRIVATE
  -Wall -Wextra -Werror -Wconversion -Wshadow -UNDEBUG)
add_test(NAME provisioning_rp_public_loader COMMAND fnr_provisioning_rp_adapter_tests)

add_executable(fnr_provisioning_esp_adapter_tests tests/native/test_provisioning_esp_adapter.cpp)
target_include_directories(fnr_provisioning_esp_adapter_tests PRIVATE firmware/esp8266)
target_link_libraries(fnr_provisioning_esp_adapter_tests PRIVATE fnr_shared)
target_compile_options(fnr_provisioning_esp_adapter_tests PRIVATE
  -Wall -Wextra -Werror -Wconversion -Wshadow -UNDEBUG)
add_test(NAME provisioning_esp_secret_loader COMMAND fnr_provisioning_esp_adapter_tests)

add_executable(fnr_radio_provisioning_adapter_tests
  tests/native/test_radio_provisioning_adapter.cpp)
target_include_directories(fnr_radio_provisioning_adapter_tests PRIVATE
  tests/native/fake_esp firmware/esp8266)
target_link_libraries(fnr_radio_provisioning_adapter_tests PRIVATE fnr_shared)
target_compile_options(fnr_radio_provisioning_adapter_tests PRIVATE
  -Wall -Wextra -Werror -Wconversion -Wshadow -UNDEBUG)
add_test(NAME provisioning_actual_esp_radio_adapter COMMAND fnr_radio_provisioning_adapter_tests)
