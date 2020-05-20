project(dir-watch-media)

if(MSVC)
	set(dir-watch-media_PLATFORM_DEPS
		w32-pthreads)
endif()

set(dir-watch-media_HEADERS
	dir-watch-media.h)
set(dir-watch-media_SOURCES
	dir-watch-media.c)

if(WIN32)
	list(APPEND dir-watch-media_SOURCES
		dir-watch-media.rc)
endif()

add_library(dir-watch-media MODULE
	${dir-watch-media_HEADERS}
	${dir-watch-media_SOURCES})
target_link_libraries(dir-watch-media
	libobs
	${dir-watch-media_PLATFORM_DEPS})

set_target_properties(dir-watch-media PROPERTIES FOLDER "plugins/exeldro")
install_obs_plugin_with_data(dir-watch-media data)
