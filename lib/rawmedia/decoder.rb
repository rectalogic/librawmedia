module RawMedia
  class Decoder
    # @param [Hash] opts decoding options.
    # @option opts [Float] :volume Exponential volume, 0..1
    # @option opts [Fixnum] :start_frame Starting video frame in target framerate
    # @option opts [Boolean] :discard_video Ignore video if True
    # @option opts [Boolean] :discard_audio Ignore audio if True
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

    # Wrap pointer in a Java ByteBuffer.
    # @param [FFI::Pointer] buffer decoded video buffer.
    # @param [Fixnum] linesize of the buffer
    # @return [java.nio.ByteBuffer] Java buffer wrapping buffer
    def wrap_video_buffer(buffer, linesize)
      org.jruby.ext.ffi.Factory.getInstance().
        wrapDirectMemory(JRuby.runtime, buffer.address).
        slice(0, @height * linesize).asByteBuffer()
    end

    # @return [FFI::Pointer, Fixnum] a pointer to the decoded buffer,
    #   and the linesize of that buffer.
    #   Buffer is valid until the next call to decode_video
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
end
