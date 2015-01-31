execute_process(COMMAND ldconfig
    RESULT_VARIABLE Result
    OUTPUT_VARIABLE Output
    ERROR_VARIABLE Error)
if(Result EQUAL 0)
    message(STATUS "Ran ldconfig...")
else()
    message(WARNING "Warning: ldconfig ${Result}\n${Output}")
    message(WARNING "         You may need to manually specify --llvm-passes")
endif()
