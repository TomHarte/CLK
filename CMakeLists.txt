cmake_minimum_required(VERSION 3.19 FATAL_ERROR)

project(CLK
	LANGUAGES CXX
	VERSION 24.01.22
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CLK_UIS "SDL")
list(PREPEND CLK_UIS "Qt")

#if(APPLE)
#	list(PREPEND CLK_UIS "MAC")
#	set(CLK_DEFAULT_UI "MAC")
#else()
	set(CLK_DEFAULT_UI "SDL")
#endif()

set(CLK_UI ${CLK_DEFAULT_UI} CACHE STRING "User interface")
set_property(CACHE CLK_UI PROPERTY STRINGS ${CLK_UIS})

if(NOT CLK_UI IN_LIST CLK_UIS)
	list(JOIN CLK_UIS ", " CLK_UIS_PRETTY)
	message(FATAL_ERROR "Invalid value for 'CLK_UI'; must be one of ${CLK_UIS_PRETTY}")
endif()

message(STATUS "Configuring for ${CLK_UI} UI")

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include_directories(".")
include("CLK_SOURCES")

add_executable(clksignal ${CLK_SOURCES})

if(MSVC)
	target_compile_options(clksignal PRIVATE /W4)
else()
	# TODO: Add -Wpedandic.
	target_compile_options(clksignal PRIVATE -Wall -Wextra)
endif()

find_package(ZLIB REQUIRED)
target_link_libraries(clksignal PRIVATE ZLIB::ZLIB)

if(CLK_UI STREQUAL "MAC")
	enable_language(OBJC OBJCXX SWIFT)
	# TODO: Build the Mac version.
else()
	find_package(OpenGL REQUIRED)
	target_link_libraries(clksignal PRIVATE OpenGL::GL)
	if(APPLE)
		target_compile_definitions(clksignal PRIVATE "GL_SILENCE_DEPRECATION" "IGNORE_APPLE")
	endif()
endif()

if(CLK_UI STREQUAL "Qt")
	set( CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} $ENV{QT6_DIR} )
	find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGLWidgets Multimedia)
	target_link_libraries(clksignal PRIVATE Qt::Widgets Qt::OpenGLWidgets Qt::Multimedia)
elseif(APPLE)
	set(BLA_VENDOR Apple)
	find_package(BLAS REQUIRED)
	target_link_libraries(clksignal PRIVATE BLAS::BLAS)
endif()

if(CLK_UI STREQUAL "SDL")
	find_package(SDL2 REQUIRED CONFIG REQUIRED COMPONENTS SDL2)
	target_link_libraries(clksignal PRIVATE SDL2::SDL2)
endif()

# TODO: Investigate building on Windows.
