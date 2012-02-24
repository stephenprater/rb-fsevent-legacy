require 'rspec'
require 'rb-fsevent-legacy'

RSpec.configure do |config|
  config.color_enabled = true
  
  config.before(:each) do
    @fixture_path = Pathname.new(File.expand_path('../fixtures/', __FILE__))
  end
  
  config.after(:all) do
    gem_root = Pathname.new(File.expand_path('../../', __FILE__))
    system "rm -rf #{gem_root.join('bin')}"
  end
  
end
