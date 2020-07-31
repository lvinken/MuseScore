
set (MNX_SRC
    ${CMAKE_CURRENT_LIST_DIR}/importmnx.cpp
    ${CMAKE_CURRENT_LIST_DIR}/importmnx.h
    ${CMAKE_CURRENT_LIST_DIR}/xmlstreamreader.cpp
    ${CMAKE_CURRENT_LIST_DIR}/xmlstreamreader.h
    )

include_directories(
      ${PROJECT_SOURCE_DIR}/importexport/musicxml
      )
