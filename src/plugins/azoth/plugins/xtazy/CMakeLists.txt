cmake_minimum_required (VERSION 2.8)
project (leechcraft_azoth_xtazy)
include (InitLCPlugin OPTIONAL)

if (NOT WIN32)
	set (QT_USE_QTDBUS TRUE)
endif ()
include (${QT_USE_FILE})

include_directories (${AZOTH_INCLUDE_DIR}
	${CMAKE_CURRENT_BINARY_DIR}
	${LEECHCRAFT_INCLUDE_DIR}
	${LEECHCRAFT_INCLUDE_DIR}/azoth
	)
set (XTAZY_SRCS
	xtazy.cpp
	xmlsettingsmanager.cpp
	tracksharedialog.cpp
	)
set (XTAZY_FORMS
	tracksharedialog.ui
	)
set (XTAZY_RESOURCES azothxtazyresources.qrc)
CreateTrs ("azoth_xtazy" "en;es;ru_RU;uk_UA" XTAZY_COMPILED_TRANSLATIONS)
QT4_ADD_RESOURCES (XTAZY_RCCS ${XTAZY_RESOURCES})
QT4_WRAP_UI (XTAZY_UIS_H ${XTAZY_FORMS})


add_library (leechcraft_azoth_xtazy SHARED
	${XTAZY_COMPILED_TRANSLATIONS}
	${XTAZY_SRCS}
	${XTAZY_RCCS}
	${XTAZY_UIS_H}
	)
target_link_libraries (leechcraft_azoth_xtazy
	${QT_LIBRARIES}
	${LEECHCRAFT_LIBRARIES}
	)
install (TARGETS leechcraft_azoth_xtazy DESTINATION ${LC_PLUGINS_DEST})
install (FILES azothxtazysettings.xml DESTINATION ${LC_SETTINGS_DEST})
install (FILES ${XTAZY_COMPILED_TRANSLATIONS} DESTINATION ${LC_TRANSLATIONS_DEST})
