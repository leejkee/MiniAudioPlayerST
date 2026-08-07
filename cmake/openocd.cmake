include_guard(GLOBAL)

option(ENABLE_OPENOCD "Create the OpenOCD flash target" ON)

set(STLINK_SERIAL "" CACHE STRING
    "Optional ST-Link serial number; leave empty to use the only connected probe")
set(OPENOCD_INTERFACE_CFG "interface/stlink.cfg" CACHE STRING
    "OpenOCD debug adapter configuration")
set(OPENOCD_TARGET_CFG "target/stm32f0x.cfg" CACHE STRING
    "OpenOCD target configuration")

function(add_openocd_flash_target firmware_target)
    if(NOT ENABLE_OPENOCD)
        message(STATUS "OpenOCD support is disabled; the flash target will not be created")
        return()
    endif()

    if(NOT TARGET "${firmware_target}")
        message(FATAL_ERROR
            "Cannot create flash target: '${firmware_target}' is not a CMake target")
    endif()

    if(NOT OPENOCD_EXECUTABLE)
        find_program(OPENOCD_EXECUTABLE NAMES openocd)
    endif()

    if(NOT OPENOCD_EXECUTABLE)
        message(STATUS
            "OpenOCD was not found; the flash target will not be created. "
            "Set OPENOCD_EXECUTABLE to enable it.")
        return()
    endif()

    set(openocd_arguments
        -f "${OPENOCD_INTERFACE_CFG}"
    )

    if(STLINK_SERIAL)
        list(APPEND openocd_arguments
            -c "adapter serial ${STLINK_SERIAL}"
        )
    endif()

    list(APPEND openocd_arguments
        -f "${OPENOCD_TARGET_CFG}"
        -c "program $<TARGET_FILE:${firmware_target}> verify reset exit"
    )

    add_custom_target(flash
        COMMAND "${OPENOCD_EXECUTABLE}" ${openocd_arguments}
        DEPENDS "${firmware_target}"
        USES_TERMINAL
        VERBATIM
        COMMENT "Flashing ${firmware_target} with OpenOCD and ST-Link"
    )
endfunction()
