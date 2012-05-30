require 'spec_helper'

module RawMedia
  describe Log do
    it 'should log messages' do
      log = double('log')
      log.should_receive(:log).at_least(:once).with('ok')
      callback = Proc.new do
        log.log('ok')
      end
      Log.set_callback(Log::LEVEL_DEBUG, callback)
      # Open decoder to generate log messages
      session = Session.new
      filename = File.expand_path('../../fixtures/320x240-30fps.mov', __FILE__)
      decoder = Decoder.new(filename, session, 1000, 1000)
    end
  end
end
