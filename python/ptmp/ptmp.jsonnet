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


    // A socket description object is used by most TP proxy/agent
    // classes.  The bind or connect arguments should pass an
    // array of endpoint addresses, each in ZMQ string format.
    socket(bc, stype, addrs, hwm=1000) :: {
        local laddrs = if std.type(addrs) == 'array' then addrs else [addrs],
        // The socket type.  PUSH, SUB, etc
        type: std.asciiUpper(stype),
        // The high water mark
        hwm: hwm,
        // bind/connect endpoints
        [bc]: laddrs,
    },

    // Some simplified socket makers.  (sock it to me) 
    // sitm : {
    //     tcp :: function(bc, stype, num) $.socket(bc, stype, ['tcp://127.0.0.1:%d'%num]),
    //     inproc :: function(bc, stype, num) $.socket(bc, stype, ['inproc://thread%05d'%num]),
    //     ipc :: function(bc, stype, num) $.socket(bc, stype, ['ipc://pipe%05d.ipc'%num]),
    // },

    maybe_socket(io, s) :: if std.type(s) == 'null' then {} else { [io]: { socket: s } },
    maybe_sockets(is, os) :: $.maybe_socket('input', is) + $.maybe_socket('output', os),


    // Build a mixin to describe a node with an auxiliary socket
    // providing a source of metrics.  The result is usually held in a
    // "metrics" attribute of the node data cfg object.
    metrics(prefix, osocket, proto="JSON", period=10000) : {
        prefix: prefix,  // deliminate a tree path with "." or "/", implicitily appended to "ptmp."  
        proto: proto,    // usually "JSON" with a converter or "GLOT" to go directly to Graphite
        period: period,  // ms
        socket:osocket,  // must be STREAM if using GLOT, o.w. any sending ZMQ socket, but usually PUB.
    },

    //
    // Create actor configurations.  The 'type' values must be what is
    // used to register a factory method.
    //

    nodeconfig(type, name, isocket, osocket, cfg) :: {
        name: name,
        type: type,
        data: cfg + $.maybe_sockets(isocket, osocket),
    },

    replay(name, isocket, osocket, cfg={})
    :: $.nodeconfig('replay', name, isocket, osocket,
                    {speed:50.0, rewrite_count:0, rewrite_tstart:0} + cfg),

    // Create a configuration for TPZipper. 
    zipper(name, isocket, osocket, cfg={})
    :: $.nodeconfig('zipper', name, isocket, osocket,
                    {nap_time: 100, sync_time:5000/0.02, tardy_policy:"drop"} + cfg),

    // Create a configuration for TPWindow
    window(name, isocket, osocket, cfg={})
    :: $.nodeconfig('window', name, isocket, osocket,
                    {tspan:50/0.02, tbuf:5000/0.02, detid:-1, toffset:0} + cfg),

    // Create a configuration for TPCat
    czmqat(name, isocket=null, osocket=null, cfg={})
    :: $.nodeconfig('czmqat', name, isocket, osocket,
                    {ifile:null, ofile:null, number:-1, delayus:-1} + cfg),

    // Create a configuration for TPStats
    stats(name, isocket=null, osocket=null, cfg={})
    :: $.nodeconfig('stats', name, isocket, osocket,
                    {link_integration_time:1000,
                     chan_integration_time:10000,
                     topkey:"tpsets",
                     tick_per_us:50, tick_per_off:0
                    }+cfg),



    // stats->graphite adapter
    graphite(name, isocket=null, osocket=null, cfg={})
    :: $.nodeconfig('stats_graphite', name, isocket, osocket, cfg),

    // Create a configuration for TPMonitor.  It doesn't quite fit the nodeconfig() pattern
    monitorz(name, filename=null, taps=[], tap_attach="pushpull", cfg={}) :: {
        name: name,
        type: 'monitor',
        data: cfg + {
            // Name of monitor output file
            filename: filename,
            
            // Array of tap descriptions.  Each element must have a
            // numerical "id" as well as usual "input.socket" and
            // "output.socket" attributes.
            taps: taps,

            attach: tap_attach,            
        } 
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

    
}
