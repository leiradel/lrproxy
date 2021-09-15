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

#include "libretro.h"
#include "dynlib.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define XSTR(s) STR(s)
#define STR(s) #s
#define TAG "[LRPROXY] "

static dynlib_t s_handle = NULL;
static retro_environment_t s_env = NULL;

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

#ifdef QUIET
    #define CORE_DLSYM(prop, name) \
        do { \
            void* sym = dynlib_symbol(s_handle, name); \
            if (!sym) goto error; \
            memcpy(&prop, &sym, sizeof(prop)); \
        } while (0)
#else
    #define CORE_DLSYM(prop, name) \
        do { \
            fprintf(stderr, TAG "Getting pointer to %s\n", name); \
            void* sym = dynlib_symbol(s_handle, name); \
            if (!sym) goto error; \
            memcpy(&prop, &sym, sizeof(prop)); \
        } while (0)
#endif

static void init(void) {
    if (s_handle != NULL) {
        return;
    }

    fprintf(stderr, TAG "Loading core \"%s\"\n", XSTR(PROXY_FOR));

    s_handle = dynlib_open(XSTR(PROXY_FOR));

    if (s_handle == NULL) {
        fprintf(stderr, TAG "Error loading core: %s\n", dynlib_error());
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
    fprintf(stderr, TAG "Couldn't find symbol: %s\n", dynlib_error());
    dynlib_close(s_handle);
    s_handle = NULL;
}

#undef CORE_DLSYM

static char const* pixel_format_str(enum retro_pixel_format const format) {
    static char unknown[32];

    switch (format) {
        case RETRO_PIXEL_FORMAT_0RGB1555: return "RETRO_PIXEL_FORMAT_0RGB1555";
        case RETRO_PIXEL_FORMAT_XRGB8888: return "RETRO_PIXEL_FORMAT_XRGB8888";
        case RETRO_PIXEL_FORMAT_RGB565: return "RETRO_PIXEL_FORMAT_RGB565";
        case RETRO_PIXEL_FORMAT_UNKNOWN: return "RETRO_PIXEL_FORMAT_UNKNOWN";

        default:
            snprintf(unknown, sizeof(unknown), "%d", format);
            return unknown;
    }
}

static char const* device_str(unsigned const device) {
    static char unknown[32];

    switch (device & RETRO_DEVICE_MASK) {
        case RETRO_DEVICE_NONE: return "RETRO_DEVICE_NONE";
        case RETRO_DEVICE_JOYPAD: return "RETRO_DEVICE_JOYPAD";
        case RETRO_DEVICE_MOUSE: return "RETRO_DEVICE_MOUSE";
        case RETRO_DEVICE_KEYBOARD: return "RETRO_DEVICE_KEYBOARD";
        case RETRO_DEVICE_LIGHTGUN: return "RETRO_DEVICE_LIGHTGUN";
        case RETRO_DEVICE_ANALOG: return "RETRO_DEVICE_ANALOG";
        case RETRO_DEVICE_POINTER: return "RETRO_DEVICE_POINTER";

        default:
            snprintf(unknown, sizeof(unknown), "%u", device & RETRO_DEVICE_MASK);
            return unknown;
    }
}

static char const* device_index_str(unsigned const device, unsigned const index) {
    static char unknown[32];

    if (device == RETRO_DEVICE_ANALOG) {
        switch (index) {
            case RETRO_DEVICE_INDEX_ANALOG_LEFT: return "RETRO_DEVICE_INDEX_ANALOG_LEFT";
            case RETRO_DEVICE_INDEX_ANALOG_RIGHT: return "RETRO_DEVICE_INDEX_ANALOG_RIGHT";
            case RETRO_DEVICE_INDEX_ANALOG_BUTTON: return "RETRO_DEVICE_INDEX_ANALOG_BUTTON";
        }
    }

    snprintf(unknown, sizeof(unknown), "%u", index);
    return unknown;
}

static char const* device_id_str(unsigned const device, unsigned const id) {
    static char unknown[32];

    switch (device & RETRO_DEVICE_MASK) {
        case RETRO_DEVICE_JOYPAD:
            switch (id) {
                case RETRO_DEVICE_ID_JOYPAD_B: return "RETRO_DEVICE_ID_JOYPAD_B";
                case RETRO_DEVICE_ID_JOYPAD_Y: return "RETRO_DEVICE_ID_JOYPAD_Y";
                case RETRO_DEVICE_ID_JOYPAD_SELECT: return "RETRO_DEVICE_ID_JOYPAD_SELECT";
                case RETRO_DEVICE_ID_JOYPAD_START: return "RETRO_DEVICE_ID_JOYPAD_START";
                case RETRO_DEVICE_ID_JOYPAD_UP: return "RETRO_DEVICE_ID_JOYPAD_UP";
                case RETRO_DEVICE_ID_JOYPAD_DOWN: return "RETRO_DEVICE_ID_JOYPAD_DOWN";
                case RETRO_DEVICE_ID_JOYPAD_LEFT: return "RETRO_DEVICE_ID_JOYPAD_LEFT";
                case RETRO_DEVICE_ID_JOYPAD_RIGHT: return "RETRO_DEVICE_ID_JOYPAD_RIGHT";
                case RETRO_DEVICE_ID_JOYPAD_A: return "RETRO_DEVICE_ID_JOYPAD_A";
                case RETRO_DEVICE_ID_JOYPAD_X: return "RETRO_DEVICE_ID_JOYPAD_X";
                case RETRO_DEVICE_ID_JOYPAD_L: return "RETRO_DEVICE_ID_JOYPAD_L";
                case RETRO_DEVICE_ID_JOYPAD_R: return "RETRO_DEVICE_ID_JOYPAD_R";
                case RETRO_DEVICE_ID_JOYPAD_L2: return "RETRO_DEVICE_ID_JOYPAD_L2";
                case RETRO_DEVICE_ID_JOYPAD_R2: return "RETRO_DEVICE_ID_JOYPAD_R2";
                case RETRO_DEVICE_ID_JOYPAD_L3: return "RETRO_DEVICE_ID_JOYPAD_L3";
                case RETRO_DEVICE_ID_JOYPAD_R3: return "RETRO_DEVICE_ID_JOYPAD_R3";
            }

            break;

        case RETRO_DEVICE_MOUSE:
            switch (id) {
                case RETRO_DEVICE_ID_MOUSE_X: return "RETRO_DEVICE_ID_MOUSE_X";
                case RETRO_DEVICE_ID_MOUSE_Y: return "RETRO_DEVICE_ID_MOUSE_Y";
                case RETRO_DEVICE_ID_MOUSE_LEFT: return "RETRO_DEVICE_ID_MOUSE_LEFT";
                case RETRO_DEVICE_ID_MOUSE_RIGHT: return "RETRO_DEVICE_ID_MOUSE_RIGHT";
                case RETRO_DEVICE_ID_MOUSE_WHEELUP: return "RETRO_DEVICE_ID_MOUSE_WHEELUP";
                case RETRO_DEVICE_ID_MOUSE_WHEELDOWN: return "RETRO_DEVICE_ID_MOUSE_WHEELDOWN";
                case RETRO_DEVICE_ID_MOUSE_MIDDLE: return "RETRO_DEVICE_ID_MOUSE_MIDDLE";
                case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP: return "RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP";
                case RETRO_DEVICE_ID_MOUSE_BUTTON_4: return "RETRO_DEVICE_ID_MOUSE_BUTTON_4";
                case RETRO_DEVICE_ID_MOUSE_BUTTON_5: return "RETRO_DEVICE_ID_MOUSE_BUTTON_5";
            }

            break;

        case RETRO_DEVICE_LIGHTGUN:
            switch (id) {
                case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X: return "RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X";
                case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y: return "RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y";
                case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN: return "RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN";
                case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER: return "RETRO_DEVICE_ID_LIGHTGUN_TRIGGER";
                case RETRO_DEVICE_ID_LIGHTGUN_RELOAD: return "RETRO_DEVICE_ID_LIGHTGUN_RELOAD";
                case RETRO_DEVICE_ID_LIGHTGUN_AUX_A: return "RETRO_DEVICE_ID_LIGHTGUN_AUX_A";
                case RETRO_DEVICE_ID_LIGHTGUN_AUX_B: return "RETRO_DEVICE_ID_LIGHTGUN_AUX_B";
                case RETRO_DEVICE_ID_LIGHTGUN_START: return "RETRO_DEVICE_ID_LIGHTGUN_START";
                case RETRO_DEVICE_ID_LIGHTGUN_SELECT: return "RETRO_DEVICE_ID_LIGHTGUN_SELECT";
                case RETRO_DEVICE_ID_LIGHTGUN_AUX_C: return "RETRO_DEVICE_ID_LIGHTGUN_AUX_C";
                case RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP: return "RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP";
                case RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN: return "RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN";
                case RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT: return "RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT";
                case RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT: return "RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT";
                case RETRO_DEVICE_ID_LIGHTGUN_X: return "RETRO_DEVICE_ID_LIGHTGUN_X";
                case RETRO_DEVICE_ID_LIGHTGUN_Y: return "RETRO_DEVICE_ID_LIGHTGUN_Y";
                case RETRO_DEVICE_ID_LIGHTGUN_PAUSE: return "RETRO_DEVICE_ID_LIGHTGUN_PAUSE";
            }

            break;

        case RETRO_DEVICE_ANALOG:
            switch (id) {
                case RETRO_DEVICE_ID_ANALOG_X: return "RETRO_DEVICE_ID_ANALOG_X";
                case RETRO_DEVICE_ID_ANALOG_Y: return "RETRO_DEVICE_ID_ANALOG_Y";
            }

            break;

        case RETRO_DEVICE_POINTER:
            switch (id) {
                case RETRO_DEVICE_ID_POINTER_X: return "RETRO_DEVICE_ID_POINTER_X";
                case RETRO_DEVICE_ID_POINTER_Y: return "RETRO_DEVICE_ID_POINTER_Y";
                case RETRO_DEVICE_ID_POINTER_PRESSED: return "RETRO_DEVICE_ID_POINTER_PRESSED";
                case RETRO_DEVICE_ID_POINTER_COUNT: return "RETRO_DEVICE_ID_POINTER_COUNT";
            }

            break;
    }

    snprintf(unknown, sizeof(unknown), "%u", id);
    return unknown;
}

static bool environment(unsigned cmd, void* data) {
    bool const result = s_env(cmd, data);

    switch (cmd) {
        case RETRO_ENVIRONMENT_SET_ROTATION:
            fprintf(stderr, TAG "RETRO_ENVIRONMENT_SET_ROTATION(%u) = %d\n", *(unsigned const*)data, result);
            break;

        case RETRO_ENVIRONMENT_GET_OVERSCAN:
            fprintf(stderr, TAG "RETRO_ENVIRONMENT_GET_OVERSCAN() = %d, %d\n", *(bool*)data, result);
            break;

        case RETRO_ENVIRONMENT_GET_CAN_DUPE:
            fprintf(stderr, TAG "RETRO_ENVIRONMENT_GET_CAN_DUPE() = %d, %d\n", *(bool*)data, result);
            break;

        case RETRO_ENVIRONMENT_SET_MESSAGE: {
            fprintf(stderr, TAG "RETRO_ENVIRONMENT_SET_MESSAGE(%p) = %d\n", data, result);

#ifndef QUIET
            struct retro_message const* const message = (struct retro_message const*)data;
            fprintf(stderr, TAG "    ->msg    = \"%s\"\n", message->msg);
            fprintf(stderr, TAG "    ->frames = %u\n", message->frames);
#endif

            break;
        }

        case RETRO_ENVIRONMENT_SHUTDOWN:
            fprintf(stderr, TAG "RETRO_ENVIRONMENT_SHUTDOWN() = %d\n", result);
            break;

        case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
            fprintf(stderr, TAG "RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL(%u) = %d\n", *(unsigned const*)data, result);
            break;

        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
            fprintf(stderr, TAG "RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY() = \"%s\", %d\n", *(char const**)data, result);
            break;

        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
            enum retro_pixel_format const format = *(enum retro_pixel_format const*)data;
            fprintf(stderr, TAG "RETRO_ENVIRONMENT_SET_PIXEL_FORMAT(%s) = %d\n", pixel_format_str(format), result);
            break;
        }

        case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS: {
            fprintf(stderr, TAG "RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS(%p) = %d\n", data, result);

#ifndef QUIET
            struct retro_input_descriptor const* desc = (struct retro_input_descriptor const*)data;
            unsigned i = 0;

            for (; desc->description != NULL; desc++, i++) {
                fprintf(stderr, TAG "    [%u].port        = %u\n", i, desc->port);

                fprintf(
                    stderr, TAG "    [%u].device      = %u << RETRO_DEVICE_TYPE_SHIFT | %s\n",
                    i, desc->device >> RETRO_DEVICE_TYPE_SHIFT, device_str(desc->device)
                );

                fprintf(stderr, TAG "    [%u].index       = %s\n", i, device_index_str(desc->device, desc->index));
                fprintf(stderr, TAG "    [%u].id          = %s\n", i, device_id_str(desc->device, desc->id));
                fprintf(stderr, TAG "    [%u].description = \"%s\"\n", i, desc->description);
            }
#endif

            break;
        }
        case RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK:
                                           /* const struct retro_keyboard_callback * --
                                            * Sets a callback function used to notify core about keyboard events.
                                            */
        case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE:
                                           /* const struct retro_disk_control_callback * --
                                            * Sets an interface which frontend can use to eject and insert
                                            * disk images.
                                            * This is used for games which consist of multiple images and
                                            * must be manually swapped out by the user (e.g. PSX).
                                            */
        case RETRO_ENVIRONMENT_SET_HW_RENDER:
                                           /* struct retro_hw_render_callback * --
                                            * Sets an interface to let a libretro core render with
                                            * hardware acceleration.
                                            * Should be called in retro_load_game().
                                            * If successful, libretro cores will be able to render to a
                                            * frontend-provided framebuffer.
                                            * The size of this framebuffer will be at least as large as
                                            * max_width/max_height provided in get_av_info().
                                            * If HW rendering is used, pass only RETRO_HW_FRAME_BUFFER_VALID or
                                            * NULL to retro_video_refresh_t.
                                            */
        case RETRO_ENVIRONMENT_GET_VARIABLE:
                                           /* struct retro_variable * --
                                            * Interface to acquire user-defined information from environment
                                            * that cannot feasibly be supported in a multi-system way.
                                            * 'key' should be set to a key which has already been set by
                                            * SET_VARIABLES.
                                            * 'data' will be set to a value or NULL.
                                            */
        case RETRO_ENVIRONMENT_SET_VARIABLES:
                                           /* const struct retro_variable * --
                                            * Allows an implementation to signal the environment
                                            * which variables it might want to check for later using
                                            * GET_VARIABLE.
                                            * This allows the frontend to present these variables to
                                            * a user dynamically.
                                            * This should be called the first time as early as
                                            * possible (ideally in retro_set_environment).
                                            * Afterward it may be called again for the core to communicate
                                            * updated options to the frontend, but the number of core
                                            * options must not change from the number in the initial call.
                                            *
                                            * 'data' points to an array of retro_variable structs
                                            * terminated by a { NULL, NULL } element.
                                            * retro_variable::key should be namespaced to not collide
                                            * with other implementations' keys. E.g. A core called
                                            * 'foo' should use keys named as 'foo_option'.
                                            * retro_variable::value should contain a human readable
                                            * description of the key as well as a '|' delimited list
                                            * of expected values.
                                            *
                                            * The number of possible options should be very limited,
                                            * i.e. it should be feasible to cycle through options
                                            * without a keyboard.
                                            *
                                            * First entry should be treated as a default.
                                            *
                                            * Example entry:
                                            * { "foo_option", "Speed hack coprocessor X; false|true" }
                                            *
                                            * Text before first ';' is description. This ';' must be
                                            * followed by a space, and followed by a list of possible
                                            * values split up with '|'.
                                            *
                                            * Only strings are operated on. The possible values will
                                            * generally be displayed and stored as-is by the frontend.
                                            */
        case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
                                           /* bool * --
                                            * Result is set to true if some variables are updated by
                                            * frontend since last call to RETRO_ENVIRONMENT_GET_VARIABLE.
                                            * Variables should be queried with GET_VARIABLE.
                                            */
        case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
                                           /* const bool * --
                                            * If true, the libretro implementation supports calls to
                                            * retro_load_game() with NULL as argument.
                                            * Used by cores which can run without particular game data.
                                            * This should be called within retro_set_environment() only.
                                            */
        case RETRO_ENVIRONMENT_GET_LIBRETRO_PATH:
                                           /* const char ** --
                                            * Retrieves the absolute path from where this libretro
                                            * implementation was loaded.
                                            * NULL is returned if the libretro was loaded statically
                                            * (i.e. linked statically to frontend), or if the path cannot be
                                            * determined.
                                            * Mostly useful in cooperation with SET_SUPPORT_NO_GAME as assets can
                                            * be loaded without ugly hacks.
                                            */

                                           /* Environment 20 was an obsolete version of SET_AUDIO_CALLBACK.
                                            * It was not used by any known core at the time,
                                            * and was removed from the API. */
        case RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK:
                                           /* const struct retro_frame_time_callback * --
                                            * Lets the core know how much time has passed since last
                                            * invocation of retro_run().
                                            * The frontend can tamper with the timing to fake fast-forward,
                                            * slow-motion, frame stepping, etc.
                                            * In this case the delta time will use the reference value
                                            * in frame_time_callback..
                                            */
        case RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK:
                                           /* const struct retro_audio_callback * --
                                            * Sets an interface which is used to notify a libretro core about audio
                                            * being available for writing.
                                            * The callback can be called from any thread, so a core using this must
                                            * have a thread safe audio implementation.
                                            * It is intended for games where audio and video are completely
                                            * asynchronous and audio can be generated on the fly.
                                            * This interface is not recommended for use with emulators which have
                                            * highly synchronous audio.
                                            *
                                            * The callback only notifies about writability; the libretro core still
                                            * has to call the normal audio callbacks
                                            * to write audio. The audio callbacks must be called from within the
                                            * notification callback.
                                            * The amount of audio data to write is up to the implementation.
                                            * Generally, the audio callback will be called continously in a loop.
                                            *
                                            * Due to thread safety guarantees and lack of sync between audio and
                                            * video, a frontend  can selectively disallow this interface based on
                                            * internal configuration. A core using this interface must also
                                            * implement the "normal" audio interface.
                                            *
                                            * A libretro core using SET_AUDIO_CALLBACK should also make use of
                                            * SET_FRAME_TIME_CALLBACK.
                                            */
        case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE:
                                           /* struct retro_rumble_interface * --
                                            * Gets an interface which is used by a libretro core to set
                                            * state of rumble motors in controllers.
                                            * A strong and weak motor is supported, and they can be
                                            * controlled indepedently.
                                            */
        case RETRO_ENVIRONMENT_GET_INPUT_DEVICE_CAPABILITIES:
                                           /* uint64_t * --
                                            * Gets a bitmask telling which device type are expected to be
                                            * handled properly in a call to retro_input_state_t.
                                            * Devices which are not handled or recognized always return
                                            * 0 in retro_input_state_t.
                                            * Example bitmask: caps = (1 << RETRO_DEVICE_JOYPAD) | (1 << RETRO_DEVICE_ANALOG).
                                            * Should only be called in retro_run().
                                            */
        case RETRO_ENVIRONMENT_GET_SENSOR_INTERFACE:
                                           /* struct retro_sensor_interface * --
                                            * Gets access to the sensor interface.
                                            * The purpose of this interface is to allow
                                            * setting state related to sensors such as polling rate,
                                            * enabling/disable it entirely, etc.
                                            * Reading sensor state is done via the normal
                                            * input_state_callback API.
                                            */
        case RETRO_ENVIRONMENT_GET_CAMERA_INTERFACE:
                                           /* struct retro_camera_callback * --
                                            * Gets an interface to a video camera driver.
                                            * A libretro core can use this interface to get access to a
                                            * video camera.
                                            * New video frames are delivered in a callback in same
                                            * thread as retro_run().
                                            *
                                            * GET_CAMERA_INTERFACE should be called in retro_load_game().
                                            *
                                            * Depending on the camera implementation used, camera frames
                                            * will be delivered as a raw framebuffer,
                                            * or as an OpenGL texture directly.
                                            *
                                            * The core has to tell the frontend here which types of
                                            * buffers can be handled properly.
                                            * An OpenGL texture can only be handled when using a
                                            * libretro GL core (SET_HW_RENDER).
                                            * It is recommended to use a libretro GL core when
                                            * using camera interface.
                                            *
                                            * The camera is not started automatically. The retrieved start/stop
                                            * functions must be used to explicitly
                                            * start and stop the camera driver.
                                            */
        case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
                                           /* struct retro_log_callback * --
                                            * Gets an interface for logging. This is useful for
                                            * logging in a cross-platform way
                                            * as certain platforms cannot use stderr for logging.
                                            * It also allows the frontend to
                                            * show logging information in a more suitable way.
                                            * If this interface is not used, libretro cores should
                                            * log to stderr as desired.
                                            */
        case RETRO_ENVIRONMENT_GET_PERF_INTERFACE:
                                           /* struct retro_perf_callback * --
                                            * Gets an interface for performance counters. This is useful
                                            * for performance logging in a cross-platform way and for detecting
                                            * architecture-specific features, such as SIMD support.
                                            */
        case RETRO_ENVIRONMENT_GET_LOCATION_INTERFACE:
                                           /* struct retro_location_callback * --
                                            * Gets access to the location interface.
                                            * The purpose of this interface is to be able to retrieve
                                            * location-based information from the host device,
                                            * such as current latitude / longitude.
                                            */
        /*case RETRO_ENVIRONMENT_GET_CONTENT_DIRECTORY: /* Old name, kept for compatibility. */
        case RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY:
                                           /* const char ** --
                                            * Returns the "core assets" directory of the frontend.
                                            * This directory can be used to store specific assets that the
                                            * core relies upon, such as art assets,
                                            * input data, etc etc.
                                            * The returned value can be NULL.
                                            * If so, no such directory is defined,
                                            * and it's up to the implementation to find a suitable directory.
                                            */
        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
                                           /* const char ** --
                                            * Returns the "save" directory of the frontend, unless there is no
                                            * save directory available. The save directory should be used to
                                            * store SRAM, memory cards, high scores, etc, if the libretro core
                                            * cannot use the regular memory interface (retro_get_memory_data()).
                                            *
                                            * If the frontend cannot designate a save directory, it will return
                                            * NULL to indicate that the core should attempt to operate without a
                                            * save directory set.
                                            *
                                            * NOTE: early libretro cores used the system directory for save
                                            * files. Cores that need to be backwards-compatible can still check
                                            * GET_SYSTEM_DIRECTORY.
                                            */
        case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
                                           /* const struct retro_system_av_info * --
                                            * Sets a new av_info structure. This can only be called from
                                            * within retro_run().
                                            * This should *only* be used if the core is completely altering the
                                            * internal resolutions, aspect ratios, timings, sampling rate, etc.
                                            * Calling this can require a full reinitialization of video/audio
                                            * drivers in the frontend,
                                            *
                                            * so it is important to call it very sparingly, and usually only with
                                            * the users explicit consent.
                                            * An eventual driver reinitialize will happen so that video and
                                            * audio callbacks
                                            * happening after this call within the same retro_run() call will
                                            * target the newly initialized driver.
                                            *
                                            * This callback makes it possible to support configurable resolutions
                                            * in games, which can be useful to
                                            * avoid setting the "worst case" in max_width/max_height.
                                            *
                                            * ***HIGHLY RECOMMENDED*** Do not call this callback every time
                                            * resolution changes in an emulator core if it's
                                            * expected to be a temporary change, for the reasons of possible
                                            * driver reinitialization.
                                            * This call is not a free pass for not trying to provide
                                            * correct values in retro_get_system_av_info(). If you need to change
                                            * things like aspect ratio or nominal width/height,
                                            * use RETRO_ENVIRONMENT_SET_GEOMETRY, which is a softer variant
                                            * of SET_SYSTEM_AV_INFO.
                                            *
                                            * If this returns false, the frontend does not acknowledge a
                                            * changed av_info struct.
                                            */
        case RETRO_ENVIRONMENT_SET_PROC_ADDRESS_CALLBACK:
                                           /* const struct retro_get_proc_address_interface * --
                                            * Allows a libretro core to announce support for the
                                            * get_proc_address() interface.
                                            * This interface allows for a standard way to extend libretro where
                                            * use of environment calls are too indirect,
                                            * e.g. for cases where the frontend wants to call directly into the core.
                                            *
                                            * If a core wants to expose this interface, SET_PROC_ADDRESS_CALLBACK
                                            * **MUST** be called from within retro_set_environment().
                                            */
        case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO:
                                           /* const struct retro_subsystem_info * --
                                            * This environment call introduces the concept of libretro "subsystems".
                                            * A subsystem is a variant of a libretro core which supports
                                            * different kinds of games.
                                            * The purpose of this is to support e.g. emulators which might
                                            * have special needs, e.g. Super Nintendo's Super GameBoy, Sufami Turbo.
                                            * It can also be used to pick among subsystems in an explicit way
                                            * if the libretro implementation is a multi-system emulator itself.
                                            *
                                            * Loading a game via a subsystem is done with retro_load_game_special(),
                                            * and this environment call allows a libretro core to expose which
                                            * subsystems are supported for use with retro_load_game_special().
                                            * A core passes an array of retro_game_special_info which is terminated
                                            * with a zeroed out retro_game_special_info struct.
                                            *
                                            * If a core wants to use this functionality, SET_SUBSYSTEM_INFO
                                            * **MUST** be called from within retro_set_environment().
                                            */
        case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
                                           /* const struct retro_controller_info * --
                                            * This environment call lets a libretro core tell the frontend
                                            * which controller subclasses are recognized in calls to
                                            * retro_set_controller_port_device().
                                            *
                                            * Some emulators such as Super Nintendo support multiple lightgun
                                            * types which must be specifically selected from. It is therefore
                                            * sometimes necessary for a frontend to be able to tell the core
                                            * about a special kind of input device which is not specifcally
                                            * provided by the Libretro API.
                                            *
                                            * In order for a frontend to understand the workings of those devices,
                                            * they must be defined as a specialized subclass of the generic device
                                            * types already defined in the libretro API.
                                            *
                                            * The core must pass an array of const struct retro_controller_info which
                                            * is terminated with a blanked out struct. Each element of the
                                            * retro_controller_info struct corresponds to the ascending port index
                                            * that is passed to retro_set_controller_port_device() when that function
                                            * is called to indicate to the core that the frontend has changed the
                                            * active device subclass. SEE ALSO: retro_set_controller_port_device()
                                            *
                                            * The ascending input port indexes provided by the core in the struct
                                            * are generally presented by frontends as ascending User # or Player #,
                                            * such as Player 1, Player 2, Player 3, etc. Which device subclasses are
                                            * supported can vary per input port.
                                            *
                                            * The first inner element of each entry in the retro_controller_info array
                                            * is a retro_controller_description struct that specifies the names and
                                            * codes of all device subclasses that are available for the corresponding
                                            * User or Player, beginning with the generic Libretro device that the
                                            * subclasses are derived from. The second inner element of each entry is the
                                            * total number of subclasses that are listed in the retro_controller_description.
                                            *
                                            * NOTE: Even if special device types are set in the libretro core,
                                            * libretro should only poll input based on the base input device types.
                                            */
        case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
                                           /* const struct retro_memory_map * --
                                            * This environment call lets a libretro core tell the frontend
                                            * about the memory maps this core emulates.
                                            * This can be used to implement, for example, cheats in a core-agnostic way.
                                            *
                                            * Should only be used by emulators; it doesn't make much sense for
                                            * anything else.
                                            * It is recommended to expose all relevant pointers through
                                            * retro_get_memory_* as well.
                                            *
                                            * Can be called from retro_init and retro_load_game.
                                            */
        case RETRO_ENVIRONMENT_SET_GEOMETRY:
                                           /* const struct retro_game_geometry * --
                                            * This environment call is similar to SET_SYSTEM_AV_INFO for changing
                                            * video parameters, but provides a guarantee that drivers will not be
                                            * reinitialized.
                                            * This can only be called from within retro_run().
                                            *
                                            * The purpose of this call is to allow a core to alter nominal
                                            * width/heights as well as aspect ratios on-the-fly, which can be
                                            * useful for some emulators to change in run-time.
                                            *
                                            * max_width/max_height arguments are ignored and cannot be changed
                                            * with this call as this could potentially require a reinitialization or a
                                            * non-constant time operation.
                                            * If max_width/max_height are to be changed, SET_SYSTEM_AV_INFO is required.
                                            *
                                            * A frontend must guarantee that this environment call completes in
                                            * constant time.
                                            */
        case RETRO_ENVIRONMENT_GET_USERNAME:
                                           /* const char **
                                            * Returns the specified username of the frontend, if specified by the user.
                                            * This username can be used as a nickname for a core that has online facilities
                                            * or any other mode where personalization of the user is desirable.
                                            * The returned value can be NULL.
                                            * If this environ callback is used by a core that requires a valid username,
                                            * a default username should be specified by the core.
                                            */
        case RETRO_ENVIRONMENT_GET_LANGUAGE:
                                           /* unsigned * --
                                            * Returns the specified language of the frontend, if specified by the user.
                                            * It can be used by the core for localization purposes.
                                            */
        case RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER:
                                           /* struct retro_framebuffer * --
                                            * Returns a preallocated framebuffer which the core can use for rendering
                                            * the frame into when not using SET_HW_RENDER.
                                            * The framebuffer returned from this call must not be used
                                            * after the current call to retro_run() returns.
                                            *
                                            * The goal of this call is to allow zero-copy behavior where a core
                                            * can render directly into video memory, avoiding extra bandwidth cost by copying
                                            * memory from core to video memory.
                                            *
                                            * If this call succeeds and the core renders into it,
                                            * the framebuffer pointer and pitch can be passed to retro_video_refresh_t.
                                            * If the buffer from GET_CURRENT_SOFTWARE_FRAMEBUFFER is to be used,
                                            * the core must pass the exact
                                            * same pointer as returned by GET_CURRENT_SOFTWARE_FRAMEBUFFER;
                                            * i.e. passing a pointer which is offset from the
                                            * buffer is undefined. The width, height and pitch parameters
                                            * must also match exactly to the values obtained from GET_CURRENT_SOFTWARE_FRAMEBUFFER.
                                            *
                                            * It is possible for a frontend to return a different pixel format
                                            * than the one used in SET_PIXEL_FORMAT. This can happen if the frontend
                                            * needs to perform conversion.
                                            *
                                            * It is still valid for a core to render to a different buffer
                                            * even if GET_CURRENT_SOFTWARE_FRAMEBUFFER succeeds.
                                            *
                                            * A frontend must make sure that the pointer obtained from this function is
                                            * writeable (and readable).
                                            */
        case RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE:
                                           /* const struct retro_hw_render_interface ** --
                                            * Returns an API specific rendering interface for accessing API specific data.
                                            * Not all HW rendering APIs support or need this.
                                            * The contents of the returned pointer is specific to the rendering API
                                            * being used. See the various headers like libretro_vulkan.h, etc.
                                            *
                                            * GET_HW_RENDER_INTERFACE cannot be called before context_reset has been called.
                                            * Similarly, after context_destroyed callback returns,
                                            * the contents of the HW_RENDER_INTERFACE are invalidated.
                                            */
        case RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS:
                                           /* const bool * --
                                            * If true, the libretro implementation supports achievements
                                            * either via memory descriptors set with RETRO_ENVIRONMENT_SET_MEMORY_MAPS
                                            * or via retro_get_memory_data/retro_get_memory_size.
                                            *
                                            * This must be called before the first call to retro_run.
                                            */
        case RETRO_ENVIRONMENT_SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE:
                                           /* const struct retro_hw_render_context_negotiation_interface * --
                                            * Sets an interface which lets the libretro core negotiate with frontend how a context is created.
                                            * The semantics of this interface depends on which API is used in SET_HW_RENDER earlier.
                                            * This interface will be used when the frontend is trying to create a HW rendering context,
                                            * so it will be used after SET_HW_RENDER, but before the context_reset callback.
                                            */
        case RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS:
                                           /* uint64_t * --
                                            * Sets quirk flags associated with serialization. The frontend will zero any flags it doesn't
                                            * recognize or support. Should be set in either retro_init or retro_load_game, but not both.
                                            */
        case RETRO_ENVIRONMENT_SET_HW_SHARED_CONTEXT:
                                           /* N/A (null) * --
                                            * The frontend will try to use a 'shared' hardware context (mostly applicable
                                            * to OpenGL) when a hardware context is being set up.
                                            *
                                            * Returns true if the frontend supports shared hardware contexts and false
                                            * if the frontend does not support shared hardware contexts.
                                            *
                                            * This will do nothing on its own until SET_HW_RENDER env callbacks are
                                            * being used.
                                            */
        case RETRO_ENVIRONMENT_GET_VFS_INTERFACE:
                                           /* struct retro_vfs_interface_info * --
                                            * Gets access to the VFS interface.
                                            * VFS presence needs to be queried prior to load_game or any
                                            * get_system/save/other_directory being called to let front end know
                                            * core supports VFS before it starts handing out paths.
                                            * It is recomended to do so in retro_set_environment
                                            */
        case RETRO_ENVIRONMENT_GET_LED_INTERFACE:
                                           /* struct retro_led_interface * --
                                            * Gets an interface which is used by a libretro core to set
                                            * state of LEDs.
                                            */
        case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE:
                                           /* int * --
                                            * Tells the core if the frontend wants audio or video.
                                            * If disabled, the frontend will discard the audio or video,
                                            * so the core may decide to skip generating a frame or generating audio.
                                            * This is mainly used for increasing performance.
                                            * Bit 0 (value 1): Enable Video
                                            * Bit 1 (value 2): Enable Audio
                                            * Bit 2 (value 4): Use Fast Savestates.
                                            * Bit 3 (value 8): Hard Disable Audio
                                            * Other bits are reserved for future use and will default to zero.
                                            * If video is disabled:
                                            * * The frontend wants the core to not generate any video,
                                            *   including presenting frames via hardware acceleration.
                                            * * The frontend's video frame callback will do nothing.
                                            * * After running the frame, the video output of the next frame should be
                                            *   no different than if video was enabled, and saving and loading state
                                            *   should have no issues.
                                            * If audio is disabled:
                                            * * The frontend wants the core to not generate any audio.
                                            * * The frontend's audio callbacks will do nothing.
                                            * * After running the frame, the audio output of the next frame should be
                                            *   no different than if audio was enabled, and saving and loading state
                                            *   should have no issues.
                                            * Fast Savestates:
                                            * * Guaranteed to be created by the same binary that will load them.
                                            * * Will not be written to or read from the disk.
                                            * * Suggest that the core assumes loading state will succeed.
                                            * * Suggest that the core updates its memory buffers in-place if possible.
                                            * * Suggest that the core skips clearing memory.
                                            * * Suggest that the core skips resetting the system.
                                            * * Suggest that the core may skip validation steps.
                                            * Hard Disable Audio:
                                            * * Used for a secondary core when running ahead.
                                            * * Indicates that the frontend will never need audio from the core.
                                            * * Suggests that the core may stop synthesizing audio, but this should not
                                            *   compromise emulation accuracy.
                                            * * Audio output for the next frame does not matter, and the frontend will
                                            *   never need an accurate audio state in the future.
                                            * * State will never be saved when using Hard Disable Audio.
                                            */
        case RETRO_ENVIRONMENT_GET_MIDI_INTERFACE:
                                           /* struct retro_midi_interface ** --
                                            * Returns a MIDI interface that can be used for raw data I/O.
                                            */

        case RETRO_ENVIRONMENT_GET_FASTFORWARDING:
                                            /* bool * --
                                            * Boolean value that indicates whether or not the frontend is in
                                            * fastforwarding mode.
                                            */

        case RETRO_ENVIRONMENT_GET_TARGET_REFRESH_RATE:
                                            /* float * --
                                            * Float value that lets us know what target refresh rate 
                                            * is curently in use by the frontend.
                                            *
                                            * The core can use the returned value to set an ideal 
                                            * refresh rate/framerate.
                                            */

        case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
                                            /* bool * --
                                            * Boolean value that indicates whether or not the frontend supports
                                            * input bitmasks being returned by retro_input_state_t. The advantage
                                            * of this is that retro_input_state_t has to be only called once to 
                                            * grab all button states instead of multiple times.
                                            *
                                            * If it returns true, you can pass RETRO_DEVICE_ID_JOYPAD_MASK as 'id'
                                            * to retro_input_state_t (make sure 'device' is set to RETRO_DEVICE_JOYPAD).
                                            * It will return a bitmask of all the digital buttons.
                                            */

        case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
                                           /* unsigned * --
                                            * Unsigned value is the API version number of the core options
                                            * interface supported by the frontend. If callback return false,
                                            * API version is assumed to be 0.
                                            *
                                            * In legacy code, core options are set by passing an array of
                                            * retro_variable structs to RETRO_ENVIRONMENT_SET_VARIABLES.
                                            * This may be still be done regardless of the core options
                                            * interface version.
                                            *
                                            * If version is >= 1 however, core options may instead be set by
                                            * passing an array of retro_core_option_definition structs to
                                            * RETRO_ENVIRONMENT_SET_CORE_OPTIONS, or a 2D array of
                                            * retro_core_option_definition structs to RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL.
                                            * This allows the core to additionally set option sublabel information
                                            * and/or provide localisation support.
                                            */

        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS:
                                           /* const struct retro_core_option_definition ** --
                                            * Allows an implementation to signal the environment
                                            * which variables it might want to check for later using
                                            * GET_VARIABLE.
                                            * This allows the frontend to present these variables to
                                            * a user dynamically.
                                            * This should only be called if RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION
                                            * returns an API version of >= 1.
                                            * This should be called instead of RETRO_ENVIRONMENT_SET_VARIABLES.
                                            * This should be called the first time as early as
                                            * possible (ideally in retro_set_environment).
                                            * Afterwards it may be called again for the core to communicate
                                            * updated options to the frontend, but the number of core
                                            * options must not change from the number in the initial call.
                                            *
                                            * 'data' points to an array of retro_core_option_definition structs
                                            * terminated by a { NULL, NULL, NULL, {{0}}, NULL } element.
                                            * retro_core_option_definition::key should be namespaced to not collide
                                            * with other implementations' keys. e.g. A core called
                                            * 'foo' should use keys named as 'foo_option'.
                                            * retro_core_option_definition::desc should contain a human readable
                                            * description of the key.
                                            * retro_core_option_definition::info should contain any additional human
                                            * readable information text that a typical user may need to
                                            * understand the functionality of the option.
                                            * retro_core_option_definition::values is an array of retro_core_option_value
                                            * structs terminated by a { NULL, NULL } element.
                                            * > retro_core_option_definition::values[index].value is an expected option
                                            *   value.
                                            * > retro_core_option_definition::values[index].label is a human readable
                                            *   label used when displaying the value on screen. If NULL,
                                            *   the value itself is used.
                                            * retro_core_option_definition::default_value is the default core option
                                            * setting. It must match one of the expected option values in the
                                            * retro_core_option_definition::values array. If it does not, or the
                                            * default value is NULL, the first entry in the
                                            * retro_core_option_definition::values array is treated as the default.
                                            *
                                            * The number of possible options should be very limited,
                                            * and must be less than RETRO_NUM_CORE_OPTION_VALUES_MAX.
                                            * i.e. it should be feasible to cycle through options
                                            * without a keyboard.
                                            *
                                            * Example entry:
                                            * {
                                            *     "foo_option",
                                            *     "Speed hack coprocessor X",
                                            *     "Provides increased performance at the expense of reduced accuracy",
                                            *     {
                                            *         { "false",    NULL },
                                            *         { "true",     NULL },
                                            *         { "unstable", "Turbo (Unstable)" },
                                            *         { NULL, NULL },
                                            *     },
                                            *     "false"
                                            * }
                                            *
                                            * Only strings are operated on. The possible values will
                                            * generally be displayed and stored as-is by the frontend.
                                            */

        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL:
                                           /* const struct retro_core_options_intl * --
                                            * Allows an implementation to signal the environment
                                            * which variables it might want to check for later using
                                            * GET_VARIABLE.
                                            * This allows the frontend to present these variables to
                                            * a user dynamically.
                                            * This should only be called if RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION
                                            * returns an API version of >= 1.
                                            * This should be called instead of RETRO_ENVIRONMENT_SET_VARIABLES.
                                            * This should be called the first time as early as
                                            * possible (ideally in retro_set_environment).
                                            * Afterwards it may be called again for the core to communicate
                                            * updated options to the frontend, but the number of core
                                            * options must not change from the number in the initial call.
                                            *
                                            * This is fundamentally the same as RETRO_ENVIRONMENT_SET_CORE_OPTIONS,
                                            * with the addition of localisation support. The description of the
                                            * RETRO_ENVIRONMENT_SET_CORE_OPTIONS callback should be consulted
                                            * for further details.
                                            *
                                            * 'data' points to a retro_core_options_intl struct.
                                            *
                                            * retro_core_options_intl::us is a pointer to an array of
                                            * retro_core_option_definition structs defining the US English
                                            * core options implementation. It must point to a valid array.
                                            *
                                            * retro_core_options_intl::local is a pointer to an array of
                                            * retro_core_option_definition structs defining core options for
                                            * the current frontend language. It may be NULL (in which case
                                            * retro_core_options_intl::us is used by the frontend). Any items
                                            * missing from this array will be read from retro_core_options_intl::us
                                            * instead.
                                            *
                                            * NOTE: Default core option values are always taken from the
                                            * retro_core_options_intl::us array. Any default values in
                                            * retro_core_options_intl::local array will be ignored.
                                            */

        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
                                           /* struct retro_core_option_display * --
                                            *
                                            * Allows an implementation to signal the environment to show
                                            * or hide a variable when displaying core options. This is
                                            * considered a *suggestion*. The frontend is free to ignore
                                            * this callback, and its implementation not considered mandatory.
                                            *
                                            * 'data' points to a retro_core_option_display struct
                                            *
                                            * retro_core_option_display::key is a variable identifier
                                            * which has already been set by SET_VARIABLES/SET_CORE_OPTIONS.
                                            *
                                            * retro_core_option_display::visible is a boolean, specifying
                                            * whether variable should be displayed
                                            *
                                            * Note that all core option variables will be set visible by
                                            * default when calling SET_VARIABLES/SET_CORE_OPTIONS.
                                            */

        case RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER:
                                           /* unsigned * --
                                            *
                                            * Allows an implementation to ask frontend preferred hardware
                                            * context to use. Core should use this information to deal
                                            * with what specific context to request with SET_HW_RENDER.
                                            *
                                            * 'data' points to an unsigned variable
                                            */

        case RETRO_ENVIRONMENT_GET_DISK_CONTROL_INTERFACE_VERSION:
                                           /* unsigned * --
                                            * Unsigned value is the API version number of the disk control
                                            * interface supported by the frontend. If callback return false,
                                            * API version is assumed to be 0.
                                            *
                                            * In legacy code, the disk control interface is defined by passing
                                            * a struct of type retro_disk_control_callback to
                                            * RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE.
                                            * This may be still be done regardless of the disk control
                                            * interface version.
                                            *
                                            * If version is >= 1 however, the disk control interface may
                                            * instead be defined by passing a struct of type
                                            * retro_disk_control_ext_callback to
                                            * RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE.
                                            * This allows the core to provide additional information about
                                            * disk images to the frontend and/or enables extra
                                            * disk control functionality by the frontend.
                                            */

        case RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE:
                                           /* const struct retro_disk_control_ext_callback * --
                                            * Sets an interface which frontend can use to eject and insert
                                            * disk images, and also obtain information about individual
                                            * disk image files registered by the core.
                                            * This is used for games which consist of multiple images and
                                            * must be manually swapped out by the user (e.g. PSX, floppy disk
                                            * based systems).
                                            */
        default:
            fprintf(stderr, TAG "Unknown environment call (%u, %p) = %d\n", cmd, data, result);
            break;
    }

    return result;
}

