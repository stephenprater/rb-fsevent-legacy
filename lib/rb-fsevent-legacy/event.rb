module FSEvent
  class Event
    #an ACTUAL filesystem event
    attr_accessor :event_path, :event_flags, :event_id

    def to_s 
      "#<%s:0x%x event_path='%s' event_flags='%s' event_id='%s'>" \
        % [self.class, self.object_id.abs * 2,self.event_path, self.event_flags, self.event_id]
    end

    def event_flag
      # honestly, since this is the legacy system i'm not sure this is
      # ever anything but zero. so lazily do nothing until i know otherwise
      # break out the event flags into the something meaningful
    end
  end
end
