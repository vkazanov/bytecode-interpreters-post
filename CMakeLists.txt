cmake_minimum_required(VERSION 3.10)
project(bytecode-interpreters)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Set default compile flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11 -O3 -g")

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

# Add custom targets for testing
add_custom_target(test-interpreters
    COMMAND ${CMAKE_COMMAND} -E echo "Running interpreters tests..."
    COMMAND ${CMAKE_COMMAND} -E env "${CMAKE_BINARY_DIR}/basic-switch"
    COMMAND ${CMAKE_COMMAND} -E env "${CMAKE_BINARY_DIR}/immediate-arg"
    COMMAND ${CMAKE_COMMAND} -E env "${CMAKE_BINARY_DIR}/stack-machine"
    COMMAND ${CMAKE_COMMAND} -E env "${CMAKE_BINARY_DIR}/register-machine"
    DEPENDS ${INTERPRETERS}
)

add_custom_target(test-regexp-interpreter
    COMMAND ${CMAKE_COMMAND} -E echo "Running regexp-interpreter test..."
    COMMAND regexp-interpreter
    DEPENDS regexp-interpreter
)

add_custom_target(pigletvm-test-run
    COMMAND ${CMAKE_COMMAND} -E echo "Running pigletvm tests..."
    COMMAND pigletvm-test
    DEPENDS pigletvm-test
)

add_custom_target(piglet-matcher-test-run
    COMMAND ${CMAKE_COMMAND} -E echo "Running piglet-matcher tests..."
    COMMAND piglet-matcher-test
    DEPENDS piglet-matcher-test
)

add_custom_target(test
    DEPENDS test-interpreters test-regexp-interpreter pigletvm-test-run piglet-matcher-test-run
)

# Add custom target for cleaning
add_custom_target(clean-all
    COMMAND ${CMAKE_COMMAND} -E remove -f ${INTERPRETERS} regexp-interpreter pigletvm pigletvm-test piglet-matcher piglet-matcher-test
)

# Set PHONY targets
set_target_properties(test-interpreters test-regexp-interpreter pigletvm-test-run piglet-matcher-test-run test clean-all PROPERTIES FOLDER "Custom Targets")