void retro_init(void) {
    init();

    s_init();
    fprintf(stderr, TAG "retro_init()\n");
}

void retro_deinit(void) {
    init();

    s_deinit();
    fprintf(stderr, TAG "retro_deinit()\n");

    dynlib_close(s_handle);
    s_handle = NULL;
}

unsigned retro_api_version(void) {
    init();

    unsigned const result = s_api_version();
    fprintf(stderr, TAG "retro_api_version() = %u\n", result);

    return result;
}

void retro_get_system_info(struct retro_system_info* info) {
    init();

    s_get_system_info(info);
    fprintf(stderr, TAG "retro_get_system_info(%p)\n", info);

#ifndef QUIET
    fprintf(stderr, TAG "    ->library_name     = \"%s\"\n", info->library_name);
    fprintf(stderr, TAG "    ->library_version  = \"%s\"\n", info->library_version);
    fprintf(stderr, TAG "    ->valid_extensions = \"%s\"\n", info->valid_extensions);
    fprintf(stderr, TAG "    ->need_fullpath    = %d\n", info->need_fullpath);
    fprintf(stderr, TAG "    ->block_extract    = %d\n", info->block_extract);
#endif
}

void retro_get_system_av_info(struct retro_system_av_info* info) {
    init();

    s_get_system_av_info(info);
    fprintf(stderr, TAG "retro_get_system_av_info(%p)\n", info);

#ifndef QUIET
    fprintf(stderr, TAG "    ->geometry.base_width   = %u\n", info->geometry.base_width);
    fprintf(stderr, TAG "    ->geometry.base_height  = %u\n", info->geometry.base_height);
    fprintf(stderr, TAG "    ->geometry.max_width    = %u\n", info->geometry.max_width);
    fprintf(stderr, TAG "    ->geometry.max_height   = %u\n", info->geometry.max_height);
    fprintf(stderr, TAG "    ->geometry.aspect_ratio = %f\n", info->geometry.aspect_ratio);
    fprintf(stderr, TAG "    ->timing.fps            = %f\n", info->timing.fps);
    fprintf(stderr, TAG "    ->timing.sample_rate    = %f\n", info->timing.sample_rate);
#endif
}

