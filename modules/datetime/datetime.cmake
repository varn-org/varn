# datetime provides calendar arithmetic plus iso-8601 parsing and formatting backed by the date.h library

list(APPEND VARN_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/include")
list(APPEND VARN_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/DatetimeModule.cpp")

set(VARN_NEEDS_DATE ON)
