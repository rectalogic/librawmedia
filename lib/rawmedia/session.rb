module RawMedia
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
end