void retro_set_environment(retro_environment_t cb) {
    init();

    s_env = cb;
    s_set_environment(environment);
    fprintf(stderr, TAG "retro_set_environment(%p)\n", cb);
}

void retro_set_video_refresh(retro_video_refresh_t cb) {
    init();

    s_set_video_refresh(cb);
    fprintf(stderr, TAG "retro_set_video_refresh(%p)\n", cb);
}

void retro_set_audio_sample(retro_audio_sample_t cb) {
    init();

    s_set_audio_sample(cb);
    fprintf(stderr, TAG "retro_set_audio_sample(%p)\n", cb);
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) {
    init();

    s_set_audio_sample_batch(cb);
    fprintf(stderr, TAG "retro_set_audio_sample_batch(%p)\n", cb);
}

void retro_set_input_poll(retro_input_poll_t cb) {
    init();

    s_set_input_poll(cb);
    fprintf(stderr, TAG "retro_set_input_poll(%p)\n", cb);
}

void retro_set_input_state(retro_input_state_t cb) {
    init();

    s_set_input_state(cb);
    fprintf(stderr, TAG "retro_set_input_state(%p)\n", cb);
}

void retro_set_controller_port_device(unsigned port, unsigned device) {
    init();

    s_set_controller_port_device(port, device);
    fprintf(stderr, TAG "retro_set_controller_port_device(%u, %u)\n", port, device);
}

