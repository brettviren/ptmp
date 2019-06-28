// Jsonnet support for producing data structures for PTMP

{
    // Make a TCP address from its parts
    tcp(host="127.0.0.1", port=5678) :: "tcp://%s:%d" % [host, port],


    // A socket description object is used by most TP proxy/agent
    // classes.  The bind or connect arguments should pass an
    // array of endpoint addresses, each in ZMQ string format.
    socket(stype, bc, addrs, hwm=1000) :: {
        // The socket type.  PUSH, SUB, etc
        type: std.asciiUpper(stype),
        // The high water mark
        hwm: hwm,
        // bind/connect endpoints
        [bc]: addrs,
    },

    // Simple TCP socket which only has 1 of everything.  (sock it to me) 
    sitm(bc, stype, port) :: $.socket(stype, bc, [$.tcp(port=port)]),


    maybe_socket(io, s) :: if std.type(s) == 'null' then {} else { [io]: { socket: s } },

    maybe_sockets(is, os) :: $.maybe_socket('input', is) + $.maybe_socket('output', os),


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

}
