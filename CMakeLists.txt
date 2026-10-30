cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)
project(usb2sun C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# USB host port selection.
#
# By default the USB host runs on two GPIOs via Pico-PIO-USB (bit-banged with the
# RP2040's PIO blocks), which leaves the Pico's own micro-USB connector free.
# Configure with -DUSB2SUN_PIO_USB=OFF to go back to using the native USB port
# through an OTG adapter.
option(USB2SUN_PIO_USB "Run the USB host on GPIOs via Pico-PIO-USB instead of the native USB port" ON)
set(USB2SUN_PIO_USB_DP_PIN 15 CACHE STRING "GPIO used for USB D+ (D- is always D+ + 1)")
set(USB2SUN_PIO_USB_VBUSEN_PIN 22 CACHE STRING "GPIO driven high to enable VBUS (leave unconnected if VBUS is wired straight to 5V)")

if (USB2SUN_PIO_USB)
  # Pico-PIO-USB is normally a submodule of TinyUSB, but the Pico SDK does not check it
  # out, so we carry our own copy as a submodule and point TinyUSB at it.
  set(PICO_PIO_USB_PATH "${CMAKE_CURRENT_LIST_DIR}/Pico-PIO-USB" CACHE PATH "Path to Pico-PIO-USB")
  if (NOT EXISTS "${PICO_PIO_USB_PATH}/src/pio_usb.c")
    message(FATAL_ERROR
      "Pico-PIO-USB not found at ${PICO_PIO_USB_PATH}.\n"
      "Run 'git submodule update --init' in ${CMAKE_CURRENT_LIST_DIR}, "
      "or pass -DPICO_PIO_USB_PATH=<path>, or build with -DUSB2SUN_PIO_USB=OFF "
      "to use the native USB port.")
  endif()
endif()

pico_sdk_init()

add_executable(usb2sun
  main.c
  hid_app.c
  hid_layout.c
  mouse.c
  keyboard.c
)
target_include_directories(usb2sun PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(usb2sun PUBLIC pico_stdlib tinyusb_host tinyusb_board)

if (USB2SUN_PIO_USB)
  if (NOT TARGET tinyusb_pico_pio_usb)
    message(FATAL_ERROR "TinyUSB did not pick up Pico-PIO-USB from ${PICO_PIO_USB_PATH}")
  endif()
  target_link_libraries(usb2sun PUBLIC tinyusb_pico_pio_usb)
  target_compile_definitions(usb2sun PUBLIC
    CFG_TUH_RPI_PIO_USB=1
    PICO_DEFAULT_PIO_USB_DP_PIN=${USB2SUN_PIO_USB_DP_PIN}
    PICO_DEFAULT_PIO_USB_VBUSEN_PIN=${USB2SUN_PIO_USB_VBUSEN_PIN}
  )
  message(STATUS "USB2Sun: USB host on PIO, D+ = GP${USB2SUN_PIO_USB_DP_PIN}, D- = GP${USB2SUN_PIO_USB_DP_PIN}+1")
else()
  target_compile_definitions(usb2sun PUBLIC CFG_TUH_RPI_PIO_USB=0)
  message(STATUS "USB2Sun: USB host on native USB port")
endif()

pico_enable_stdio_usb(usb2sun 0)
pico_enable_stdio_uart(usb2sun 0)
pico_set_program_url(usb2sun "https://github.com/jgilje/usb2sun")
pico_add_extra_outputs(usb2sun)
