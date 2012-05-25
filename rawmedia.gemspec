$:.push File.expand_path("../lib", __FILE__)
require "rawmedia/version"

Gem::Specification.new do |s|
  s.name        = "rawmedia"
  s.version     = RawMedia::VERSION
  s.platform    = Gem::Platform::RUBY
  s.authors     = ["Andrew Wason"]
  s.email       = ["rectalogic@rectalogic.com"]
  s.homepage    = ""
  s.summary     = %q{Audio/video decoding and raw encoding via ffmpeg}
  s.description = s.summary

  s.files       = `git ls-files`.split("\n")
  s.extensions  = ['ext/rawmedia/extconf.rb']

  s.require_paths = ["lib"]
end