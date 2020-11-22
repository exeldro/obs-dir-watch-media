#include <obs-module.h>
#include <graphics/image-file.h>
#include <util/platform.h>
#include <util/dstr.h>
#include <sys/stat.h>

#define do_log(level, format, ...)                     \
	blog(level, "[dir_watch_media: '%s'] " format, \
	     obs_source_get_name(ss->source), ##__VA_ARGS__)

#define debug(format, ...) blog(LOG_DEBUG, format, ##__VA_ARGS__)
#define info(format, ...) blog(LOG_INFO, format, ##__VA_ARGS__)
#define warn(format, ...) blog(LOG_WARNING, format, ##__VA_ARGS__)

#if __linux__ || __APPLE__
/* stracsecmp is the POSIX version of strcmpi */
#define strcmpi strcasecmp
#endif

/* Settings */
#define S_DWM_ID "dir_watch_media"
#define S_DIRECTORY "dir"
#define S_SORT_BY "sort_by"
#define S_FILTER "filter"
#define S_EXTENSION "extension"
#define S_FFMPEG_SOURCE "ffmpeg_source"
#define S_LOCAL_FILE "local_file"
#define S_IS_LOCAL_FILE "is_local_file"
#define S_RESTART "restart"
#define S_VLC_SOURCE "vlc_source"
#define S_PLAYLIST "playlist"
#define S_VALUE "value"
#define S_IMAGE_SOURCE "image_source"
#define S_FILE "file"
#define S_SORT_BY "sort_by"
#define S_CLEAR_HOTKEY_ID "dwm_clear"
#define S_REMOVE_LAST_HOTKEY_ID "dwm_remove_last"
#define S_REMOVE_FIRST_HOTKEY_ID "dwm_remove_first"
#define S_DELETE_LAST_HOTKEY_ID "dwm_delete_last"
#define S_DELETE_FIRST_HOTKEY_ID "dwm_delete_first"
#define S_RANDOM_HOTKEY_ID "dwm_random"
#define S_REFRESH_HOTKEY_ID "dwm_refresh"

/* Translation */
#define T_(s) obs_module_text(s)
#define T_DIRECTORY T_("Directory")
#define T_DWM_DESCRIPTION T_("DWM.Description")
#define T_NAME T_("DWM.Name")
#define T_CLEAR_HOTKEY_NAME T_("DWM.Clear")
#define T_RANDOM_HOTKEY_NAME T_("DWM.Random")
#define T_REFRESH_HOTKEY_NAME T_("DWM.Refresh")
#define T_REMOVE_LAST_HOTKEY_NAME T_("DWM.Remove.Last")
#define T_REMOVE_FIRST_HOTKEY_NAME T_("DWM.Remove.First")
#define T_DELETE_LAST_HOTKEY_NAME T_("DWM.Delete.Last")
#define T_DELETE_FIRST_HOTKEY_NAME T_("DWM.Delete.First")
#define T_SORT_BY T_("DWM.SortBy")
#define T_CREATED_NEWEST T_("DWM.Created.Newest")
#define T_CREATED_OLDEST T_("DWM.Created.Oldest")
#define T_MODIFIED_NEWEST T_("DWM.Modified.Newest")
#define T_MODIFIED_OLDEST T_("DWM.Modified.Oldest")
#define T_ALPHA_FIRST T_("DWM.Alphabetically.First")
#define T_ALPHA_LAST T_("DWM.Alphabetically.Last")
#define T_EXTENSION T_("DWM.Extension")
#define T_FILTER T_("DWM.Filter")

enum sort_by {
	created_newest,
	created_oldest,
	modified_newest,
	modified_oldest,
	alphabetically_first,
	alphabetically_last,
};

struct dir_watch_media_source {
	obs_source_t *source;
	char *directory;
	char *file;
	char *filter;
	char *extension;
	char *delete_file;
	enum sort_by sort_by;
	time_t time;
	bool hotkeys_added;
};

static const char *dir_watch_media_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return T_NAME;
}

