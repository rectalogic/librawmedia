# Copyright (c) 2012 Hewlett-Packard Development Company, L.P. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found in the LICENSE file.

module RawMedia
  class Decoder
    # @param [Fixnum] max_width maximum width of decoded video
    # @param [Fixnum] max_height maximum height of decoded video
    # @param [Hash] opts decoding options.
    # @option opts [Float] :volume Exponential volume, 0..1
    # @option opts [Fixnum] :start_frame Starting video frame in target framerate
    # @option opts [Boolean] :discard_video Ignore video if True
    # @option opts [Boolean] :discard_audio Ignore audio if True
    def initialize(filename, session, max_width, max_height, opts={})
      volume = opts.fetch(:volume, 1.0)
      # Use an exponential curve for volume
      # See http://www.dr-lex.be/info-stuff/volumecontrols.html
      if volume > 0 and volume < 1
        volume = Math.exp(6.908 * volume) / 1000.0
        volume *= volume * 10 if volume < 0.1
      end
      config = Internal::RawMediaDecoderConfig.new
      config[:max_width] = max_width
      config[:max_height] = max_height
      config[:start_frame] = opts.fetch(:start_frame, 0)
      config[:volume] = volume
      config[:discard_video] = opts[:discard_video]
      config[:discard_audio] = opts[:discard_audio]
      decoder = Internal::rawmedia_create_decoder(filename, session.session, config)
      raise(RawMediaError, "Failed to create Decoder for #{filename}") unless decoder
      # Wrap in AutoPointer to manage lifetime
      @decoder = Internal::RawMediaDecoder.new(decoder)
      info = Internal::rawmedia_get_decoder_info(@decoder)
      @info = Internal::RawMediaDecoderInfo.new(info)

      # Pointers for decode_video
      @video_buffer_ptr = FFI::MemoryPointer.new :pointer
      @width_ptr = FFI::MemoryPointer.new :int
      @height_ptr = FFI::MemoryPointer.new :int
      @video_buffer_size_ptr = FFI::MemoryPointer.new :int
    end

    def width
      @width_ptr.get_int
    end

    def height
      @height_ptr.get_int
    end

    def video_buffer
      @video_buffer_ptr.get_pointer
    end

    def video_buffer_size
      @video_buffer_size_ptr.get_int
    end

    # Wraps video_buffer in a Java ByteBuffer.
    # @return [java.nio.ByteBuffer] Java buffer wrapping buffer
    def video_byte_buffer
      Util.wrap_pointer(video_buffer, video_buffer_size)
    end

    # Decode a frame of video.
    # After decoding, #width, #height, #buffer and #buffer_size will all be
    # valid until the next call to decode_video.
    # @return [Fixnum] 0 if no new frame decoded, > 0 if frame decoded
    def decode_video
      Internal::check Internal::rawmedia_decode_video(@decoder,
                                                      @video_buffer_ptr,
                                                      @width_ptr,
                                                      @height_ptr,
                                                      @video_buffer_size_ptr)
    end

    # Decodes audio into the provided buffer.
    # @param [FFI::Buffer] buffer a buffer of at least size Session#audio_framebuffer_size
    def decode_audio(buffer)
      Internal::check Internal::rawmedia_decode_audio(@decoder, buffer)
    end

    def duration
      @duration ||= @info[:duration]
    end

    def has_video?
      @has_video ||= @info[:has_video]
    end

    def has_audio?
      @has_audio ||= @info[:has_audio]
    end

    def destroy
      @decoder.autorelease = false
      Internal::check Internal::rawmedia_destroy_decoder(@decoder)
      @decoder = nil
    end
  end
end
