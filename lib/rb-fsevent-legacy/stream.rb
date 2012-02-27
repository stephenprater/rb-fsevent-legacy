class FSEvent
  class Stream
    class << self
      def running?
        if NativeStream.timer_events > 0 then true end
      end
    end

    #the callback recieves FSEvent objects which describe the events
    def initialize(path = nil, latency = 1.0)
      unless path.nil?
        @streams = [NativeStream.new(path,Float(latency))]
      else
        @streams = []
      end
      @run = false;
    end

    def latency
      @latency || 1.0
    end

    def latency= val
      @latency = Float(val)
    end

    def watch path = nil, &block
      @streams.push(NativeStream.new(path,self.latency)) unless path.nil?
      @call_block = block
      self
    end
    
    def empty
      @streams.each { |s| s.stop }.delete_if { |s| not s.running? }
    end

    def paths
      @streams.collect { |s| s.path }
    end

    def add_paths paths
      paths.each do |p|
        self.add_path p
      end
    end

    def add_path path
      @streams.push(NativeStream.new(path,self.latency))
    end

    def status
      status = @streams.collect do |s|
        s.running?
      end
    end

    def running?
      status = @streams.collect { |s| s.running? }
      if not status.include? true
        false
      elsif not status.include? false
        true
      else
        raise StandardError, "native streams are in mixed states"
      end
    end

    def killall
      @streams.each do |s|
        s.stop
      end
    end

    def streams
      @streams
    end

    def events
      events = []
      @streams.each do |s|
        s.events do |e|
          if block_given?
            events << (yield e)
          else
            events << e
          end
        end
      end
      events.compact
    end

    def run 
      @run = true
      while @run do 
        self.events do |e|
          if @call_block
            @call_block.call(e)
          end
        end
        sleep 1
      end
    end

    def stop
      @run = false
    end
  end
end
