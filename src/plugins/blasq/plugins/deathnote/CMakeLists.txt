cmake_minimum_required (VERSION 2.8)
project (leechcraft_blasq_deathnote)
include (InitLCPlugin OPTIONAL)

set (QT_USE_QTNETWORK TRUE)
set (QT_USE_QTXML TRUE)
set (QT_USE_QTXMLPATTERNS TRUE)
include (${QT_USE_FILE})

include_directories (
	${CMAKE_CURRENT_BINARY_DIR}
	${Boost_INCLUDE_DIR}
	${LEECHCRAFT_INCLUDE_DIR}
	)

set (DEATHNOTE_SRCS
	deathnote.cpp
	fotobilderaccount.cpp
	fotobilderservice.cpp
	registerpage.cpp
	)

set (DEATHNOTE_FORMS
	registerpage.ui
	)
# CreateTrs("blasq_vangog" "en;ru_RU" VANGOG_COMPILED_TRANSLATIONS)
# CreateTrsUpTarget("blasq_vangog" "en;ru_RU" "${VANGOG_SRCS}" "${VANGOG_FORMS}" "")

QT4_WRAP_UI (DEATHNOTE_UIS_H ${DEATHNOTE_FORMS})

add_library (leechcraft_blasq_deathnote SHARED
# 	${VANGOG_COMPILED_TRANSLATIONS}
	${DEATHNOTE_SRCS}
	${DEATHNOTE_UIS_H}
	)

target_link_libraries (leechcraft_blasq_deathnote
	${QT_LIBRARIES}
	${LEECHCRAFT_LIBRARIES}
	)

install (TARGETS leechcraft_blasq_deathnote DESTINATION ${LC_PLUGINS_DEST})
