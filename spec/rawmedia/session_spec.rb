require 'spec_helper'

module RawMedia
  describe Session do
    it 'should compute audio framebuffer size' do
      session = Session.new(Rational(30))
      session.audio_framebuffer_size.should == 5880
    end

    it 'should create an audio buffer' do
      session = Session.new(Rational(25))
      session.create_audio_buffer.size.should == 7056
    end
  end
end
