module RawMedia
  class Encoder
    def initialize(filename, session, width, height, has_video=true, has_audio=true)
      config = Internal::RawMediaEncoderConfig.new
      config[:width] = width
      config[:height] = height
      config[:has_video] = has_video
      config[:has_audio] = has_audio
      encoder = Internal::rawmedia_create_encoder(filename, session.session, config)
      raise(RawMediaError, "Failed to create Encoder for #{filename}") unless encoder
      # Wrap in ManagedStruct to manage lifetime
      @encoder = Internal::RawMediaEncoder.new(encoder)
    end

    def encode_video(buffer, buffersize)
      Internal::check Internal::rawmedia_encode_video(@encoder, buffer, buffersize)
    end

    def encode_audio(buffer)
      Internal::check Internal::rawmedia_encode_audio(@encoder, buffer)
    end

    def destroy
      @encoder.autorelease = false
      Internal::check Internal::rawmedia_destroy_encoder(@encoder)
    end
  end
end
