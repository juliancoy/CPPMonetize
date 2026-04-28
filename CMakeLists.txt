cmake_minimum_required(VERSION 3.21)
project(CPPMonetize VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network Gui)

add_library(cppmonetize STATIC
    src/Models.cpp
    src/MonetizeClient.cpp
    src/OAuthDesktopFlow.cpp
    src/AuthIdentity.cpp
    src/ApiErrorUtils.cpp
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

set(CPPMONETIZE_DEFAULT_SUPABASE_URL "" CACHE STRING
    "Optional built-in Supabase URL fallback for desktop auth (public client config).")
set(CPPMONETIZE_DEFAULT_SUPABASE_ANON_KEY "" CACHE STRING
    "Optional built-in Supabase anon key fallback for desktop auth (public client config).")

if(CPPMONETIZE_DEFAULT_SUPABASE_URL)
    set(_cpmon_url_escaped "\"${CPPMONETIZE_DEFAULT_SUPABASE_URL}\"")
    target_compile_definitions(cppmonetize PRIVATE
        "CPPMONETIZE_DEFAULT_SUPABASE_URL=${_cpmon_url_escaped}")
endif()

if(CPPMONETIZE_DEFAULT_SUPABASE_ANON_KEY)
    set(_cpmon_anon_escaped "\"${CPPMONETIZE_DEFAULT_SUPABASE_ANON_KEY}\"")
    target_compile_definitions(cppmonetize PRIVATE
        "CPPMONETIZE_DEFAULT_SUPABASE_ANON_KEY=${_cpmon_anon_escaped}")
endif()

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

    add_executable(cppmonetize_oauth_tests
        tests/test_oauth_desktop_flow.cpp
    )
    target_link_libraries(cppmonetize_oauth_tests PRIVATE cppmonetize Qt6::Test)
    add_test(NAME cppmonetize_oauth_tests COMMAND cppmonetize_oauth_tests)
endif()
