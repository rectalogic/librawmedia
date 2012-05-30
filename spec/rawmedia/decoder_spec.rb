require 'spec_helper'

module RawMedia
  describe Decoder do
    let(:framerate) { Rational(15) }
    let(:session) { Session.new(framerate) }
    let(:filename) { File.expand_path('../../fixtures/320x240-30fps.mov', __FILE__) }

    it 'should not have a size until decoded' do
      decoder = Decoder.new(filename, session, 1000, 1000)
      decoder.width.should == 0
      decoder.height.should == 0
      decoder.video_buffer.address.should == 0
    end

    it 'should have audio, video and duration' do
      decoder = Decoder.new(filename, session, 100, 100)
      decoder.has_audio?.should be true
      decoder.has_video?.should be true
      # Video is 5s long
      decoder.duration.should be_within(5).of(5*framerate.to_f)
    end

    it 'should discard audio' do
      decoder = Decoder.new(filename, session, 100, 100, discard_audio: true)
      decoder.has_audio?.should be false
    end

    it 'should have a size once decoded and not upscale' do
      decoder = Decoder.new(filename, session, 1000, 1000)
      decoder.decode_video
      decoder.width.should == 320
      decoder.height.should == 240
      decoder.video_buffer.address.should_not == 0
    end

    it 'should scale decoded video to meet bounds' do
      decoder = Decoder.new(filename, session, 300, 300)
      decoder.decode_video
      decoder.width.should == 300
      decoder.height.should == 225
    end

    it 'should decode audio' do
      decoder = Decoder.new(filename, session, 300, 300)
      buffer = session.create_audio_buffer
      decoder.decode_audio(buffer)
      decoder.destroy
    end

    it 'should be destroyed' do
      decoder = Decoder.new(filename, session, 300, 300)
      decoder.decode_video
      decoder.decode_audio(session.create_audio_buffer)
      decoder.destroy
    end
  end
end
