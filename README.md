# Overview

librawmedia provides a simplified API on top of FFmpeg for decoding
frames of video and corresponding audio into raw buffers which can
then be manipulated before multiplexing into an output MOV file.

A Ruby wrapper built using FFI is included.

## WIP

librawmedia is currently a work in progress and incomplete.
It's main purpose is for use with [RenderMix](https://github.com/rectalogic/rendermix)

Currently working with FFmpeg as of commit 9586db6

## Building

Building librawmedia requires cmake.
Use `rake gem:install` to build and install the gem.
If FFmpeg is installed in a non-standard location, set `PKG_CONFIG_PATH`:

    PKG_CONFIG_PATH=<ffmpeg-install-dir>/lib/pkgconfig rake gem:install

## License

Copyright (c) 2012 Hewlett-Packard Development Company, L.P. All rights reserved.
Use of this source code is governed by a BSD-style license that can be
found in the LICENSE file.
