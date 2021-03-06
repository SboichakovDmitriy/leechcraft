cmake_minimum_required (VERSION 2.8)
project (leechcraft_dolozhee)
include (InitLCPlugin OPTIONAL)

set (QT_USE_QTNETWORK TRUE)
set (QT_USE_QTXML TRUE)
include (${QT_USE_FILE})
include_directories (
	${CMAKE_CURRENT_BINARY_DIR}
	${Boost_INCLUDE_DIR}
	${LEECHCRAFT_INCLUDE_DIR}
	)
set (SRCS
	dolozhee.cpp
	xmlgenerator.cpp
	reportwizard.cpp
	chooseuserpage.cpp
	userstatuspage.cpp
	reporttypepage.cpp
	bugreportpage.cpp
	featurerequestpage.cpp
	fileattachpage.cpp
	finalpage.cpp
	previewpage.cpp
	)
set (FORMS
	chooseuserpage.ui
	userstatuspage.ui
	reporttypepage.ui
	bugreportpage.ui
	featurerequestpage.ui
	fileattachpage.ui
	finalpage.ui
	previewpage.ui
	)
CreateTrs ("dolozhee" "en;ru_RU" COMPILED_TRANSLATIONS)
CreateTrsUpTarget ("dolozhee" "en;ru_RU" "${SRCS}" "${FORMS}" "")
QT4_WRAP_UI (UIS_H ${FORMS})

add_library (leechcraft_dolozhee SHARED
	${COMPILED_TRANSLATIONS}
	${SRCS}
	${UIS_H}
	)
target_link_libraries (leechcraft_dolozhee
	${QT_LIBRARIES}
	${LEECHCRAFT_LIBRARIES}
	)
install (TARGETS leechcraft_dolozhee DESTINATION ${LC_PLUGINS_DEST})
