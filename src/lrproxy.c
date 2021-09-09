/*
MIT License

Copyright (c) 2021 Andre Leiradella

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#define PROXY_FOR "/home/leiradel/.hackable-console/cores/dosbox_pure_libretro.so"

#include "libretro.h"
#include "dynlib.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static dynlib_t s_handle = NULL;

static void (*s_init)(void);
static void (*s_deinit)(void);
static unsigned (*s_api_version)(void);
static void (*s_get_system_info)(struct retro_system_info*);
static void (*s_get_system_av_info)(struct retro_system_av_info*);
static void (*s_set_environment)(retro_environment_t);
static void (*s_set_video_refresh)(retro_video_refresh_t);
static void (*s_set_audio_sample)(retro_audio_sample_t);
static void (*s_set_audio_sample_batch)(retro_audio_sample_batch_t);
static void (*s_set_input_poll)(retro_input_poll_t);
static void (*s_set_input_state)(retro_input_state_t);
static void (*s_set_controller_port_device)(unsigned, unsigned);
static void (*s_reset)(void);
static void (*s_run)(void);
static size_t (*s_serialize_size)(void);
static bool (*s_serialize)(void*, size_t);
static bool (*s_unserialize)(void const*, size_t);
static void (*s_cheat_reset)(void);
static void (*s_cheat_set)(unsigned, bool, char const*);
static bool (*s_load_game)(struct retro_game_info const*);
static bool (*s_load_game_special)(unsigned, struct retro_game_info const*, size_t);
static void (*s_unload_game)(void);
static unsigned (*s_getRegion)(void);
static void* (*s_get_memory_data)(unsigned);
static size_t (*s_get_memory_size)(unsigned);

#define CORE_DLSYM(prop, name) \
    do { \
        fprintf(stderr, "APILOG Getting pointer to %s\n", name); \
        void* sym = dynlib_symbol(s_handle, name); \
        if (!sym) goto error; \
        memcpy(&prop, &sym, sizeof(prop)); \
    } while (0)

static void init(void) {
    if (s_handle != NULL) {
        return;
    }

    fprintf(stderr, "APILOG Loading core \"%s\"\n", PROXY_FOR);

    s_handle = dynlib_open(PROXY_FOR);

    if (s_handle == NULL) {
        fprintf(stderr, "APILOG Error loading core: %s\n", dynlib_error());
        return;
    }

    CORE_DLSYM(s_init, "retro_init");
    CORE_DLSYM(s_deinit, "retro_deinit");
    CORE_DLSYM(s_api_version, "retro_api_version");
    CORE_DLSYM(s_get_system_info, "retro_get_system_info");
    CORE_DLSYM(s_get_system_av_info, "retro_get_system_av_info");
    CORE_DLSYM(s_set_environment, "retro_set_environment");
    CORE_DLSYM(s_set_video_refresh, "retro_set_video_refresh");
    CORE_DLSYM(s_set_audio_sample, "retro_set_audio_sample");
    CORE_DLSYM(s_set_audio_sample_batch, "retro_set_audio_sample_batch");
    CORE_DLSYM(s_set_input_poll, "retro_set_input_poll");
    CORE_DLSYM(s_set_input_state, "retro_set_input_state");
    CORE_DLSYM(s_set_controller_port_device, "retro_set_controller_port_device");
    CORE_DLSYM(s_reset, "retro_reset");
    CORE_DLSYM(s_run, "retro_run");
    CORE_DLSYM(s_serialize_size, "retro_serialize_size");
    CORE_DLSYM(s_serialize, "retro_serialize");
    CORE_DLSYM(s_unserialize, "retro_unserialize");
    CORE_DLSYM(s_cheat_reset, "retro_cheat_reset");
    CORE_DLSYM(s_cheat_set, "retro_cheat_set");
    CORE_DLSYM(s_load_game, "retro_load_game");
    CORE_DLSYM(s_load_game_special, "retro_load_game_special");
    CORE_DLSYM(s_unload_game, "retro_unload_game");
    CORE_DLSYM(s_getRegion, "retro_get_region");
    CORE_DLSYM(s_get_memory_data, "retro_get_memory_data");
    CORE_DLSYM(s_get_memory_size, "retro_get_memory_size");

    return;

error:
    fprintf(stderr, "APILOG Couldn't find symbol: %s\n", dynlib_error());
    dynlib_close(s_handle);
    s_handle = NULL;
}

#undef CORE_DLSYM

void retro_init(void) {
    init();

    s_init();
    fprintf(stderr, "APILOG retro_init()\n");
}

void retro_deinit(void) {
    init();

    s_deinit();
    fprintf(stderr, "APILOG retro_deinit()\n");

    dynlib_close(s_handle);
    s_handle = NULL;
}

unsigned retro_api_version(void) {
    init();

    unsigned const result = s_api_version();
    fprintf(stderr, "APILOG retro_api_version() = %u\n", result);

    return result;
}

void retro_get_system_info(struct retro_system_info* info) {
    init();

    s_get_system_info(info);
    fprintf(stderr, "APILOG retro_get_system_info(%p)\n", info);
}

void retro_get_system_av_info(struct retro_system_av_info* info) {
    init();

    s_get_system_av_info(info);
    fprintf(stderr, "APILOG retro_get_system_av_info(%p)\n", info);
}

void retro_set_environment(retro_environment_t cb) {
    init();

    s_set_environment(cb);
    fprintf(stderr, "APILOG retro_set_environment(%p)\n", cb);
}

void retro_set_video_refresh(retro_video_refresh_t cb) {
    init();

    s_set_video_refresh(cb);
    fprintf(stderr, "APILOG retro_set_video_refresh(%p)\n", cb);
}

void retro_set_audio_sample(retro_audio_sample_t cb) {
    init();

    s_set_audio_sample(cb);
    fprintf(stderr, "APILOG retro_set_audio_sample(%p)\n", cb);
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) {
    init();

    s_set_audio_sample_batch(cb);
    fprintf(stderr, "APILOG retro_set_audio_sample_batch(%p)\n", cb);
}

void retro_set_input_poll(retro_input_poll_t cb) {
    init();

    s_set_input_poll(cb);
    fprintf(stderr, "APILOG retro_set_input_poll(%p)\n", cb);
}

void retro_set_input_state(retro_input_state_t cb) {
    init();

    s_set_input_state(cb);
    fprintf(stderr, "APILOG retro_set_input_state(%p)\n", cb);
}

void retro_set_controller_port_device(unsigned port, unsigned device) {
    init();

    s_set_controller_port_device(port, device);
    fprintf(stderr, "APILOG retro_set_controller_port_device(%u, %u)\n", port, device);
}

void retro_reset(void) {
    init();

    s_reset();
    fprintf(stderr, "APILOG retro_reset()\n");
}

void retro_run(void) {
    init();

    s_run();
    fprintf(stderr, "APILOG retro_run()\n");
}

size_t retro_serialize_size(void) {
    init();

    size_t const result = s_serialize_size();
    fprintf(stderr, "APILOG retro_serialize_size() = %zu\n", result);

    return result;
}

bool retro_serialize(void* data, size_t size) {
    init();

    bool const result = s_serialize(data, size);
    fprintf(stderr, "APILOG retro_serialize(%p, %zu) = %d\n", data, size, result);

    return result;
}

bool retro_unserialize(void const* data, size_t size) {
    init();

    bool const result = s_unserialize(data, size);
    fprintf(stderr, "APILOG retro_unserialize(%p, %zu) = %d\n", data, size, result);

    return result;
}

void retro_cheat_reset(void) {
    init();

    s_cheat_reset();
    fprintf(stderr, "APILOG retro_cheat_reset()\n");
}

void retro_cheat_set(unsigned index, bool enabled, char const* code) {
    init();

    s_cheat_set(index, enabled, code);
    fprintf(stderr, "APILOG retro_cheat_set(%u, %d, \"%s\")\n", index, enabled, code);
}

bool retro_load_game(struct retro_game_info const* game) {
    init();

    bool const result = s_load_game(game);
    fprintf(stderr, "APILOG retro_load_game(%p) = %d\n", game, result);

    return result;
}

bool retro_load_game_special(unsigned game_type, struct retro_game_info const* info, size_t num_info) {
    init();

    bool const result = s_load_game_special(game_type, info, num_info);
    fprintf(stderr, "APILOG retro_load_game_special(%u, %p, %zu) = %d\n", game_type, info, num_info, result);

    return result;
}

void retro_unload_game(void) {
    init();

    s_unload_game();
    fprintf(stderr, "APILOG retro_unload_game()\n");
}

unsigned retro_get_region(void) {
    init();

    unsigned const result = s_getRegion();
    fprintf(stderr, "APILOG retro_get_region() = %u\n", result);

    return result;
}

void* retro_get_memory_data(unsigned id) {
    init();

    void* const result = s_get_memory_data(id);
    fprintf(stderr, "APILOG retro_get_memory_data(%u) = %p\n", id, result);

    return result;
}

size_t retro_get_memory_size(unsigned id) {
    init();

    size_t const result = s_get_memory_size(id);
    fprintf(stderr, "APILOG retro_get_memory_size(%u) = %zu\n", id, result);

    return result;
}
