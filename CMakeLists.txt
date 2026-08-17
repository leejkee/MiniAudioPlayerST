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

add_library(stm32_hal OBJECT
    "${FIRMWARE_ROOT}/Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal.c"
    "${FIRMWARE_ROOT}/Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_cortex.c"
    "${FIRMWARE_ROOT}/Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_dma.c"
    "${FIRMWARE_ROOT}/Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_exti.c"
    "${FIRMWARE_ROOT}/Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_flash.c"
    "${FIRMWARE_ROOT}/Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_flash_ex.c"
    "${FIRMWARE_ROOT}/Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_gpio.c"
    "${FIRMWARE_ROOT}/Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_i2c.c"
    "${FIRMWARE_ROOT}/Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_i2c_ex.c"
    "${FIRMWARE_ROOT}/Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_pwr.c"
    "${FIRMWARE_ROOT}/Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_pwr_ex.c"
    "${FIRMWARE_ROOT}/Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_rcc.c"
    "${FIRMWARE_ROOT}/Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_rcc_ex.c"
    "${FIRMWARE_ROOT}/Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_spi.c"
    "${FIRMWARE_ROOT}/Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_spi_ex.c"
    "${FIRMWARE_ROOT}/Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_tim.c"
    "${FIRMWARE_ROOT}/Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_tim_ex.c"
    "${FIRMWARE_ROOT}/Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_uart.c"
    "${FIRMWARE_ROOT}/Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_uart_ex.c"
)
target_include_directories(stm32_hal PUBLIC
    "${FIRMWARE_ROOT}/Core/Inc"
    "${FIRMWARE_ROOT}/Drivers/STM32F0xx_HAL_Driver/Inc"
    "${FIRMWARE_ROOT}/Drivers/STM32F0xx_HAL_Driver/Inc/Legacy"
)
target_link_libraries(stm32_hal PUBLIC cmsis project_options)

add_library(fatfs OBJECT
    "${FIRMWARE_ROOT}/FATFS/App/fatfs.c"
    "${FIRMWARE_ROOT}/FATFS/Target/user_diskio.c"
    "${FIRMWARE_ROOT}/Middlewares/Third_Party/FatFs/src/diskio.c"
    "${FIRMWARE_ROOT}/Middlewares/Third_Party/FatFs/src/ff.c"
    "${FIRMWARE_ROOT}/Middlewares/Third_Party/FatFs/src/ff_gen_drv.c"
    "${FIRMWARE_ROOT}/Middlewares/Third_Party/FatFs/src/option/ccsbcs.c"
    "${FIRMWARE_ROOT}/Middlewares/Third_Party/FatFs/src/option/syscall.c"
)
target_include_directories(fatfs PUBLIC
    "${FIRMWARE_ROOT}/FATFS/App"
    "${FIRMWARE_ROOT}/FATFS/Target"
    "${FIRMWARE_ROOT}/Middlewares/Third_Party/FatFs/src"
    "${FIRMWARE_ROOT}/Middlewares/Third_Party/FatFs/src/option"
)
target_include_directories(fatfs PRIVATE "${FIRMWARE_ROOT}/BSP/include")
target_link_libraries(fatfs PUBLIC stm32_hal project_options)

add_library(bsp OBJECT
    "${FIRMWARE_ROOT}/BSP/src/bsp_SD.c"
    "${FIRMWARE_ROOT}/BSP/src/bsp_i2c_scanner.c"
    "${FIRMWARE_ROOT}/BSP/src/bsp_key.c"
    "${FIRMWARE_ROOT}/BSP/src/bsp_spi.c"
    "${FIRMWARE_ROOT}/BSP/src/bsp_spi_event.c"
    "${FIRMWARE_ROOT}/BSP/src/bsp_ssd1315.c"
    "${FIRMWARE_ROOT}/BSP/src/bsp_uart.c"
    "${FIRMWARE_ROOT}/BSP/src/bsp_vs1003.c"
)
target_include_directories(bsp PUBLIC "${FIRMWARE_ROOT}/BSP/include")
target_include_directories(bsp PRIVATE "${FIRMWARE_ROOT}/App/include")
target_link_libraries(bsp PUBLIC stm32_hal fatfs project_options)

add_library(app OBJECT
    "${FIRMWARE_ROOT}/App/src/sd_card.c"
    "${FIRMWARE_ROOT}/App/src/file_manager.c"
    "${FIRMWARE_ROOT}/App/src/font_cn.c"
    "${FIRMWARE_ROOT}/App/src/font_en.c"
    "${FIRMWARE_ROOT}/App/src/font_file_cn.c"
    "${FIRMWARE_ROOT}/App/src/font_icon.c"
    "${FIRMWARE_ROOT}/App/src/mp3_metadata.c"
    "${FIRMWARE_ROOT}/App/src/player.c"
    "${FIRMWARE_ROOT}/App/src/ui_file_page.c"
    "${FIRMWARE_ROOT}/App/src/ui_main_page.c"
    "${FIRMWARE_ROOT}/App/src/ui_menu_pages.c"
    "${FIRMWARE_ROOT}/App/src/ui_render.c"
)
target_include_directories(app PUBLIC "${FIRMWARE_ROOT}/App/include")
target_link_libraries(app PUBLIC bsp fatfs project_options)

add_library(core OBJECT
    "${FIRMWARE_ROOT}/Core/Src/gpio.c"
    "${FIRMWARE_ROOT}/Core/Src/i2c.c"
    "${FIRMWARE_ROOT}/Core/Src/main.c"
    "${FIRMWARE_ROOT}/Core/Src/spi.c"
    "${FIRMWARE_ROOT}/Core/Src/stm32f0xx_hal_msp.c"
    "${FIRMWARE_ROOT}/Core/Src/stm32f0xx_it.c"
    "${FIRMWARE_ROOT}/Core/Src/system_stm32f0xx.c"
    "${FIRMWARE_ROOT}/Core/Src/tim.c"
    "${FIRMWARE_ROOT}/Core/Src/usart.c"
    "${FIRMWARE_ROOT}/Core/Src/dma.c"
)
target_include_directories(core PUBLIC "${FIRMWARE_ROOT}/Core/Inc")
target_link_libraries(core PUBLIC app bsp fatfs stm32_hal project_options)

add_library(app_tests OBJECT
    "${FIRMWARE_ROOT}/App/test/src/iic_scan_test.c"
    "${FIRMWARE_ROOT}/App/test/src/key_controller_test.c"
    "${FIRMWARE_ROOT}/App/test/src/key_test.c"
    "${FIRMWARE_ROOT}/App/test/src/main_page_test.c"
    "${FIRMWARE_ROOT}/App/test/src/playlist_test.c"
    "${FIRMWARE_ROOT}/App/test/src/sd_oled_test.c"
    "${FIRMWARE_ROOT}/App/test/src/sd_test.c"
    "${FIRMWARE_ROOT}/App/test/src/ssd1315_font_test.c"
    "${FIRMWARE_ROOT}/App/test/src/ssd1315_test.c"
    "${FIRMWARE_ROOT}/App/test/src/vs1003_test.c"
    "${FIRMWARE_ROOT}/App/test/src/test_main.c"
)
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
