# Copyright (c) 2012 Hewlett-Packard Development Company, L.P. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found in the LICENSE file.

$LOAD_PATH.unshift(File.expand_path('..', __FILE__)).uniq!

begin
  require 'java'
rescue LoadError
end

require 'ffi'
require 'rational'

require 'rawmedia/version'
require 'rawmedia/errors'
require 'rawmedia/util'
require 'rawmedia/internal'
require 'rawmedia/log'
require 'rawmedia/session'
require 'rawmedia/decoder'
require 'rawmedia/encoder'
require 'rawmedia/audio_mixer'
