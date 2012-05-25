require 'bundler'
Bundler::GemHelper.install_tasks
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
