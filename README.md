# lrproxy

A Libretro core that loads another core and intercepts all calls from a Libretro frontend to it, logging them to `stderr` as they happen.

## Configuration

Edit `lrproxy.c` and change the `PROXY_FOR` macro at the top with the name of the core you want it to load.

## Build

It's just two files, so just build a shared library out of them, using `-DPROXY_FOR=dosbox_pure_libretro.so` to specify the core you want it to load:

```
$ gcc -O2 -fPIC -shared -o proxy_core.so lrproxy.c dynlib.c
```

If the amount of logging is too much, use `-DQUIET` to make it less verbose.

## TODO

* Better logging of API arguments and returned values
* Log environment calls

PRs welcome.

## License

MIT, enjoy.

## Log of RetroArch

This is the API log generated using **lrproxy** to load the DOSBox Pure core, with some notes from me.

As soon as core is loaded:

```
APILOG retro_get_system_info(0x7ffd4dc16e10)
APILOG retro_set_environment(0x55e41099b910) this one doesn't happen if a core and a content are being loaded from the cmdline
APILOG retro_set_environment(0x55e4109c8a30) different environment functions!
```

Content is loaded:

```
APILOG retro_init()
APILOG retro_load_game(0x555fa5bbc820) = 1
APILOG retro_set_video_refresh(0x555fa31317b0)
APILOG retro_set_audio_sample(0x555fa3103500)
APILOG retro_set_audio_sample_batch(0x555fa3103610)
APILOG retro_set_input_state(0x555fa3109960)
APILOG retro_set_video_refresh(0x555fa31317b0)
APILOG retro_set_audio_sample(0x555fa3103500)
APILOG retro_set_audio_sample_batch(0x555fa3103610)
APILOG retro_set_input_state(0x555fa3109960)
APILOG retro_api_version() = 1
APILOG retro_set_video_refresh(0x555fa31317b0)
APILOG retro_set_audio_sample(0x555fa3103500)
APILOG retro_set_audio_sample_batch(0x555fa3103610)
APILOG retro_set_input_state(0x555fa3118df0)
APILOG retro_set_input_poll(0x555fa3118dc0)
APILOG retro_get_system_av_info(0x555fa3bab1b0)
APILOG retro_get_system_info(0x7ffff748bab0)
APILOG retro_get_system_info(0x7ffff74848b0)
APILOG retro_set_environment(0x555fa3101910)
APILOG retro_set_environment(0x555fa312ea30)
APILOG retro_set_controller_port_device(0, 257)
APILOG retro_set_controller_port_device(1, 513)
APILOG retro_set_controller_port_device(2, 259)
APILOG retro_set_controller_port_device(3, 1)
APILOG retro_set_controller_port_device(4, 1)
APILOG retro_set_controller_port_device(5, 0)
APILOG retro_set_controller_port_device(6, 0)
APILOG retro_set_controller_port_device(7, 0)
APILOG retro_run() many of these of course
```

Content is closed:

```
APILOG retro_set_video_refresh(0x555fa31317b0)
APILOG retro_set_audio_sample(0x555fa3103500)
APILOG retro_set_audio_sample_batch(0x555fa3103610)
APILOG retro_set_input_state(0x555fa3118df0)
APILOG retro_get_system_info(0x7ffff74765e0)
APILOG retro_unload_game()
APILOG retro_deinit()
APILOG retro_get_system_info(0x7ffff7477610) core is unloaded and loaded again for a new content, but the UI says "no core"
APILOG retro_set_environment(0x555fa3101910)
APILOG retro_set_environment(0x555fa312ea30)
```

There are no further API calls when RetroArch is closed, it seems it doesn't properly shut the core down.
