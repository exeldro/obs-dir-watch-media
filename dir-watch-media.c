#include <obs-module.h>
#include <graphics/image-file.h>
#include <util/platform.h>
#include <util/dstr.h>
#include <sys/stat.h>
#include "obs-internal.h"

#define do_log(level, format, ...) \
	blog(level, "[dir_watch_media: '%s'] " format, \
			obs_source_get_name(ss->source), ##__VA_ARGS__)

#define debug(format, ...) \
	blog(LOG_DEBUG, format, ##__VA_ARGS__)
#define info(format, ...) \
	blog(LOG_INFO, format, ##__VA_ARGS__)
#define warn(format, ...) \
	blog(LOG_WARNING, format, ##__VA_ARGS__)


enum sort_by
{
	created_newest,
	created_oldest,
	modified_newest,
	modified_oldest,
	alphabetically_first,
	alphabetically_last,
};
struct dir_watch_media_source {
	obs_source_t *source;	
	char         *directory;
	char         *file;
	enum sort_by sort_by;
};

static const char *dir_watch_media_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "Directory watch media";
}



static void dir_watch_media_source_update(void *data, obs_data_t *settings)
{
	struct dir_watch_media_source *context = data;
	const char *dir = obs_data_get_string(settings, "dir");

	if (!context->directory || strcmp(dir, context->directory) != 0) {
		if (context->directory)
			bfree(context->directory);
		context->directory = bstrdup(dir);
	}
	context->sort_by = obs_data_get_int(settings,"sort_by");
}

static void dir_watch_media_source_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "sort_by", modified_newest);
}


static void *dir_watch_media_source_create(obs_data_t *settings, obs_source_t *source)
{
	struct dir_watch_media_source *context = bzalloc(sizeof(struct dir_watch_media_source));
	context->source = source;

	dir_watch_media_source_update(context, settings);
	return context;
}

static void dir_watch_media_source_destroy(void *data)
{
	struct dir_watch_media_source *context = data;
	if (context->directory)
		bfree(context->directory);
	bfree(context);
}


static void dir_watch_media_source_tick(void *data, float seconds)
{
	struct dir_watch_media_source* context = data;
	if(!context->file)
		return;
	obs_source_t* parent = obs_filter_get_parent(context->source);
	if (parent)
	{
		proc_handler_t* ph = obs_source_get_proc_handler(parent);
		if (ph) {
			calldata_t cd = { 0 };
			proc_handler_call(ph, "get_duration", &cd);
			proc_handler_call(ph, "get_nb_frames", &cd);
			const uint64_t duration = (uint64_t)calldata_int(&cd, "duration");
			const uint64_t frames = (uint64_t)calldata_int(&cd, "num_frames");
			calldata_free(&cd);
			if (frames <= 1 && context->file)
			{
				bfree(context->file);
				context->file = NULL;
			}
		}
	}

}


static obs_properties_t * dir_watch_media_source_properties(void *data)
{
	struct dir_watch_media_source *s = data;

	obs_properties_t *props = obs_properties_create();

	obs_properties_add_path(props,
			"dir", obs_module_text("Directory"),
			OBS_PATH_DIRECTORY, NULL, s->directory);
	obs_property_t* prop = obs_properties_add_list(props,"sort_by","Sort by", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(prop,"Created newest",created_newest);
	obs_property_list_add_int(prop,"Created oldest",created_oldest);
	obs_property_list_add_int(prop,"Modified newest",modified_newest);
	obs_property_list_add_int(prop,"Modified oldest",modified_oldest);
	obs_property_list_add_int(prop,"Alphabetically first",alphabetically_first);
	obs_property_list_add_int(prop,"Alphabetically last",alphabetically_last);
	return props;
}

void dir_watch_media_source_render(void* data, gs_effect_t* effect)
{
	UNUSED_PARAMETER(effect);
	struct dir_watch_media_source* context = data;
	obs_source_skip_video_filter(context->source);
	if (!context->directory)
		return;

	os_dir_t* dir = os_opendir(context->directory);
	if (!dir)
		return;

	struct dstr dir_path;
	dstr_init(&dir_path);
	struct dstr selected_path;
	dstr_init(&selected_path);
	time_t time = 0;
	char* file = NULL;

	struct os_dirent* ent = os_readdir(dir);
	while (ent)
	{
		if (ent->directory) {
			ent = os_readdir(dir);
			continue;
		}
		//const char* ext = os_get_path_extension(ent->d_name);

		dstr_copy(&dir_path, context->directory);
		dstr_cat_ch(&dir_path, '/');
		dstr_cat(&dir_path, ent->d_name);

		if(context->sort_by == alphabetically_first)
		{
			if(!file || astrcmpi(file, ent->d_name) > 0)
			{
				if(file)
					bfree(file);
				file = bstrdup(ent->d_name);
				dstr_copy_dstr(&selected_path, &dir_path);
			}
		}else if(context->sort_by == alphabetically_last)
		{
			if(!file || astrcmpi(file, ent->d_name) < 0)
			{
				if(file)
					bfree(file);
				file = bstrdup(ent->d_name);
				dstr_copy_dstr(&selected_path, &dir_path);
			}
		}else{
			struct stat stats;
			if (os_stat(dir_path.array, &stats) == 0 && stats.st_size > 0)
			{
				if(context->sort_by == created_newest)
				{
					if (time == 0 || stats.st_ctime > time)
					{
						dstr_copy_dstr(&selected_path, &dir_path);
						time = stats.st_ctime;
					}
				}
				else if(context->sort_by == created_oldest)
				{
					if (time == 0 || stats.st_ctime < time)
					{
						dstr_copy_dstr(&selected_path, &dir_path);
						time = stats.st_ctime;
					}
				}
				else if(context->sort_by == modified_newest)
				{
					if (time == 0 || stats.st_mtime > time)
					{
						dstr_copy_dstr(&selected_path, &dir_path);
						time = stats.st_mtime;
					}
				}
				else if(context->sort_by == modified_oldest)
				{
					if (time == 0 || stats.st_mtime < time)
					{
						dstr_copy_dstr(&selected_path, &dir_path);
						time = stats.st_mtime;
					}
				}
			}
		}
		ent = os_readdir(dir);
	}
	if(file)
		bfree(file);
	dstr_free(&dir_path);
	os_closedir(dir);
	if (selected_path.array && selected_path.len) {
		FILE* f = os_fopen(selected_path.array, "rb+");
		if (!f) {
			dstr_free(&selected_path);
			return;
		}
		fclose(f);
		if (!context->file || strcmp(context->file, selected_path.array) != 0)
		{
			obs_source_t* parent = obs_filter_get_parent(context->source);
			if (parent)
			{
				obs_data_t* settings = obs_data_create();
				const char* id = obs_source_get_id(parent);
				if (strcmp(id, "ffmpeg_source") == 0)
				{
					obs_data_set_string(settings, "local_file", selected_path.array);
					obs_data_set_bool(settings, "is_local_file", true);
					obs_source_update(parent, settings);
					proc_handler_t* ph = obs_source_get_proc_handler(parent);
					if (ph) {
						calldata_t cd = { 0 };
						proc_handler_call(ph, "restart", &cd);
						calldata_free(&cd);
					}
				}
				else if (strcmp(id, "vlc_source") == 0)
				{
					obs_data_array_t* array = obs_data_array_create();
					obs_data_t* item = obs_data_create();
					obs_data_set_string(item, "value", context->file);
					obs_data_array_push_back(array, item);
					obs_data_set_array(settings, "playlist", array);
					obs_source_update(parent, settings);
					obs_data_release(item);
					obs_data_array_release(array);
				}
			}
			if (!context->file || !selected_path.array || strcmp(context->file, selected_path.array) != 0) {
				if (context->file)
					bfree(context->file);
				context->file = bstrdup(selected_path.array);
			}
		}
	}
	dstr_free(&selected_path);
}
static void dir_watch_media_source_filter_remove(void* data, obs_source_t* parent)
{
	
}

struct obs_source_info dir_watch_media_info = {
	.id = "dir_watch_media",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags   = OBS_SOURCE_VIDEO,
	.get_name       = dir_watch_media_source_get_name,
	.create         = dir_watch_media_source_create,
	.destroy        = dir_watch_media_source_destroy,
	.update         = dir_watch_media_source_update,
	.get_defaults   = dir_watch_media_source_defaults,
	.video_render   = dir_watch_media_source_render,
	.video_tick     = dir_watch_media_source_tick,
	.get_properties = dir_watch_media_source_properties,
	.filter_remove  = dir_watch_media_source_filter_remove,
};

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("dir-watch-media", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "Dir watch media";
}

bool obs_module_load(void)
{
	obs_register_source(&dir_watch_media_info);
	return true;
}
