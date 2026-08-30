cmake_minimum_required(VERSION 3.22)

project(MiniAudioPlayerST LANGUAGES C ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(FIRMWARE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/firmware/MiniAudioPlayerST")
set(MCU_FLAGS
    -mcpu=cortex-m0
    -mthumb
    -mfloat-abi=soft
)

# Common target configuration.  Keeping it on an INTERFACE target ensures that
# every module and the final link use the same MCU ABI and preprocessor model.
add_library(project_options INTERFACE)
target_compile_features(project_options INTERFACE c_std_11)
target_compile_definitions(project_options INTERFACE
    USE_HAL_DRIVER
    STM32F072xB
    BSP_UART_USE_NEWLIB
)
target_compile_options(project_options INTERFACE
    ${MCU_FLAGS}
    -ffunction-sections
    -fdata-sections
    -fno-common
    $<$<CONFIG:Debug>:-Og>
    $<$<CONFIG:Debug>:-g3>
    $<$<CONFIG:Release>:-Os>
)
target_link_options(project_options INTERFACE ${MCU_FLAGS})

# Header-only CMSIS device support.
add_library(cmsis INTERFACE)
target_include_directories(cmsis INTERFACE
    "${FIRMWARE_ROOT}/Drivers/CMSIS/Include"
    "${FIRMWARE_ROOT}/Drivers/CMSIS/Device/ST/STM32F0xx/Include"
)
target_link_libraries(cmsis INTERFACE project_options)

file(GLOB STM32_HAL_SOURCES CONFIGURE_DEPENDS
    "${FIRMWARE_ROOT}/Drivers/STM32F0xx_HAL_Driver/Src/*.c"
)
list(FILTER STM32_HAL_SOURCES EXCLUDE REGEX "_template\\.c$")
add_library(stm32_hal OBJECT ${STM32_HAL_SOURCES})
target_include_directories(stm32_hal PUBLIC
    "${FIRMWARE_ROOT}/Core/Inc"
    "${FIRMWARE_ROOT}/Drivers/STM32F0xx_HAL_Driver/Inc"
    "${FIRMWARE_ROOT}/Drivers/STM32F0xx_HAL_Driver/Inc/Legacy"
)
target_link_libraries(stm32_hal PUBLIC cmsis project_options)

file(GLOB_RECURSE FATFS_SOURCES CONFIGURE_DEPENDS
    "${FIRMWARE_ROOT}/FATFS/*.c"
    "${FIRMWARE_ROOT}/Middlewares/Third_Party/FatFs/src/*.c"
)
list(FILTER FATFS_SOURCES EXCLUDE REGEX "/option/(cc932|cc936|cc949|cc950|unicode)\\.c$")
add_library(fatfs OBJECT ${FATFS_SOURCES})
target_include_directories(fatfs PUBLIC
    "${FIRMWARE_ROOT}/FATFS/App"
    "${FIRMWARE_ROOT}/FATFS/Target"
    "${FIRMWARE_ROOT}/Middlewares/Third_Party/FatFs/src"
    "${FIRMWARE_ROOT}/Middlewares/Third_Party/FatFs/src/option"
)
target_include_directories(fatfs PRIVATE "${FIRMWARE_ROOT}/BSP/include")
target_link_libraries(fatfs PUBLIC stm32_hal project_options)

file(GLOB BSP_SOURCES CONFIGURE_DEPENDS "${FIRMWARE_ROOT}/BSP/src/*.c")
add_library(bsp OBJECT ${BSP_SOURCES})
target_include_directories(bsp PUBLIC "${FIRMWARE_ROOT}/BSP/include")
target_include_directories(bsp PRIVATE "${FIRMWARE_ROOT}/App/include")
target_link_libraries(bsp PUBLIC stm32_hal fatfs project_options)

file(GLOB APP_SOURCES CONFIGURE_DEPENDS "${FIRMWARE_ROOT}/App/src/*.c")
add_library(app OBJECT ${APP_SOURCES})
target_include_directories(app PUBLIC "${FIRMWARE_ROOT}/App/include")
target_link_libraries(app PUBLIC bsp fatfs project_options)

file(GLOB CORE_SOURCES CONFIGURE_DEPENDS "${FIRMWARE_ROOT}/Core/Src/*.c")
add_library(core OBJECT ${CORE_SOURCES})
target_include_directories(core PUBLIC "${FIRMWARE_ROOT}/Core/Inc")
target_link_libraries(core PUBLIC app bsp fatfs stm32_hal project_options)

file(GLOB APP_TEST_SOURCES CONFIGURE_DEPENDS
    "${FIRMWARE_ROOT}/App/test/src/*.c"
)
add_library(app_tests OBJECT ${APP_TEST_SOURCES})
target_include_directories(app_tests PUBLIC "${FIRMWARE_ROOT}/App/test/include")
target_link_libraries(app_tests PUBLIC app bsp fatfs project_options)

# main.c currently selects an on-target test entry point.
target_link_libraries(core PUBLIC app_tests)

set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/STM32F072C8Tx_FLASH.ld")

add_executable(${PROJECT_NAME}
    "${FIRMWARE_ROOT}/Drivers/CMSIS/Device/ST/STM32F0xx/Source/Templates/gcc/startup_stm32f072xb.s"
)
target_link_libraries(${PROJECT_NAME} PRIVATE
    core app_tests app bsp fatfs stm32_hal project_options
)

set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".elf")
target_link_options(${PROJECT_NAME} PRIVATE
    "-T${LINKER_SCRIPT}"
    -Wl,--gc-sections
    -Wl,--cref
    -Wl,-Map=${PROJECT_NAME}.map
    -Wl,--print-memory-usage
    --specs=nano.specs
    --specs=nosys.specs
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex
            $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}>
    COMMENT "Generating HEX/BIN images and reporting firmware size"
)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/openocd.cmake")
add_openocd_flash_target(${PROJECT_NAME})
