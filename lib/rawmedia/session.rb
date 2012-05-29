module RawMedia
  class Session
    attr_reader :session
    attr_reader :framerate

    def initialize(framerate=Rational(30))
      @session = Internal::RawMediaSession.new
      @session[:framerate_num] = framerate.numerator
      @session[:framerate_den] = framerate.denominator
      Internal::check Internal::rawmedia_init_session(@session)
      @framerate = framerate
    end

    def audio_framebuffer_size
      @session[:audio_framebuffer_size]
    end

    def create_audio_buffer
      FFI::Buffer.new_out(audio_framebuffer_size)
    end
  end
end
