cmake_minimum_required(VERSION 3.10)
project(bytecode-interpreters C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Set compiler-specific flags
if(MSVC)
    # MSVC flags
    add_compile_options(/W3 /O2)
    # Disable some warnings that are common in this codebase
    add_compile_options(/wd4996)  # deprecated functions
    add_compile_options(/wd4244)  # conversion warnings
    add_compile_options(/wd4267)  # size_t conversions
else()
    # GCC/Clang flags
    set(CMAKE_C_EXTENSIONS ON)
    add_compile_options(-std=gnu11 -O3 -g -Wall -Wextra)
endif()

# Define source files for each target
set(INTERPRETERS basic-switch immediate-arg stack-machine register-machine)

foreach(interpreter ${INTERPRETERS})
    add_executable(${interpreter} "interpreter-${interpreter}.c")
endforeach()

add_executable(regexp-interpreter interpreter-regexp.c)
add_executable(pigletvm pigletvm.c pigletvm-rcache.c pigletvm-exec.c)
add_executable(pigletvm-test pigletvm.c pigletvm-rcache.c pigletvm-test.c)
add_executable(piglet-matcher piglet-matcher.c piglet-matcher-exec.c)
add_executable(piglet-matcher-test piglet-matcher.c piglet-matcher-test.c)

# Enable CTest
enable_testing()

# Add tests using CTest
# Basic interpreters - each one runs and should return 0
foreach(interpreter ${INTERPRETERS})
    add_test(NAME ${interpreter} COMMAND ${interpreter})
endforeach()

add_test(NAME regexp-interpreter COMMAND regexp-interpreter)
add_test(NAME pigletvm-test COMMAND pigletvm-test)
add_test(NAME piglet-matcher-test COMMAND piglet-matcher-test)

# Custom target 'run-tests' as an alias for running ctest (convenience)
add_custom_target(run-tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -C $<CONFIG>
    DEPENDS ${INTERPRETERS} regexp-interpreter pigletvm-test piglet-matcher-test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
