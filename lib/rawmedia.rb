$LOAD_PATH.unshift(File.expand_path('..', __FILE__)).uniq!

begin
  require 'java'
rescue LoadError
end

require 'ffi'
require 'rational'

require 'rawmedia/version'
require 'rawmedia/errors'
require 'rawmedia/internal'
require 'rawmedia/log'
require 'rawmedia/session'
require 'rawmedia/decoder'
require 'rawmedia/encoder'
require 'rawmedia/audio_mixer'
