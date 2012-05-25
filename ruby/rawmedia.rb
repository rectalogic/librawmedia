require 'java'
require 'ffi'
require 'rational'

module RawMedia
  class RawMediaError < StandardError; end

  class Session
    attr_reader :session
    attr_reader :width
    attr_reader :height
    attr_reader :framerate
    def initialize(width, height, framerate=Rational(30))
      @session = Internal::RawMediaSession.new
      @session[:framerate_num] = framerate.numerator
      @session[:framerate_den] = framerate.denominator
      @session[:width] = width
      @session[:height] = height
      Internal::check Internal::rawmedia_init_session(@session)
      @width = width
      @height = height
      @framerate = framerate
    end
    def audio_framebuffer_size
      @session[:audio_framebuffer_size]
    end
  end

  class Decoder
    # opts:
    #  :volume - exponential volume, 0..1
    #  :start_frame - starting video frame in target framerate
    #  :discard_video - ignore video if True
    #  :discard_audio = ignore audio if True
    def initialize(filename, session, opts={})
      volume = opts[:volume] || 1.0
      # Use an exponential curve for volume
      # See http://www.dr-lex.be/info-stuff/volumecontrols.html
      if volume > 0 and volume < 1
        volume = Math.exp(6.908 * volume) / 1000.0
        volume *= volume * 10 if volume < 0.1
      end
      config = Internal::RawMediaDecoderConfig.new
      config[:start_frame] = opts[:start_frame] || 0
      config[:volume] = volume
      config[:discard_video] = opts[:discard_video]
      config[:discard_audio] = opts[:discard_audio]
      decoder = Internal::rawmedia_create_decoder(filename, session.session, config)
      raise(RawMediaError, "Failed to create Decoder for #{filename}") unless decoder
      # Wrap in ManagedStruct to manage lifetime
      @decoder = Internal::RawMediaDecoder.new(decoder)
      info = Internal::rawmedia_get_decoder_info(@decoder)
      @info = Internal::RawMediaDecoderInfo.new(info)
      @audio_framebuffer_size = session.audio_framebuffer_size
      @height = session.height

      # Pointers for decode_video
      @linesize_ptr = FFI::MemoryPointer.new :int
      @video_buffer_ptr = FFI::MemoryPointer.new :pointer
    end

    def create_audio_buffer
      FFI::Buffer.new_out(@audio_framebuffer_size)
    end

    # Wrap buffer FFI::Pointer in a Java ByteBuffer
    def wrap_video_buffer(buffer, linesize)
      org.jruby.ext.ffi.Factory.getInstance().
        wrapDirectMemory(JRuby.runtime, buffer.address).
        slice(0, @height * linesize).asByteBuffer()
    end

    # Return Pointer and linesize of internal video buffer.
    # Buffer is valid until the next call to decode_video
    def decode_video
      Internal::check Internal::rawmedia_decode_video(@decoder,
                                                      @video_buffer_ptr,
                                                      @linesize_ptr)
      return @video_buffer_ptr.get_pointer, @linesize_ptr.get_int
    end

    # Decodes audio into the provided buffer.
    # Buffer must be of size Session::audio_framebuffer_size,
    # use create_audio_buffer to construct.
    def decode_audio(buffer)
      Internal::check Internal::rawmedia_decode_audio(@decoder, buffer)
    end

    def duration
      @info[:duration]
    end

    def has_video?
      @info[:has_video]
    end

    def has_audio?
      @info[:has_audio]
    end

    def destroy
      @decoder.autorelease = false
      Internal::check Internal::rawmedia_destroy_decoder(@decoder)
    end
  end

  class Encoder
    def initialize(filename, session, has_video=true, has_audio=true)
      config = Internal::RawMediaEncoderConfig.new
      config[:has_video] = has_video
      config[:has_audio] = has_audio
      encoder = Internal::rawmedia_create_encoder(filename, session.session, config)
      raise(RawMediaError, "Failed to create Encoder for #{filename}") unless encoder
      # Wrap in ManagedStruct to manage lifetime
      @encoder = Internal::RawMediaEncoder.new(encoder)
    end

    def encode_video(buffer, linesize)
      Internal::check Internal::rawmedia_encode_video(@encoder, buffer, linesize)
    end

    def encode_audio(buffer)
      Internal::check Internal::rawmedia_encode_audio(@encoder, buffer)
    end

    def destroy
      @encoder.autorelease = false
      Internal::check Internal::rawmedia_destroy_encoder(@encoder)
    end
  end

  module Internal
    extend FFI::Library
    ffi_lib 'rawmedia'

    attach_function :rawmedia_init, [], :void
    attach_function :rawmedia_init_session, [:pointer], :int
    attach_function :rawmedia_create_decoder, [:string, :pointer, :pointer], :pointer
    attach_function :rawmedia_get_decoder_info, [:pointer], :pointer
    attach_function :rawmedia_decode_video, [:pointer, :pointer, :pointer], :int
    attach_function :rawmedia_decode_audio, [:pointer, :buffer_out], :int
    attach_function :rawmedia_destroy_decoder, [:pointer], :int
    attach_function :rawmedia_create_encoder, [:string, :pointer, :pointer], :pointer
    attach_function :rawmedia_encode_video, [:pointer, :pointer, :int], :int
    attach_function :rawmedia_encode_audio, [:pointer, :buffer_in], :int
    attach_function :rawmedia_destroy_encoder, [:pointer], :int
    

    class RawMediaSession < FFI::Struct
      layout :framerate_num, :int,
             :framerate_den, :int,
             :width, :int,
             :height, :int,
             :audio_framebuffer_size, :int
    end
    class RawMediaDecoder < FFI::AutoPointer
      def self.release(ptr)
        Internal::rawmedia_destroy_decoder(ptr)
      end
    end
    class RawMediaDecoderConfig < FFI::Struct
      layout :start_frame, :int,
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
      layout :has_video, :bool,
             :has_audio, :bool
    end

    def self.check(result)
      raise RawMediaError if result < 0
    end

    rawmedia_init()
  end
end
