SET(SRC
    BinaryWriter.cxx
    SurfaceToWF1.cxx
)

ADD_EXECUTABLE(surfacetowf1 ${SRC})
target_link_libraries(surfacetowf1 PRIVATE moviemaker-common)
install(TARGETS surfacetowf1 RUNTIME DESTINATION bin)
