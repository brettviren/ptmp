// Jsonnet support for producing data structures for PTMP

{
    util : {
        // Return the file name sans path and extension
        basename(filepath) :: {
            local path=std.split(filepath, '/'),
            ret : std.split(path[std.length(path)-1], '.')[0]
        }.ret,

        // return the last element of an array
        back(arr) :: arr[std.length(arr)-1],
    },


    // Make a TCP address from its parts
    tcp(host="127.0.0.1", port=5678) :: "tcp://%s:%d" % [host, port],


    // A socket description object is used by most TP proxy/agent
    // classes.  The bind or connect arguments should pass an
    // array of endpoint addresses, each in ZMQ string format.
    socket(bc, stype, addrs, hwm=1000) :: {
        // The socket type.  PUSH, SUB, etc
        type: std.asciiUpper(stype),
        // The high water mark
        hwm: hwm,
        // bind/connect endpoints
        [bc]: addrs,
    },

    // Some simplified socket makers.  (sock it to me) 
    sitm : {
        tcp :: function(bc, stype, num) $.socket(bc, stype, ['tcp://127.0.0.1:%d'%num]),
        inproc :: function(bc, stype, num) $.socket(bc, stype, ['inproc://thread%05d'%num]),
        ipc :: function(bc, stype, num) $.socket(bc, stype, ['ipc://pipe%05d.ipc'%num]),
    },


    maybe_socket(io, s) :: if std.type(s) == 'null' then {} else { [io]: { socket: s } },
    maybe_sockets(is, os) :: $.maybe_socket('input', is) + $.maybe_socket('output', os),


    //
    // Create actor configurations.  The 'type' values must be in the
    // factory method of TPAgent.
    //


    replay(name, isocket, osocket, speed=50.0, rewrite_count=0) :: {
        name: name,
        type: 'replay',
        data: {
            // Speed in hardware clock ticks per microsecond to replay the messages.
            speed: speed,
            // If nonzero then rewrite the output TPSet.count number 
            rewrite_count: rewrite_count,
        } + $.maybe_sockets(isocket, osocket),
    },

    // Create a configuration for TPSorted. 
    sorted(name, isocket, osocket, tardy, tardy_policy="drop") :: {
        name: name,
        type: 'sorted',
        data: {
            // the tardy time in ms
            tardy: tardy,
            // the tardy policy (drop vs block)
            tardy_policy: tardy_policy,
        } + $.maybe_sockets(isocket, osocket),
    },

    // Create a configuration for TPWindow
    window(name, isocket, osocket, tspan, tbuf, detid=-1, toffset=0) :: {
        name: name,
        type: 'window',
        data: {
            // The time span in data time (hardware clock) of the windowed output
            tspan: tspan,
            // Maximum buffer in data time (hardware clock) to hold TPs
            tbuf: tbuf,
            // An offset from zero to begin (mod-tspan) the windows.
            toffset: toffset,
            // Detector ID to use for output
            detid: detid,
        } + $.maybe_sockets(isocket, osocket),
    },

    // Create a configuration for TPCat
    czmqat(name, isocket=null, osocket=null, ifile=null, ofile=null, number=-1, delayus=-1) :: {
        name: name,
        type:'czmqat',
        data: {
            // an input file name
            ifile: ifile,
            // an output file name
            ofile: ofile,
            // number of messages to process, less non-positive means run forever
            number: number,
            // number of (real time) microseconds to delay before emitting next message.
            delayus: delayus,
        } + $.maybe_sockets(isocket, osocket),
    },


    // Create a configuration for TPMonitor.  
    monitor(name, filename, taps) :: {
        name: name,
        type: 'monitor',
        data: {
            // Name of monitor output file
            filename: filename,
            
            // Array of tap descriptions.  Each element must have a
            // numerical "id" as well as usual "input.socket" and
            // "output.socket" attributes.
            taps: taps,
        },
    },

    // Create a ptmper configuration
    ptmper(proxies, ttl=0, snooze=1000, pause=0, reprieve=0) :: {
        // An array of proxy configuration objects (see above)
        proxies:proxies,
        // time to live in seconds
        ttl:ttl,
        // time to snooze between waking up to check ttl (in ms)
        snooze:snooze,
        // time in seconds to sleep before creating any proxies
        pause: pause,
        // time in seconds to sleep before destroying proxies
        reprieve: reprieve,
    },

    
    // make a "tap" description
    tap(id, isock, osock) :: {
        id: id,
    } + $.maybe_sockets(isock, osock),


    // make some topologies

    files_sorted_monitored(filenames, mon_filename,
                           port_base=7000, nmsgs=100000, tspan = 50/0.02, tbuf = 5000/0.02,
                           rewrite_count=0,
                           socker = $.sitm.tcp
                          ) ::
    {
        local nfiles = std.length(filenames),
        local fiota = std.range(0, nfiles-1),
        local bnames = [$.util.basename(fn) for fn in filenames],

        local fsends = [$.czmqat("fsend-"+bnames[ind],
                                 number = nmsgs, ifile = filenames[ind],
                                 osocket = socker('bind','push', port_base + 100 + ind))
                        for ind in fiota],
        
        local replays = [$.replay("replay-"+bnames[ind], 
                                  isocket = socker('connect','pull', port_base + 100 + ind),
                                  osocket = socker('bind',   'push', port_base + 200 + ind),
                                  rewrite_count=rewrite_count)
                         for ind in fiota],

        local windows = [$.window("window-"+bnames[ind], tspan = tspan, tbuf = tbuf,
                                  isocket = socker('connect','pull', port_base + 300 + ind),
                                  osocket = socker('bind',   'push', port_base + 400 + ind))
                         for ind in fiota],

        local sorted = $.sorted("sorted", 
                                $.socket('connect', 'pull',
                                         [$.tcp(port = port_base + 500 + ind) for ind in fiota]),
                                socker('bind','push', port_base + 600),
                                tardy = 100),

        local sink = $.czmqat("sink",
                              isocket = socker('connect', 'pull', port_base + 700)),

        local taps =
            [$.tap(200 + ind,   // between replays and windows
                   socker('connect','pull', port_base + 200 + ind),
                   socker('bind',   'push', port_base + 300 + ind))
             for ind in fiota]
            +
            [$.tap(400 + ind,   // between windows and sorted
                   socker('connect','pull', port_base + 400 + ind),
                   socker('bind',   'push', port_base + 500 + ind))
             for ind in fiota]
            +
            [$.tap(600,         // between sorted and sink
                   socker('connect','pull', port_base + 600),
                   socker('bind',   'push', port_base + 700))],

        local monitor = $.monitor("monitor", mon_filename, taps),

        ret: fsends + replays + windows + [sorted, sink, monitor]
    }.ret,
}
