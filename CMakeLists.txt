project(wmsystemtray)
cmake_minimum_required(VERSION 2.8)

## Config
if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release")
endif()

message("Build type: ${CMAKE_BUILD_TYPE}")

if(CMAKE_COMPILER_IS_GNUCXX)
##-fvisibility=hidden -fvisibility-inlines-hidden
	set(CMAKE_CXX_FLAGS                "-pipe -Wextra -Wall -std=c++11")
	set(CMAKE_CXX_FLAGS_DEBUG          "-O0 -ggdb")
	set(CMAKE_CXX_FLAGS_MINSIZEREL     "-s -Os -DNDEBUG")
	set(CMAKE_CXX_FLAGS_RELEASE        "-s -O2 -DNDEBUG")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-s -O2 -g")
endif()

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

# Find Qt libraries
find_package(Qt5Core    REQUIRED)
find_package(Qt5Network REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5Sql REQUIRED)
find_package(Qt5LinguistTools REQUIRED)


find_package( PkgConfig )
pkg_check_modules(PC_XPM REQUIRED xpm)
pkg_check_modules(PC_XMU REQUIRED xmu)
pkg_check_modules(PC_XFIXES REQUIRED xfixes)
pkg_check_modules(PC_XEXT REQUIRED xext)

## Library
set(SOURCES
	fdtray.c
	wmsystemtray.c
	icon.c
	warn.c
	global.c
	standalone_tray.c
	qt5_tray.cpp
)


set(HEADERS
	fdtray.h
	wmsystemtray.h
	icon.h
	warn.h
	global.h
	standalone_tray.h
	qt5_tray.h
)


## Main application
add_executable(wmsystemtray  ${SOURCES} )
target_link_libraries(wmsystemtray Qt5::Core Qt5::Widgets Qt5::Network Qt5::Sql ${PC_XPM_LIBRARIES} ${PC_XMU_LIBRARIES} ${PC_XFIXES_LIBRARIES} ${PC_XEXT_LIBRARIES})


# Copy libs
if (WIN32)
	message( "QT looks like to be installed in: " ${_qt5Core_install_prefix} )
	set(QT_DIST_DIR ${_qt5Core_install_prefix})
	add_custom_command(TARGET FOE POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy ${QT_DIST_DIR}/bin/Qt5Core.dll                   ${CMAKE_BINARY_DIR}
		COMMAND ${CMAKE_COMMAND} -E copy ${QT_DIST_DIR}/bin/Qt5Widgets.dll                ${CMAKE_BINARY_DIR}
		COMMAND ${CMAKE_COMMAND} -E copy ${QT_DIST_DIR}/bin/Qt5Gui.dll                    ${CMAKE_BINARY_DIR}
		COMMAND ${CMAKE_COMMAND} -E copy ${QT_DIST_DIR}/bin/Qt5Sql.dll                    ${CMAKE_BINARY_DIR}
		COMMAND ${CMAKE_COMMAND} -E copy ${QT_DIST_DIR}/bin/Qt5Network.dll                ${CMAKE_BINARY_DIR}
		COMMAND ${CMAKE_COMMAND} -E copy ${QT_DIST_DIR}/bin/libgcc_s_dw2-1.dll            ${CMAKE_BINARY_DIR}
		COMMAND ${CMAKE_COMMAND} -E copy ${QT_DIST_DIR}/bin/libwinpthread-1.dll           ${CMAKE_BINARY_DIR}
		COMMAND ${CMAKE_COMMAND} -E copy ${QT_DIST_DIR}/bin/libstdc++-6.dll               ${CMAKE_BINARY_DIR}
		COMMAND ${CMAKE_COMMAND} -E copy ${QT_DIST_DIR}/bin/icudt53.dll                   ${CMAKE_BINARY_DIR}
		COMMAND ${CMAKE_COMMAND} -E copy ${QT_DIST_DIR}/bin/icuin53.dll                   ${CMAKE_BINARY_DIR}
		COMMAND ${CMAKE_COMMAND} -E copy ${QT_DIST_DIR}/bin/icuuc53.dll                   ${CMAKE_BINARY_DIR}
		COMMAND ${CMAKE_COMMAND} -E copy ${QT_DIST_DIR}/plugins/sqldrivers/qsqlite.dll    ${CMAKE_BINARY_DIR}
		COMMAND ${CMAKE_COMMAND} -E copy ${QT_DIST_DIR}/plugins/platforms/qwindows.dll    ${CMAKE_BINARY_DIR}
	)
endif()