static void dir_watch_media_source_update(void *data, obs_data_t *settings)
{
	struct dir_watch_media_source *context = data;
	const char *dir = obs_data_get_string(settings, S_DIRECTORY);

	if (!context->directory || strcmp(dir, context->directory) != 0) {
		if (context->directory)
			bfree(context->directory);
		context->directory = bstrdup(dir);
	}
	const enum sort_by sort_by = obs_data_get_int(settings, S_SORT_BY);
	if (sort_by != context->sort_by) {
		context->sort_by = sort_by;
		context->time = 0;
	}

	const char *filter = obs_data_get_string(settings, S_FILTER);
	if (!context->filter || strcmp(filter, context->filter) != 0) {
		if (context->filter) {
			bfree(context->filter);
			context->filter = NULL;
			context->time = 0;
		}
		if (strlen(filter) > 0) {
			context->filter = bstrdup(filter);
			context->time = 0;
		}
	}

	const char *extension = obs_data_get_string(settings, S_EXTENSION);
	if (!context->extension || strcmp(extension, context->extension) != 0) {
		if (context->extension) {
			bfree(context->extension);
			context->extension = NULL;
			context->time = 0;
		}
		if (strlen(extension) > 0) {
			context->extension = bstrdup(extension);
			context->time = 0;
		}
	}
}

static void dir_watch_media_clear(void *data, obs_hotkey_id hotkey_id,
				  obs_hotkey_t *hotkey, bool pressed)
{
	struct dir_watch_media_source *context = data;

	if (!pressed)
		return;

	obs_source_t *parent = obs_filter_get_parent(context->source);
	if (!parent) {
		return;
	}

	obs_data_t *settings = obs_source_get_settings(parent);
	const char *id = obs_source_get_unversioned_id(parent);
	if (strcmp(id, S_FFMPEG_SOURCE) == 0) {
		obs_data_set_string(settings, S_LOCAL_FILE, "");
		obs_data_set_bool(settings, S_IS_LOCAL_FILE, true);
		obs_source_update(parent, settings);
		proc_handler_t *ph = obs_source_get_proc_handler(parent);
		if (ph) {
			calldata_t cd = {0};
			proc_handler_call(ph, S_RESTART, &cd);
			calldata_free(&cd);
		}
	} else if (strcmp(id, S_VLC_SOURCE) == 0) {
		obs_data_array_t *array =
			obs_data_get_array(settings, S_PLAYLIST);
		if (!array) {
			array = obs_data_array_create();
			obs_data_set_array(settings, S_PLAYLIST, array);
		}
		bool twice = false;
		size_t count = obs_data_array_count(array);
		for (size_t i = 0; i < count; i++) {
			obs_data_array_erase(array, 0);
		}
		obs_source_update(parent, settings);
		obs_data_array_release(array);
	} else if (strcmp(id, S_IMAGE_SOURCE) == 0) {
		obs_data_set_string(settings, S_FILE, "");
		obs_source_update(parent, settings);
	}
	obs_data_release(settings);
	UNUSED_PARAMETER(hotkey);
	UNUSED_PARAMETER(hotkey_id);
}

