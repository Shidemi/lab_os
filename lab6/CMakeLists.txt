find_package(Java)

if(Java_JAVAC_EXECUTABLE)
    file(GLOB_RECURSE LAB6_JAVA_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/*.java"
    )

    set(LAB6_CLASSES_DIR "${CMAKE_CURRENT_BINARY_DIR}/classes")

    add_custom_target(Lab6Java ALL
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${LAB6_CLASSES_DIR}"
        COMMAND "${Java_JAVAC_EXECUTABLE}" -encoding UTF-8 -d "${LAB6_CLASSES_DIR}" ${LAB6_JAVA_SOURCES}
        BYPRODUCTS "${LAB6_CLASSES_DIR}/lab3/Main.class" "${LAB6_CLASSES_DIR}/lab3/MarkerThread.class"
        SOURCES ${LAB6_JAVA_SOURCES}
        COMMENT "Compiling lab6 Java sources"
        VERBATIM
    )
else()
    message(WARNING "Java compiler was not found; lab6 target is disabled.")
endif()
