# sleep and spawn helpers with the promise model resumed on the event loop and no external deps

list(APPEND VARN_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/include")

list(APPEND VARN_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/src/Promise.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/AsyncModule.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/AsyncTask.cpp"
)
