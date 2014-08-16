execute_process(COMMAND ldconfig
    RESULT_VARIABLE Result
    OUTPUT_VARIABLE Output
    ERROR_VARIABLE Error)
if(Result EQUAL 0)
    message(STATUS "Ran ldconfig...")
else()
    message(FATAL_ERROR "Error: ldconfig ${Result}\n${Output}")
endif()
