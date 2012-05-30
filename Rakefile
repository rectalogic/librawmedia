require 'rubygems'
require 'bundler'
Bundler::GemHelper.install_tasks
require 'rspec/core/rake_task'
require 'rawmedia/rake/fixture_task'
require 'ffi'

libname = "lib/#{FFI::Platform::LIBPREFIX}rawmedia.#{FFI::Platform::LIBSUFFIX}"

file libname => Dir.glob("rawmedia/*{.h,.c}") do
  Dir.chdir("ext/rawmedia") do
    ruby "extconf.rb"
    sh "make && make install"
  end
end

desc "Build native library during development"
task :lib => libname

RSpec::Core::RakeTask.new(:spec)
task :spec => :lib

desc 'Run RSpec code examples with simplecov'
RSpec::Core::RakeTask.new(:coverage) do |task|
    task.rcov = true
    task.rcov_path = 'rspec'
    task.rcov_opts = '--require simplecov_start'
end
task :coverage => :lib

fixture_320x240_30fps = 'spec/fixtures/320x240-30fps.mov'
RawMedia::Rake::FixtureTask.new(fixture_320x240_30fps) do |task|
  task.framerate = '30'
  task.size = '320x240'
end
fixture_320x180_25fps = 'spec/fixtures/320x180-25fps.mov'
RawMedia::Rake::FixtureTask.new(fixture_320x180_25fps) do |task|
  task.framerate = '25'
  task.size = '320x180'
end
task :fixtures => [fixture_320x240_30fps, fixture_320x180_25fps]