void retro_reset(void) {
    init();

    s_reset();
    fprintf(stderr, TAG "retro_reset()\n");
}

void retro_run(void) {
    init();

    s_run();
    fprintf(stderr, TAG "retro_run()\n");
}

size_t retro_serialize_size(void) {
    init();

    size_t const result = s_serialize_size();
    fprintf(stderr, TAG "retro_serialize_size() = %zu\n", result);

    return result;
}

bool retro_serialize(void* data, size_t size) {
    init();

    bool const result = s_serialize(data, size);
    fprintf(stderr, TAG "retro_serialize(%p, %zu) = %d\n", data, size, result);

    return result;
}

bool retro_unserialize(void const* data, size_t size) {
    init();

    bool const result = s_unserialize(data, size);
    fprintf(stderr, TAG "retro_unserialize(%p, %zu) = %d\n", data, size, result);

    return result;
}

void retro_cheat_reset(void) {
    init();

    s_cheat_reset();
    fprintf(stderr, TAG "retro_cheat_reset()\n");
}

void retro_cheat_set(unsigned index, bool enabled, char const* code) {
    init();

    s_cheat_set(index, enabled, code);
    fprintf(stderr, TAG "retro_cheat_set(%u, %d, \"%s\")\n", index, enabled, code);
}

bool retro_load_game(struct retro_game_info const* game) {
    init();

    bool const result = s_load_game(game);
    fprintf(stderr, TAG "retro_load_game(%p) = %d\n", game, result);

#ifndef QUIET
    fprintf(stderr, TAG "    ->path = \"%s\"\n", game->path);
    fprintf(stderr, TAG "    ->data = %p\n", game->data);
    fprintf(stderr, TAG "    ->size = %zu\n", game->size);
    fprintf(stderr, TAG "    ->meta = \"%s\"\n", game->meta);
#endif

    return result;
}

