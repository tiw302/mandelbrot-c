# embed_shader.cmake
# Reads a GLSL shader file and writes its content as a static C string literal in a header file.
# Usage: cmake -DGLSL_FILE=input.glsl -DHEADER_FILE=output.h -DVAR_NAME=var_name -P scripts/embed_shader.cmake

if(NOT GLSL_FILE OR NOT HEADER_FILE OR NOT VAR_NAME)
    message(FATAL_ERROR "Missing GLSL_FILE, HEADER_FILE, or VAR_NAME arguments")
endif()

file(READ ${GLSL_FILE} SHADER_CONTENT)
string(REPLACE "\r" "" SHADER_CONTENT "${SHADER_CONTENT}")
string(REPLACE "\\" "\\\\" SHADER_CONTENT "${SHADER_CONTENT}")
string(REPLACE "\"" "\\\"" SHADER_CONTENT "${SHADER_CONTENT}")
string(REPLACE "\n" "\\n\"\n\"" SHADER_CONTENT "${SHADER_CONTENT}")
set(HEADER_CONTENT "static const char* ${VAR_NAME} = \n\"${SHADER_CONTENT}\";\n")
file(WRITE ${HEADER_FILE} "${HEADER_CONTENT}")
