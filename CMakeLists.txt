cmake_minimum_required(VERSION 3.12..4.0)

project(dir-watch-media VERSION 0.6.0)
set(PROJECT_FULL_NAME "Directory Watch Media")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.in ${CMAKE_CURRENT_SOURCE_DIR}/version.h)

if(MSVC)
	set(dir-watch-media_PLATFORM_DEPS
		w32-pthreads)
endif()

set(dir-watch-media_HEADERS
	dir-watch-media.h)
set(dir-watch-media_SOURCES
	dir-watch-media.c)

if(WIN32)
	get_filename_component(ISS_FILES_DIR "${CMAKE_BINARY_DIR}\\..\\package" ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_FILES_DIR}" ISS_FILES_DIR)
	get_filename_component(ISS_PACKAGE_DIR "${CMAKE_PACKAGE_PREFIX}\\.." ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_PACKAGE_DIR}" ISS_PACKAGE_DIR)
	get_filename_component(ISS_SOURCE_DIR "${PROJECT_SOURCE_DIR}" ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_SOURCE_DIR}" ISS_SOURCE_DIR)
	configure_file("installer.iss.in"
			"${PROJECT_BINARY_DIR}/installer.iss"
		)
	configure_file(resource.rc.in dir-watch-media.rc)
	list(APPEND dir-watch-media_SOURCES
		dir-watch-media.rc)
endif()

if(BUILD_OUT_OF_TREE)
    find_package(LibObs REQUIRED)
endif()

add_library(dir-watch-media MODULE
	${dir-watch-media_HEADERS}
	${dir-watch-media_SOURCES})
target_link_libraries(dir-watch-media
	libobs
	${dir-watch-media_PLATFORM_DEPS})

if(BUILD_OUT_OF_TREE)
    if(NOT LIB_OUT_DIR)
        set(LIB_OUT_DIR "/lib/obs-plugins")
    endif()
    if(NOT DATA_OUT_DIR)
        set(DATA_OUT_DIR "/share/obs/obs-plugins/dir-watch-media")
    endif()
    set_target_properties(dir-watch-media PROPERTIES PREFIX "")
    install(TARGETS dir-watch-media
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/${LIB_OUT_DIR})
    install(DIRECTORY data/locale
        DESTINATION ${CMAKE_INSTALL_PREFIX}/${DATA_OUT_DIR})
else()
    target_include_directories(dir-watch-media PRIVATE
        "${CMAKE_SOURCE_DIR}/UI/obs-frontend-api")
    set_target_properties(dir-watch-media PROPERTIES FOLDER "plugins/exeldro")
    install_obs_plugin_with_data(dir-watch-media data)
endif()