static void dir_watch_media_random(void *data, obs_hotkey_id hotkey_id,
				   obs_hotkey_t *hotkey, bool pressed)
{
	struct dir_watch_media_source *context = data;

	if (!pressed)
		return;

	obs_source_t *parent = obs_filter_get_parent(context->source);
	if (!parent) {
		return;
	}

	if (!context->directory)
		return;

	os_dir_t *dir = os_opendir(context->directory);
	if (!dir)
		return;

	struct dstr selected_path;
	dstr_init(&selected_path);

	long long count = 0;
	struct os_dirent *ent = os_readdir(dir);
	while (ent) {
		if (ent->directory) {
			ent = os_readdir(dir);
			continue;
		}
		if (context->filter &&
		    strstr(ent->d_name, context->filter) == NULL) {
			ent = os_readdir(dir);
			continue;
		}
		const char *extension = os_get_path_extension(ent->d_name);
		if (context->extension && extension &&
		    astrcmpi(context->extension, extension) != 0 &&
		    astrcmpi(context->extension, extension + 1)) {
			ent = os_readdir(dir);
			continue;
		}
		count++;
		const int r = rand();
		if (count <= 1 || r % count == 0) {
			dstr_copy(&selected_path, context->directory);
			dstr_cat_ch(&selected_path, '/');
			dstr_cat(&selected_path, ent->d_name);
		}
		ent = os_readdir(dir);
	}
	if (!count) {
		dstr_free(&selected_path);
		return;
	}

	obs_data_t *settings = obs_source_get_settings(parent);
	const char *id = obs_source_get_unversioned_id(parent);
	if (strcmp(id, S_FFMPEG_SOURCE) == 0) {
		obs_data_set_string(settings, S_LOCAL_FILE,
				    selected_path.array);
		obs_data_set_bool(settings, S_IS_LOCAL_FILE, true);
		obs_source_update(parent, settings);
		proc_handler_t *ph = obs_source_get_proc_handler(parent);
		if (ph) {
			calldata_t cd = {0};
			proc_handler_call(ph, S_RESTART, &cd);
			calldata_free(&cd);
		}
	} else if (strcmp(id, S_VLC_SOURCE) == 0) {
		obs_data_array_t *array =
			obs_data_get_array(settings, S_PLAYLIST);
		if (!array) {
			array = obs_data_array_create();
			obs_data_set_array(settings, S_PLAYLIST, array);
		}
		bool twice = false;
		const size_t count = obs_data_array_count(array);
		for (size_t i = 0; i < count; i++) {
			obs_data_t *item = obs_data_array_item(array, i);
			if (strcmpi(obs_data_get_string(item, S_VALUE),
				    selected_path.array) == 0) {
				twice = true;
			}
			obs_data_release(item);
		}
		if (!twice) {
			obs_data_t *item = obs_data_create();
			obs_data_set_string(item, S_VALUE, selected_path.array);
			obs_data_array_push_back(array, item);
			obs_data_release(item);
			obs_source_update(parent, settings);
		}
		obs_data_array_release(array);
	} else if (strcmp(id, S_IMAGE_SOURCE) == 0) {
		obs_data_set_string(settings, S_FILE, selected_path.array);
		obs_source_update(parent, settings);
	}
	obs_data_release(settings);
	dstr_free(&selected_path);
}

static void dir_watch_media_refresh(void *data, obs_hotkey_id hotkey_id,
				  obs_hotkey_t *hotkey, bool pressed)
{
	struct dir_watch_media_source *context = data;

	if (!pressed)
		return;

	obs_source_t *parent = obs_filter_get_parent(context->source);
	if (!parent) {
		return;
	}

	obs_data_t *settings = obs_source_get_settings(parent);
	obs_source_update(parent, settings);
	obs_data_release(settings);
	UNUSED_PARAMETER(hotkey);
	UNUSED_PARAMETER(hotkey_id);
}

static void dir_watch_media_remove(void *data, bool first, bool delete)
{
	struct dir_watch_media_source *context = data;

	obs_source_t *parent = obs_filter_get_parent(context->source);
	if (!parent) {
		return;
	}

	const char *id = obs_source_get_unversioned_id(parent);
	if (strcmp(id, S_VLC_SOURCE) != 0) {
		return;
	}
	obs_data_t *settings = obs_source_get_settings(parent);
	obs_data_array_t *array = obs_data_get_array(settings, S_PLAYLIST);
	if (!array) {
		array = obs_data_array_create();
		obs_data_set_array(settings, S_PLAYLIST, array);
	}
	const size_t count = obs_data_array_count(array);
	if (count > 0) {
		size_t index = first ? 0 : count - 1;
		if (delete) {
			obs_data_t *item = obs_data_array_item(array, index);
			const char *filepath =
				obs_data_get_string(item, S_VALUE);
			if (filepath && os_file_exists(filepath)) {
				if (context->delete_file)
					bfree(context->delete_file);
				context->delete_file = bstrdup(filepath);
			}
		}
		obs_data_array_erase(array, index);
	}
	obs_source_update(parent, settings);
	obs_data_array_release(array);
	obs_data_release(settings);
}

