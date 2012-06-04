# Copyright (c) 2012 Hewlett-Packard Development Company, L.P. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found in the LICENSE file.

module RawMedia
  module Internal
    extend FFI::Library
    ffi_lib File.expand_path("../../#{FFI::Platform::LIBPREFIX}rawmedia.#{FFI::Platform::LIBSUFFIX}", __FILE__)

    attach_function :rawmedia_init, [], :void
    callback :log_callback, [:string], :void
    attach_function :rawmedia_set_log, [:int, :log_callback], :void
    attach_function :rawmedia_init_session, [:pointer], :int
    attach_function :rawmedia_mix_audio, [:pointer, :pointer, :int, :pointer], :void
    attach_function :rawmedia_create_decoder, [:string, :pointer, :pointer], :pointer
    attach_function :rawmedia_get_decoder_info, [:pointer], :pointer
    attach_function :rawmedia_decode_video, [:pointer, :pointer, :pointer, :pointer, :pointer], :int
    attach_function :rawmedia_decode_audio, [:pointer, :pointer], :int
    attach_function :rawmedia_destroy_decoder, [:pointer], :int
    attach_function :rawmedia_create_encoder, [:string, :pointer, :pointer], :pointer
    attach_function :rawmedia_encode_video, [:pointer, :pointer, :int], :int
    attach_function :rawmedia_encode_audio, [:pointer, :pointer], :int
    attach_function :rawmedia_destroy_encoder, [:pointer], :int
    

    class RawMediaSession < FFI::Struct
      layout :framerate_num, :int,
             :framerate_den, :int,
             :audio_framebuffer_size, :int
    end
    class RawMediaDecoder < FFI::AutoPointer
      def self.release(ptr)
        Internal::rawmedia_destroy_decoder(ptr)
      end
    end
    class RawMediaDecoderConfig < FFI::Struct
      layout :max_width, :int,
             :max_height, :int,
             :start_frame, :int,
             :volume, :float,
             :discard_video, :bool,
             :discard_audio, :bool
    end
    class RawMediaDecoderInfo < FFI::Struct
      layout :duration, :int,
             :has_video, :bool,
             :has_audio, :bool
    end
    class RawMediaEncoder < FFI::AutoPointer
      def self.release(ptr)
        Internal::rawmedia_destroy_encoder(ptr)
      end
    end
    class RawMediaEncoderConfig < FFI::Struct
      layout :width, :int,
             :height, :int,
             :has_video, :bool,
             :has_audio, :bool
    end

    def self.check(result)
      raise RawMediaError if result < 0
      result
    end

    rawmedia_init()
  end
end
