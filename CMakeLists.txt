cmake_minimum_required(VERSION 2.8)
project(stoat)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)
find_package(LLVM REQUIRED)
find_program(CLANG   NAMES clang   DOC "Clang C Compiler")
find_program(CLANGPP NAMES clang++ DOC "Clang C++ Compiler")
find_program(RUBY    NAMES ruby    DOC "Ruby Scripting Language")

if(NOT RUBY OR NOT CLANG OR NOT CLANGPP)
    message(error_fatal "Missing Dependencies...")
endif()

option(DISABLE_LDCONFIG "Disable running ldconfig on install")

link_directories(${LLVM_LIB_DIR})
include_directories(${LLVM_INCLUDE_DIRS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LLVM_DEFINITIONS} -fno-rtti -std=c++17")
message(status "Build Flags '${CMAKE_CXX_FLAGS}'")

add_library(stoat SHARED src/llvm-passes.cpp)

######################################################################
#                      Installation                                  #
######################################################################

install(TARGETS stoat LIBRARY DESTINATION lib)
install(PROGRAMS ${CMAKE_SOURCE_DIR}/stoat
                 ${CMAKE_SOURCE_DIR}/stoat-compile
                 ${CMAKE_SOURCE_DIR}/stoat-compile++
    DESTINATION bin)
install(PROGRAMS ${CMAKE_SOURCE_DIR}/lib/callgraph.rb
                 ${CMAKE_SOURCE_DIR}/lib/deductions.rb
                 ${CMAKE_SOURCE_DIR}/lib/graph.rb
                 ${CMAKE_SOURCE_DIR}/lib/load-callgraph.rb
    DESTINATION share/stoat)
install(FILES ${CMAKE_SOURCE_DIR}/data/whitelist.txt
              ${CMAKE_SOURCE_DIR}/data/blacklist.txt DESTINATION share/stoat)
if (NOT ${DISABLE_LDCONFIG})
    install(SCRIPT cmake/InstallScript.cmake)
endif()


######################################################################
#               Tests to verify it works                             #
######################################################################
set(SRC ${CMAKE_CURRENT_SOURCE_DIR})
enable_testing()

#Verify one file basic behavior
add_test(test-one ${RUBY} ${CMAKE_SOURCE_DIR}/test/run-test.rb
    ${CMAKE_SOURCE_DIR}/stoat
    ${CMAKE_BINARY_DIR}/libstoat.so
    ${CLANG}
    ${CMAKE_SOURCE_DIR}/data/empty_whitelist.txt
    ${CMAKE_SOURCE_DIR}/data/blacklist.txt
    nil
    ${SRC}/test/test-input.c)
set_property(TEST test-one
    PROPERTY PASS_REGULAR_EXPRESSION
     "undefined_function : Assumed Unsafe")

 add_test(suppression
    ${RUBY} ${CMAKE_SOURCE_DIR}/test/run-test.rb
    ${CMAKE_SOURCE_DIR}/stoat
    ${CMAKE_BINARY_DIR}/libstoat.so
    ${CLANG}
    ${CMAKE_SOURCE_DIR}/data/whitelist.txt
    ${CMAKE_SOURCE_DIR}/data/blacklist.txt
    ${SRC}/test/suppression_example.txt
    ${SRC}/test/test-input.c)

add_test(suppression-two
    ${RUBY} ${CMAKE_SOURCE_DIR}/test/run-test.rb
    ${CMAKE_SOURCE_DIR}/stoat
    ${CMAKE_BINARY_DIR}/libstoat.so
    ${CLANG}
    ${CMAKE_SOURCE_DIR}/data/whitelist.txt
    ${CMAKE_SOURCE_DIR}/data/blacklist.txt
    ${SRC}/test/suppression_two.txt
    ${SRC}/test/test-suppression.c)

set_property(TEST suppression-two
    PROPERTY PASS_REGULAR_EXPRESSION
    "1 error")

#Check that information is translated over multiple translation units
add_test(multi-translation-unit
    ${RUBY} ${CMAKE_SOURCE_DIR}/test/run-test.rb
    ${CMAKE_SOURCE_DIR}/stoat
    ${CMAKE_BINARY_DIR}/libstoat.so
    ${CLANGPP}
    ${CMAKE_SOURCE_DIR}/data/empty_whitelist.txt
    ${CMAKE_SOURCE_DIR}/data/blacklist.txt
    nil
    ${SRC}/test/test-input.cpp ${SRC}/test/test-input2.cpp)
set_property(TEST multi-translation-unit PROPERTY PASS_REGULAR_EXPRESSION
    "undefined_and_used.* : Assumed Unsafe")

#Verify that functions can be externally annotated
add_test(manual-whitelist
    ${RUBY} ${CMAKE_SOURCE_DIR}/test/run-test.rb
    ${CMAKE_SOURCE_DIR}/stoat
    ${CMAKE_BINARY_DIR}/libstoat.so
    ${CLANGPP}
    ${CMAKE_SOURCE_DIR}/data/whitelist.txt
    ${CMAKE_SOURCE_DIR}/data/blacklist.txt
    nil
    ${SRC}/test/math-safety.cpp)

#Verify that virtual methods are handled correctly
add_test(virtual-calls
    ${RUBY} ${CMAKE_SOURCE_DIR}/test/run-test.rb
    ${CMAKE_SOURCE_DIR}/stoat
    ${CMAKE_BINARY_DIR}/libstoat.so
    ${CLANGPP}
    ${CMAKE_SOURCE_DIR}/data/whitelist.txt
    ${CMAKE_SOURCE_DIR}/data/blacklist.txt
    nil
    ${SRC}/test/virtual-methods.cpp)

#Verify that 'operator new' was found
add_test(op-new
    ${RUBY} ${CMAKE_SOURCE_DIR}/test/run-test.rb
    ${CMAKE_SOURCE_DIR}/stoat
    ${CMAKE_BINARY_DIR}/libstoat.so
    ${CLANGPP}
    ${CMAKE_SOURCE_DIR}/data/whitelist.txt
    ${CMAKE_SOURCE_DIR}/data/blacklist.txt
    nil
    ${SRC}/test/cpp-new-detection.cpp)
set_property(TEST op-new PROPERTY PASS_REGULAR_EXPRESSION
    "new")

add_test(malloc-blacklist
    ${RUBY} ${CMAKE_SOURCE_DIR}/test/run-test.rb
    ${CMAKE_SOURCE_DIR}/stoat
    ${CMAKE_BINARY_DIR}/libstoat.so
    ${CLANG}
    ${CMAKE_SOURCE_DIR}/data/whitelist.txt
    ${CMAKE_SOURCE_DIR}/data/blacklist.txt
    nil
    ${SRC}/test/malloc-blacklist.c)

set_property(TEST malloc-blacklist PROPERTY PASS_REGULAR_EXPRESSION
    "malloc : NonRealtime \\(Blacklist\\)")

add_test(lambda-call
    ${RUBY} ${CMAKE_SOURCE_DIR}/test/run-test.rb
    ${CMAKE_SOURCE_DIR}/stoat
    ${CMAKE_BINARY_DIR}/libstoat.so
    ${CLANGPP}
    ${CMAKE_SOURCE_DIR}/data/whitelist.txt
    ${CMAKE_SOURCE_DIR}/data/blacklist.txt
    nil
    ${SRC}/test/lambda-call.cpp)
set_property(TEST lambda-call PROPERTY PASS_REGULAR_EXPRESSION
    "malloc_of_doooooooooom.* : Assumed Unsafe")

add_test(namespace-disambiguation
    ${RUBY} ${CMAKE_SOURCE_DIR}/test/run-test.rb
    ${CMAKE_SOURCE_DIR}/stoat
    ${CMAKE_BINARY_DIR}/libstoat.so
    ${CLANGPP}
    ${CMAKE_SOURCE_DIR}/data/whitelist.txt
    ${CMAKE_SOURCE_DIR}/data/blacklist.txt
    nil
    ${SRC}/test/namespaces.cpp)
set_property(TEST namespace-disambiguation PROPERTY PASS_REGULAR_EXPRESSION
    "Total of 2 error")

#Test virtual inheritance combined with diamond graph structures
add_test(diamond
    ${RUBY} ${CMAKE_SOURCE_DIR}/test/run-test.rb
    ${CMAKE_SOURCE_DIR}/stoat
    ${CMAKE_BINARY_DIR}/libstoat.so
    ${CLANGPP}
    ${CMAKE_SOURCE_DIR}/data/whitelist.txt
    ${CMAKE_SOURCE_DIR}/data/blacklist.txt
    nil
    ${SRC}/test/diamond-inherit.cpp)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(CMAKE_SHARED_LINKER_FLAGS "-undefined dynamic_lookup")
endif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
