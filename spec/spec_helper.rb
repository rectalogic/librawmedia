require_relative '../lib/rawmedia'

Dir[File.expand_path("../support/**/*.rb", __FILE__)].each {|f| require f}

RSpec.configure do |config|
  config.mock_framework = :rspec
  config.expect_with :rspec
end
