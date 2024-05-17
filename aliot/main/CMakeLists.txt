idf_component_register(
    SRC_DIRS
    "."
    "src"

    INCLUDE_DIRS
    "."
    "src"
)

target_compile_options(${COMPONENT_LIB} PUBLIC "-Wno-pointer-sign") 
target_compile_options(${COMPONENT_LIB} PUBLIC "-Wno-discarded-qualifiers") 
target_compile_options(${COMPONENT_LIB} PUBLIC "-Wno-unused-function") 
target_compile_options(${COMPONENT_LIB} PUBLIC "-Wno-cast-function-type") 
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-stringop-truncation") 