static void dir_watch_media_remove_last(void *data, obs_hotkey_id hotkey_id,
					obs_hotkey_t *hotkey, bool pressed)
{
	if (!pressed)
		return;
	dir_watch_media_remove(data, false, false);
	UNUSED_PARAMETER(hotkey);
	UNUSED_PARAMETER(hotkey_id);
}

static void dir_watch_media_remove_first(void *data, obs_hotkey_id hotkey_id,
					 obs_hotkey_t *hotkey, bool pressed)
{
	if (!pressed)
		return;
	dir_watch_media_remove(data, true, false);
	UNUSED_PARAMETER(hotkey);
	UNUSED_PARAMETER(hotkey_id);
}

static void dir_watch_media_delete_last(void *data, obs_hotkey_id hotkey_id,
					obs_hotkey_t *hotkey, bool pressed)
{
	if (!pressed)
		return;
	dir_watch_media_remove(data, false, true);
	UNUSED_PARAMETER(hotkey);
	UNUSED_PARAMETER(hotkey_id);
}

static void dir_watch_media_delete_first(void *data, obs_hotkey_id hotkey_id,
					 obs_hotkey_t *hotkey, bool pressed)
{
	if (!pressed)
		return;
	dir_watch_media_remove(data, true, true);
	UNUSED_PARAMETER(hotkey);
	UNUSED_PARAMETER(hotkey_id);
}

static void dir_watch_media_source_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, S_SORT_BY, modified_newest);
}

static void *dir_watch_media_source_create(obs_data_t *settings,
					   obs_source_t *source)
{
	struct dir_watch_media_source *context =
		bzalloc(sizeof(struct dir_watch_media_source));
	context->source = source;

	dir_watch_media_source_update(context, settings);
	return context;
}

static void dir_watch_media_source_destroy(void *data)
{
	struct dir_watch_media_source *context = data;
	bfree(context->delete_file);
	bfree(context->directory);
	bfree(context->extension);
	bfree(context->filter);
	bfree(context->file);
	bfree(context);
}

static void dir_watch_media_source_tick(void *data, float seconds)
{
	struct dir_watch_media_source *context = data;
	if (context->delete_file) {
		if (os_file_exists(context->delete_file)) {
			os_unlink(context->delete_file);
		} else {
			bfree(context->delete_file);
			context->delete_file = NULL;
		}
	}

	if (context->hotkeys_added)
		return;
	obs_source_t *parent = obs_filter_get_parent(context->source);
	if (!parent)
		return;

	context->hotkeys_added = true;
	obs_hotkey_register_source(parent, S_CLEAR_HOTKEY_ID,
				   T_CLEAR_HOTKEY_NAME, dir_watch_media_clear,
				   context);
	obs_hotkey_register_source(parent, S_RANDOM_HOTKEY_ID,
				   T_RANDOM_HOTKEY_NAME, dir_watch_media_random,
				   context);
	obs_hotkey_register_source(parent, S_REFRESH_HOTKEY_ID,
				   T_REFRESH_HOTKEY_NAME, dir_watch_media_refresh,
				   context);
	const char *id = obs_source_get_unversioned_id(parent);
	if (strcmp(id, S_VLC_SOURCE) != 0)
		return;
	obs_hotkey_register_source(parent, S_REMOVE_LAST_HOTKEY_ID,
				   T_REMOVE_LAST_HOTKEY_NAME,
				   dir_watch_media_remove_last, context);
	obs_hotkey_register_source(parent, S_REMOVE_FIRST_HOTKEY_ID,
				   T_REMOVE_FIRST_HOTKEY_NAME,
				   dir_watch_media_remove_first, context);

	obs_hotkey_register_source(parent, S_DELETE_LAST_HOTKEY_ID,
				   T_DELETE_LAST_HOTKEY_NAME,
				   dir_watch_media_delete_last, context);
	obs_hotkey_register_source(parent, S_DELETE_FIRST_HOTKEY_ID,
				   T_DELETE_FIRST_HOTKEY_NAME,
				   dir_watch_media_delete_first, context);
}

