// Jsonnet support for producing data structures for PTMP

{
    // Functions and structure relevant to the PTMP C++ api
    api : {

        // Make a TCP address from its parts
        tcp(host="127.0.0.1", port=5678) :: "tcp://%s:%d" % [host, port],

        // A socket description object is used by most TP proxy/agent
        // classes.  The bind or connect arguments should pass an
        // array of endpoint addresses, each in ZMQ string format.
        socket(stype, bind=[], connect=[], hwm=1000) :: {
            // The socket type.  PUSH, SUB, etc
            type: std.asciiUpper(stype),
            // The high water mark
            hwm: hwm,
            // Any addresses to bind
            bind: bind,
            // Any addresses to connect
            connect: connect
        },

        // Create a configuration for TPSorted. 
        sorted(isockets, osockets, tardy, tardy_policy="drop") :: {
            type: 'sorted',
            // an array of input sockets
            input: { socket: isockets },
            // an array of output sockets
            output: { socket: osockets },
            // the tardy time in ms
            tardy: tardy,
            // the tardy policy (drop vs block)
            tardy_policy: tardy_policy,
        },

        // Create a configuration for TPWindow
        window(isockets, osockets, tspan, tbuf, detid=-1, toffset=0) :: {
            type: 'window',
            // an array of input sockets
            input: { socket: isockets },
            // an array of output sockets
            output: { socket: osockets },
            // The time span in data time (hardware clock) of the windowed output
            tspan: tspan,
            // Maximum buffer in data time (hardware clock) to hold TPs
            tbuf: tbuf,
            // An offset from zero to begin (mod-tspan) the windows.
            toffset: toffset,
            // Detector ID to use for output
            detid: detid,
        },

        // Create a configuration for TPCat
        czmqat(isockets=[], osockets=[], ifile=null, ofile=null, number=-1, delayus=-1) :: {
            type:'czmqat',
            // an array of input sockets
            input: { socket: isockets },
            // an array of output sockets
            output: { socket: osockets },
            // an input file name
            ifile: ifile,
            // an output file name
            ofile: ofile,
            // number of messages to process, less non-positive means run forever
            number: number,
            // number of (real time) microseconds to delay before emitting next message.
            delayus: delayus,
        },

    },

    cli : {
        ptmper(proxies, ttl=0, snooze=1000, pause=0, reprieve=0) :: {
            // An array of proxy configuration objects (see ptmp.api. above)
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
    }
}
