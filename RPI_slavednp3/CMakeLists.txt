cmake_minimum_required(VERSION 3.16)
project(slavednp3_modularizado LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Não tentar baixar dependências da internet
set(FETCHCONTENT_FULLY_DISCONNECTED ON CACHE BOOL "" FORCE)

# Usar as dependências locais que já existem dentro do opendnp3/build/_deps
set(FETCHCONTENT_SOURCE_DIR_ASIO
    "${CMAKE_CURRENT_LIST_DIR}/../opendnp3/build/_deps/asio-src"
    CACHE PATH "" FORCE
)

set(FETCHCONTENT_SOURCE_DIR_EXE4CPP
    "${CMAKE_CURRENT_LIST_DIR}/../opendnp3/build/_deps/exe4cpp-src"
    CACHE PATH "" FORCE
)

set(FETCHCONTENT_SOURCE_DIR_SER4CPP
    "${CMAKE_CURRENT_LIST_DIR}/../opendnp3/build/_deps/ser4cpp-src"
    CACHE PATH "" FORCE
)

# Configurações do OpenDNP3
set(DNP3_TLS OFF CACHE BOOL "" FORCE)
set(DNP3_TESTS OFF CACHE BOOL "" FORCE)
set(DNP3_EXAMPLES OFF CACHE BOOL "" FORCE)
set(DNP3_JAVA OFF CACHE BOOL "" FORCE)
set(DNP3_STATIC_LIBS ON CACHE BOOL "" FORCE)

# Compila o OpenDNP3 junto com o seu projeto
add_subdirectory(
    "${CMAKE_CURRENT_LIST_DIR}/../opendnp3"
    "${CMAKE_BINARY_DIR}/opendnp3-build"
)

find_library(MODBUS_LIB modbus)
if (NOT MODBUS_LIB)
    message(FATAL_ERROR "libmodbus nao encontrada. Instale com: sudo apt install libmodbus-dev")
endif()

add_executable(slavednp3
    src/Main.cpp
    src/Log.cpp
    src/Config.cpp
    src/Modbus.cpp
    src/Dnp3.cpp
    src/Interface.cpp
)

target_include_directories(slavednp3 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_definitions(slavednp3 PRIVATE LIBMODBUS_MASTER)

target_link_libraries(slavednp3 PRIVATE
    opendnp3
    ${MODBUS_LIB}
    pthread
)