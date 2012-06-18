# Copyright (c) 2012 Hewlett-Packard Development Company, L.P. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found in the LICENSE file.

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
      @audio_framebuffer_size ||= @session[:audio_framebuffer_size]
    end

    def create_audio_buffer
      FFI::MemoryPointer.new(audio_framebuffer_size)
    end

    # @return [AudioMixer]
    def create_audio_mixer
      AudioMixer.new(self)
    end
  end
end