static obs_properties_t *dir_watch_media_source_properties(void *data)
{
	struct dir_watch_media_source *s = data;

	obs_properties_t *props = obs_properties_create();

	obs_properties_add_path(props, S_DIRECTORY, T_DIRECTORY,
				OBS_PATH_DIRECTORY, NULL, s->directory);
	obs_property_t *prop = obs_properties_add_list(props, S_SORT_BY,
						       T_SORT_BY,
						       OBS_COMBO_TYPE_LIST,
						       OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(prop, T_CREATED_NEWEST, created_newest);
	obs_property_list_add_int(prop, T_CREATED_OLDEST, created_oldest);
	obs_property_list_add_int(prop, T_MODIFIED_NEWEST, modified_newest);
	obs_property_list_add_int(prop, T_MODIFIED_OLDEST, modified_oldest);
	obs_property_list_add_int(prop, T_ALPHA_FIRST, alphabetically_first);
	obs_property_list_add_int(prop, T_ALPHA_LAST, alphabetically_last);

	obs_properties_add_text(props, S_EXTENSION, T_EXTENSION,
				OBS_TEXT_DEFAULT);
	obs_properties_add_text(props, S_FILTER, T_FILTER, OBS_TEXT_DEFAULT);
	return props;
}

void dir_watch_media_source_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);
	struct dir_watch_media_source *context = data;
	obs_source_skip_video_filter(context->source);
	if (!context->directory)
		return;

	os_dir_t *dir = os_opendir(context->directory);
	if (!dir)
		return;

	struct dstr dir_path;
	struct dstr selected_path;
	dstr_init(&dir_path);
	dstr_init(&selected_path);
	time_t time = context->time;
	char *file = NULL;

	struct os_dirent *ent = os_readdir(dir);
	while (ent) {
		if (ent->directory) {
			ent = os_readdir(dir);
			continue;
		}
		if (context->filter &&
		    strstr(ent->d_name, context->filter) == NULL) {
			ent = os_readdir(dir);
			continue;
		}
		const char *extension = os_get_path_extension(ent->d_name);
		if (context->extension && extension &&
		    astrcmpi(context->extension, extension) != 0 &&
		    astrcmpi(context->extension, extension + 1)) {
			ent = os_readdir(dir);
			continue;
		}

		dstr_copy(&dir_path, context->directory);
		dstr_cat_ch(&dir_path, '/');
		dstr_cat(&dir_path, ent->d_name);

		if (context->sort_by == alphabetically_first) {
			if (!file || astrcmpi(file, ent->d_name) >= 0) {
				bfree(file);
				file = bstrdup(ent->d_name);
				dstr_copy_dstr(&selected_path, &dir_path);
			}
		} else if (context->sort_by == alphabetically_last) {
			if (!file || astrcmpi(file, ent->d_name) <= 0) {
				bfree(file);
				file = bstrdup(ent->d_name);
				dstr_copy_dstr(&selected_path, &dir_path);
			}
		} else {
			struct stat stats;
			if (os_stat(dir_path.array, &stats) == 0 &&
			    stats.st_size > 0) {
				if (context->sort_by == created_newest) {
					if (time == 0 ||
					    stats.st_ctime >= time) {
						dstr_copy_dstr(&selected_path,
							       &dir_path);
						time = stats.st_ctime;
					}
				} else if (context->sort_by == created_oldest) {
					if (time == 0 ||
					    stats.st_ctime <= time) {
						dstr_copy_dstr(&selected_path,
							       &dir_path);
						time = stats.st_ctime;
					}
				} else if (context->sort_by ==
					   modified_newest) {
					if (time == 0 ||
					    stats.st_mtime >= time) {
						dstr_copy_dstr(&selected_path,
							       &dir_path);
						time = stats.st_mtime;
					}
				} else if (context->sort_by ==
					   modified_oldest) {
					if (time == 0 ||
					    stats.st_mtime <= time) {
						dstr_copy_dstr(&selected_path,
							       &dir_path);
						time = stats.st_mtime;
					}
				}
			}
		}
		ent = os_readdir(dir);
	}

	context->time = time;
	bfree(file);
	dstr_free(&dir_path);
	os_closedir(dir);
	if (!selected_path.array || !selected_path.len) {
		dstr_free(&selected_path);
		return;
	}
	if (context->file && strcmp(context->file, selected_path.array) == 0) {
		dstr_free(&selected_path);
		return;
	}
	FILE *f = os_fopen(selected_path.array, "rb+");
	if (!f) {
		dstr_free(&selected_path);
		return;
	}
	fclose(f);
	bfree(context->file);
	context->file = bstrdup(selected_path.array);
	dstr_free(&selected_path);
	obs_source_t *parent = obs_filter_get_parent(context->source);
	if (!parent) {
		return;
	}
	obs_data_t *settings = obs_source_get_settings(parent);
	const char *id = obs_source_get_unversioned_id(parent);
	if (strcmp(id, S_FFMPEG_SOURCE) == 0) {
		obs_data_set_string(settings, S_LOCAL_FILE, context->file);
		obs_data_set_bool(settings, S_IS_LOCAL_FILE, true);
		obs_source_update(parent, settings);
		proc_handler_t *ph = obs_source_get_proc_handler(parent);
		if (ph) {
			calldata_t cd = {0};
			proc_handler_call(ph, S_RESTART, &cd);
			calldata_free(&cd);
		}
	} else if (strcmp(id, S_VLC_SOURCE) == 0) {
		obs_data_array_t *array =
			obs_data_get_array(settings, S_PLAYLIST);
		if (!array) {
			array = obs_data_array_create();
			obs_data_set_array(settings, S_PLAYLIST, array);
		}
		bool twice = false;
		size_t count = obs_data_array_count(array);
		for (size_t i = 0; i < count; i++) {
			obs_data_t *item = obs_data_array_item(array, i);
			if (strcmpi(obs_data_get_string(item, S_VALUE),
				    context->file) == 0) {
				twice = true;
			}
			obs_data_release(item);
		}
		if (!twice) {
			obs_data_t *item = obs_data_create();
			obs_data_set_string(item, S_VALUE, context->file);
			obs_data_array_push_back(array, item);
			obs_data_release(item);
			obs_source_update(parent, settings);
		}
		obs_data_array_release(array);
	} else if (strcmp(id, S_IMAGE_SOURCE) == 0) {
		obs_data_set_string(settings, S_FILE, context->file);
		obs_source_update(parent, settings);
	}
	obs_data_release(settings);
}
static void dir_watch_media_source_filter_remove(void *data,
						 obs_source_t *parent)
{
}

struct obs_source_info dir_watch_media_info = {
	.id = S_DWM_ID,
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = dir_watch_media_source_get_name,
	.create = dir_watch_media_source_create,
	.destroy = dir_watch_media_source_destroy,
	.update = dir_watch_media_source_update,
	.get_defaults = dir_watch_media_source_defaults,
	.video_render = dir_watch_media_source_render,
	.video_tick = dir_watch_media_source_tick,
	.get_properties = dir_watch_media_source_properties,
	.filter_remove = dir_watch_media_source_filter_remove,
};

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("dir-watch-media", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return T_DWM_DESCRIPTION;
}

bool obs_module_load(void)
{
	obs_register_source(&dir_watch_media_info);
	return true;
}
