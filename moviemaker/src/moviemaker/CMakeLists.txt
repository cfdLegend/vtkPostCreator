SET(SRC
    Config.cxx
    MovieMaker.cxx
    Slicer.cxx
    DedFile.cxx
    MovieMaker-app.cxx
)

set_source_files_properties(Slicer.cxx PROPERTIES COMPILE_FLAGS "-DREAL=double -DVOID=int -DANSI_DECLARATORS")

ADD_EXECUTABLE(moviemaker ${SRC})
target_link_libraries(moviemaker PRIVATE /cfd/aerocfd/codes/triangle/libtriangle.a)
target_include_directories(moviemaker PRIVATE /cfd/aerocfd/codes/triangle)
target_link_libraries(moviemaker PRIVATE moviemaker-common)
install(TARGETS moviemaker RUNTIME DESTINATION bin)