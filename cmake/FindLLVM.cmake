#Find LLVM on the system
#
# LLVM_LIBRARY_DIRS     - llvm library base dir
# LLVM_DEFINITIONS      - cppflags
# LLVM_INCLUDE_DIRS     - include path
# LLVM_LIBRARIES        - linker flags
# LLVM_FOUND            - True when llvm is found
# LLVM_INSTALL_PREFIX   - Installation location of llvm

find_program(LLVM_CONFIG
    NAMES llvm-config llvm-config-37 llvm-config-3.7 llvm-config-36 llvm-config-3.6
          llvm-config-35 llvm-config-3.5 llvm-config-34 llvm-config-3.4 llvm-config-33 llvm-config-3.3
    PATHS /usr/bin /usr/local/bin 
    DOC "llvm-config Configuration Helper")
if(LLVM_CONFIG STREQUAL "LLVM_CONFIG-NOTFOUND")
    set(LLVM_FOUND 1)
endif()

#Gather flags
execute_process(
    COMMAND ${LLVM_CONFIG} --libdir
    OUTPUT_VARIABLE LLVM_LIBRARY_DIRS
    OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(
    COMMAND ${LLVM_CONFIG} --cppflags
    OUTPUT_VARIABLE LLVM_DEFINITIONS
    OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(
    COMMAND ${LLVM_CONFIG} --includedir
    OUTPUT_VARIABLE LLVM_INCLUDE_DIRS
    OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(
    COMMAND ${LLVM_CONFIG} --libs
    OUTPUT_VARIABLE LLVM_LIBRARIES
    OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(
    COMMAND ${LLVM_CONFIG} --version
    OUTPUT_VARIABLE LLVM_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(
    COMMAND ${LLVM_CONFIG} --prefix
    OUTPUT_VARIABLE LLVM_INSTALL_PREFIX
    OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(
    COMMAND ${LLVM_CONFIG} --ldflags
    OUTPUT_VARIABLE LLVM_LDFLAGS
    OUTPUT_STRIP_TRAILING_WHITESPACE)
set(LLVM_LIBRARIES ${LLVM_LIBRARIES} ${LLVM_LDFLAGS})


include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LLVM
    REQUIRED_VARS LLVM_LIBRARIES LLVM_INCLUDE_DIRS
    VERSION_VAR LLVM_VERSION)
