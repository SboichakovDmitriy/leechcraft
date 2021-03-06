cmake_minimum_required (VERSION 2.8)
project (leechcraft_sb2)
include (InitLCPlugin OPTIONAL)
include (CMakeDependentOption)

set (CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package (QJSON REQUIRED)

cmake_dependent_option (WITH_X11 "Enable X11 support on Linux" ON "UNIX AND NOT APPLE" OFF)

set (QT_USE_QTDECLARATIVE TRUE)
include (${QT_USE_FILE})
include_directories (
	${CMAKE_CURRENT_BINARY_DIR}
	${Boost_INCLUDE_DIR}
	${LEECHCRAFT_INCLUDE_DIR}
	${QJSON_INCLUDE_DIR}
	)
set (SRCS
	sb2.cpp
	sbview.cpp
	viewmanager.cpp
	desaturateeffect.cpp
	quarkproxy.cpp
	quarkmanager.cpp
	quarksettingsmanager.cpp
	tablistview.cpp
	tabunhidelistview.cpp
	launcherdroparea.cpp
	launchercomponent.cpp
	lcmenucomponent.cpp
	traycomponent.cpp
	baseactioncomponent.cpp
	dockactioncomponent.cpp
	quarkunhidelistview.cpp
	quarkorderview.cpp
	declarativewindow.cpp
	viewgeometrymanager.cpp
	)

if (WITH_X11)
	find_package (X11)

	if (X11_FOUND)
		add_definitions (-DWITH_X11)
	endif ()
endif ()

set (SB2_RESOURCES sb2resources.qrc)
QT4_ADD_RESOURCES (SB2_RCCS ${SB2_RESOURCES})

add_library (leechcraft_sb2 SHARED
	${COMPILED_TRANSLATIONS}
	${SRCS}
	${MOC_SRCS}
	${SB2_RCCS}
	)
target_link_libraries (leechcraft_sb2
	${QT_LIBRARIES}
	${LEECHCRAFT_LIBRARIES}
	${QJSON_LIBRARIES}
	)
install (TARGETS leechcraft_sb2 DESTINATION ${LC_PLUGINS_DEST})
install (DIRECTORY share/qml/sb2 DESTINATION ${LC_QML_DEST})
