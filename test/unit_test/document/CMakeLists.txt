include_directories(${CMAKE_CURRENT_SOURCE_DIR}/)

file(GLOB UNIT_TEST_DOCUMENT_SRCS "./*.cc")

add_library(UNIT_TEST_DOCUMENT_OBJS OBJECT ${UNIT_TEST_DOCUMENT_SRCS})

add_dependencies(UNIT_TEST_DOCUMENT_OBJS ${DEPEND_LIBS})