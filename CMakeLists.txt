cmake_minimum_required(VERSION 3.21)
project(CPPMonetize VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network Gui)

add_library(cppmonetize STATIC
    src/Models.cpp
    src/MonetizeClient.cpp
    src/OAuthDesktopFlow.cpp
    src/TokenStore.cpp
)

add_library(CPPMonetize::cppmonetize ALIAS cppmonetize)

target_include_directories(cppmonetize
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(cppmonetize
    PUBLIC
        Qt6::Core
        Qt6::Network
        Qt6::Gui
)

target_compile_features(cppmonetize PUBLIC cxx_std_20)

include(GNUInstallDirs)
install(TARGETS cppmonetize
    EXPORT CPPMonetizeTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT CPPMonetizeTargets
    FILE CPPMonetizeTargets.cmake
    NAMESPACE CPPMonetize::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CPPMonetize
)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(cppmonetize_tests
        tests/test_models.cpp
    )
    target_link_libraries(cppmonetize_tests PRIVATE cppmonetize Qt6::Test)
    add_test(NAME cppmonetize_tests COMMAND cppmonetize_tests)
endif()
