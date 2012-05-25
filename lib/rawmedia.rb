begin
  require 'java'
rescue LoadError
end

require 'ffi'
require 'rational'

require 'rawmedia/version'
require 'rawmedia/errors'
require 'rawmedia/internal'
require 'rawmedia/session'
require 'rawmedia/decoder'
require 'rawmedia/encoder'
