# Copyright (c) Microsoft Corporation.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

# Create the bin output
function(create_bin_output TARGET)
    add_custom_target(${TARGET}.bin ALL 
        DEPENDS ${TARGET}
        COMMAND ${CMAKE_OBJCOPY} -Obinary ${TARGET}.elf ${TARGET}.bin)
endfunction()

# Add custom command to print firmware size in Berkley format
function(firmware_size TARGET)
    add_custom_target(${TARGET}.size ALL 
        DEPENDS ${TARGET} 
        COMMAND ${CMAKE_SIZE_UTIL} -B ${TARGET}.elf)
endfunction()

function(add_azrtos_component_dir dirname)
    # Store the current list in a temp
    set(tmp ${azrtos_targets})
    # Add the subdir
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/${dirname})
    # If there is a linker script defined, use it
    if(EXISTS ${LINKER_SCRIPT})
        target_link_options(${dirname} INTERFACE -T ${LINKER_SCRIPT})
    endif()
    # Add this target into the temp
    list(APPEND tmp "azrtos::${dirname}")
    # Copy the temp back up to the parent list
    set(azrtos_targets ${tmp} PARENT_SCOPE)
endfunction()
