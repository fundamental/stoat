cmake_minimum_required(VERSION 2.8)
project(sfpv2)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 1)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)
find_package(LLVM REQUIRED)
find_program(CLANG   NAMES clang   DOC "Clang C Compiler")
find_program(CLANGPP NAMES clang++ DOC "Clang C++ Compiler")
find_program(RUBY    NAMES ruby    DOC "Ruby Scripting Language")
#LLVM

add_definitions(-fno-rtti)
add_definitions(-DCLANG_LOCATION="${LLVM_BIN_DIR}/clang")
link_directories(${LLVM_LIB_DIR})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LLVM_FLAGS} -std=c++11")
message(status "Build Flags '${CMAKE_CXX_FLAGS}'")

add_definitions(-DVERSION="${VERSION_MAJOR}.${VERSION_MINOR}")
add_library(foo SHARED src/pass1.cpp)
target_link_libraries(foo ${LLVM_LDFLAGS})

######################################################################
#               Tests to verify it works                             #
######################################################################
set(SRC ${CMAKE_CURRENT_SOURCE_DIR})
enable_testing()

#Verify one file basic behavior
add_test(test-one ${RUBY} ${CMAKE_SOURCE_DIR}/test/run-test.rb
    ${CMAKE_SOURCE_DIR}/run.rb
    ${CMAKE_BINARY_DIR}/libfoo.so
    ${CLANG}
    ${CMAKE_SOURCE_DIR}/data/empty_whitelist.txt
    ${CMAKE_SOURCE_DIR}/data/blacklist.txt
    ${SRC}/test/test-input.c)
set_property(TEST test-one
    PROPERTY PASS_REGULAR_EXPRESSION
     "undefined_function : No Code Or Annotations")

#Check that information is translated over multiple translation units
add_test(multi-translation-unit
    ${RUBY} ${CMAKE_SOURCE_DIR}/test/run-test.rb
    ${CMAKE_SOURCE_DIR}/run.rb
    ${CMAKE_BINARY_DIR}/libfoo.so
    ${CLANGPP}
    ${CMAKE_SOURCE_DIR}/data/empty_whitelist.txt
    ${CMAKE_SOURCE_DIR}/data/blacklist.txt
    ${SRC}/test/test-input.cpp ${SRC}/test/test-input2.cpp)
set_property(TEST multi-translation-unit PROPERTY PASS_REGULAR_EXPRESSION
    "undefined_and_used.* : No Code")

#Verify that functions can be externally annotated
add_test(manual-whitelist
    ${RUBY} ${CMAKE_SOURCE_DIR}/test/run-test.rb
    ${CMAKE_SOURCE_DIR}/run.rb
    ${CMAKE_BINARY_DIR}/libfoo.so
    ${CLANGPP}
    ${CMAKE_SOURCE_DIR}/data/whitelist.txt
    ${CMAKE_SOURCE_DIR}/data/blacklist.txt
    ${SRC}/test/math-safety.cpp)

#Verify that virtual methods are handled correctly
add_test(virtual-calls
    ${RUBY} ${CMAKE_SOURCE_DIR}/test/run-test.rb
    ${CMAKE_SOURCE_DIR}/run.rb
    ${CMAKE_BINARY_DIR}/libfoo.so
    ${CLANGPP}
    ${CMAKE_SOURCE_DIR}/data/whitelist.txt
    ${CMAKE_SOURCE_DIR}/data/blacklist.txt
    ${SRC}/test/virtual-methods.cpp)

#Verify that 'operator new' was found
add_test(op-new
    ${RUBY} ${CMAKE_SOURCE_DIR}/test/run-test.rb
    ${CMAKE_SOURCE_DIR}/run.rb
    ${CMAKE_BINARY_DIR}/libfoo.so
    ${CLANGPP}
    ${CMAKE_SOURCE_DIR}/data/whitelist.txt
    ${CMAKE_SOURCE_DIR}/data/blacklist.txt
    ${SRC}/test/cpp-new-detection.cpp)
set_property(TEST op-new PROPERTY PASS_REGULAR_EXPRESSION
    "new")

add_test(malloc-blacklist
    ${RUBY} ${CMAKE_SOURCE_DIR}/test/run-test.rb
    ${CMAKE_SOURCE_DIR}/run.rb
    ${CMAKE_BINARY_DIR}/libfoo.so
    ${CLANG}
    ${CMAKE_SOURCE_DIR}/data/whitelist.txt
    ${CMAKE_SOURCE_DIR}/data/blacklist.txt
    ${SRC}/test/malloc-blacklist.c)

set_property(TEST malloc-blacklist PROPERTY PASS_REGULAR_EXPRESSION
    "malloc : The Function Was Declared NonRealtime By A Blacklist")

add_test(lambda-call
    ${RUBY} ${CMAKE_SOURCE_DIR}/test/run-test.rb
    ${CMAKE_SOURCE_DIR}/run.rb
    ${CMAKE_BINARY_DIR}/libfoo.so
    ${CLANGPP}
    ${CMAKE_SOURCE_DIR}/data/whitelist.txt
    ${CMAKE_SOURCE_DIR}/data/blacklist.txt
    ${SRC}/test/lambda-call.cpp)
set_property(TEST lambda-call PROPERTY PASS_REGULAR_EXPRESSION
    "malloc_of_doooooooooom.* : No Code Or Annotations")
