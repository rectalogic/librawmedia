# Copyright (c) 2012 Hewlett-Packard Development Company, L.P. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found in the LICENSE file.

require 'rubygems'
require 'bundler'
Bundler::GemHelper.install_tasks
require 'rspec/core/rake_task'
require 'rawmedia/rake/video_fixture_task'
require 'ffi'

libname = "lib/#{FFI::Platform::LIBPREFIX}rawmedia.#{FFI::Platform::LIBSUFFIX}"

file libname => Dir.glob("rawmedia/*{.h,.c}") do
  Dir.chdir("ext/rawmedia") do
    ruby 'extconf.rb', '--debug'
    sh "make && make install"
  end
end

fixture_320x240_30fps = 'spec/fixtures/320x240-30fps.mov'
fixture_320x180_25fps = 'spec/fixtures/320x180-25fps.mov'

desc "Build native library during development"
task :lib => libname

RSpec::Core::RakeTask.new(:spec) do |task|
  task.rspec_opts = %{--color --format progress}
end
task :spec => [:lib, fixture_320x240_30fps, fixture_320x180_25fps]

desc 'Run RSpec code examples with simplecov'
RSpec::Core::RakeTask.new(:coverage) do |task|
  task.rcov = true
  task.rcov_path = 'rspec'
  task.rcov_opts = '--require simplecov_start'
end
task :coverage => [:lib, fixture_320x240_30fps, fixture_320x180_25fps]

directory 'spec/fixtures'

RawMedia::Rake::VideoFixtureTask.new(fixture_320x240_30fps) do |task|
  task.framerate = '30'
  task.size = '320x240'
end
task fixture_320x240_30fps => 'spec/fixtures'
RawMedia::Rake::VideoFixtureTask.new(fixture_320x180_25fps) do |task|
  task.framerate = '25'
  task.size = '320x180'
end
task fixture_320x180_25fps => 'spec/fixtures'

desc "Generate all media fixtures"
task :fixtures => [fixture_320x240_30fps, fixture_320x180_25fps]
