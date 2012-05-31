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

    # @return [Fixnum] required size of audio buffer in bytes
    def audio_framebuffer_size
      @session[:audio_framebuffer_size]
    end

    def create_audio_buffer
      FFI::MemoryPointer.new(audio_framebuffer_size)
    end

    # @param [Fixnum] buffer_count number of audio buffers the mixer will
    #  be able to mix
    # @return [AudioMixer]
    def create_audio_mixer
      AudioMixer.new(self)
    end
  end
end
