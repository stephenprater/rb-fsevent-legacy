require "rb-fsevent-legacy/version"

if defined? FSEvent
  Object.send :remove_const, :FSEvent
end

class FSEvent
  require "rb-fsevent-legacy/stream"
  require "rb-fsevent-legacy/event"
  require "rb-fsevent-legacy/fs_native_stream"
end
