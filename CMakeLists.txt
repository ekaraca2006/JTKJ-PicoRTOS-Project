# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# == EDIT at the beginning. Define the Project name and name for the main target (app).=========
set(APP_NAME JTKJ_app)
set(MAIN_TARGET hat_app)
#===============================================================================================

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work =======
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
# ================================================================================================

# == DO NOT EDIT: SETUP pico SDK and create project ==============================================
set(PICO_BOARD pico_w CACHE STRING "Board type")
include(pico_sdk_import.cmake)
project(${APP_NAME} C CXX ASM)
pico_sdk_init()
# =================================================================================================

# DO NOT EDIT: Configure FreeRTOS =================================================================
include(FreeRTOS_Kernel_import.cmake)
target_include_directories(FreeRTOS-Kernel INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/config
)
# ===============================================================================================

# DO NOT EDIT: Prepare the build to compile different libraries of the project ==================
add_subdirectory(libs/TKJHAT)
add_subdirectory(libs/usb-serial-debug)
# ===============================================================================================

# DO NOT EDIT: Prepare build to compile the different examples ==================================
# Examples are separate projects. Uncomment one only when you want to build it too.
# add_subdirectory(examples/hello_blink)
# add_subdirectory(examples/hello_blink_freertos)
# add_subdirectory(examples/hello_pico)
# add_subdirectory(examples/hello_freertos)
# add_subdirectory(examples/hello_dual_cdc)
# add_subdirectory(examples/hello_microphone)
# add_subdirectory(examples/compilation_errors)
# add_subdirectory(examples/hello_hat)
# add_subdirectory(examples/hat_example)
# add_subdirectory(examples/hat_imu_ex)
# add_subdirectory(examples/hat_imu_cdc_ex)
# add_subdirectory(examples/hello_serial_client)
# add_subdirectory(examples/hello_serial_bidirectional_client)
# ===============================================================================================

# Add all application source files here.
add_executable(${MAIN_TARGET}
    src/main.c
    src/morse.c
    src/communication.c
    src/display.c
)

target_link_libraries(${MAIN_TARGET}
    pico_stdlib
    FreeRTOS-Kernel
    FreeRTOS-Kernel-Heap4
    TKJHAT_SDK
    usb_serial_debug
)

# Pico W support.
if (PICO_CYW43_SUPPORTED)
    target_link_libraries(${MAIN_TARGET} pico_cyw43_arch_none)
endif()

# USB stdio is disabled because usb_serial_debug provides two CDC ports.
pico_enable_stdio_usb(${MAIN_TARGET} 0)
pico_enable_stdio_uart(${MAIN_TARGET} 0)

pico_add_extra_outputs(${MAIN_TARGET})
pico_set_program_url(${MAIN_TARGET} https://github.com/UniOulu-Ubicomp-Programming-Courses/JTKJ-PicoRTOS-Project)
# ===============================================================================================