bool retro_load_game_special(unsigned game_type, struct retro_game_info const* info, size_t num_info) {
    init();

    bool const result = s_load_game_special(game_type, info, num_info);
    fprintf(stderr, TAG "retro_load_game_special(%u, %p, %zu) = %d\n", game_type, info, num_info, result);

#ifndef QUIET
    for (size_t i = 0; i < num_info; i++) {
        fprintf(stderr, TAG "    [%zu].path = \"%s\"\n", i, info[i].path);
        fprintf(stderr, TAG "    [%zu].data = %p\n", i, info[i].data);
        fprintf(stderr, TAG "    [%zu].size = %zu\n", i, info[i].size);
        fprintf(stderr, TAG "    [%zu].meta = \"%s\"\n", i, info[i].meta);
    }
#endif

    return result;
}

void retro_unload_game(void) {
    init();

    s_unload_game();
    fprintf(stderr, TAG "retro_unload_game()\n");
}

unsigned retro_get_region(void) {
    init();

    unsigned const result = s_getRegion();
    fprintf(stderr, TAG "retro_get_region() = %u\n", result);

    return result;
}

void* retro_get_memory_data(unsigned id) {
    init();

    void* const result = s_get_memory_data(id);
    fprintf(stderr, TAG "retro_get_memory_data(%u) = %p\n", id, result);

    return result;
}

size_t retro_get_memory_size(unsigned id) {
    init();

    size_t const result = s_get_memory_size(id);
    fprintf(stderr, TAG "retro_get_memory_size(%u) = %zu\n", id, result);

    return result;
}
