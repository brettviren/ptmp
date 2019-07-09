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

        // describe a parallel layer of cnum nodes with an input and output base numbers.
        parlayer :: function(ibase=0, obase=0, cnum=0, name="") {
            ibase:ibase, obase:obase, cnum:cnum, name:name
        },
        
    },


    // Make a TCP address from its parts
    tcp(host="127.0.0.1", port=5678) :: "tcp://%s:%d" % [host, port],


    // A socket description object is used by most TP proxy/agent
    // classes.  The bind or connect arguments should pass an
    // array of endpoint addresses, each in ZMQ string format.
    socket(bc, stype, addrs, hwm=10000) :: {
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

    // Create a configuration for TPZipper. 
    zipper(name, isocket, osocket, sync_time, tardy_policy="drop") :: {
        name: name,
        type: 'zipper',
        data: {
            // the sync time in ms
            sync_time: sync_time,
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
    monitor(name, filename, taps, tap_attach="pushpull") :: {
        name: name,
        type: 'monitor',
        data: {
            // Name of monitor output file
            filename: filename,
            
            // Array of tap descriptions.  Each element must have a
            // numerical "id" as well as usual "input.socket" and
            // "output.socket" attributes.
            taps: taps,

            attach: tap_attach,
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
    tap(id, isock, osock, layer="") :: {
        id: id, layer: layer,
    } + $.maybe_sockets(isock, osock),


    // make some topologies
    default_topo_config: {
        nmsgs: 100000,
        tspan: 50/0.02,
        tbuf: 5000/0.02,
        rewrite_count: 0,
        socker: $.sitm.tcp,
        port_base: 7000,
        sync_time: 10,
        filenames: [],
        mon_filename: "foo.mon",
        tap_attach: "pushpull",
    },

    component_makers(cfg) :: {
        local name(ind) = $.util.basename(cfg.filenames[ind]),
        local iota(num) = if num == 0 then std.range(0, std.length(cfg.filenames)-1) else std.range(0,num-1),
        fsenders(layer) :: [
            $.czmqat("fsend-"+name(ind),
                     number = cfg.nmsgs, ifile = cfg.filenames[ind],
                     osocket = cfg.socker('bind','push', cfg.port_base + layer.obase + ind))
            for ind in iota(layer.cnum)],


        replays(layer) :: [
            $.replay("replay-"+name(ind), 
                     isocket = cfg.socker('connect','pull', cfg.port_base + layer.ibase + ind),
                     osocket = cfg.socker('bind',   'push', cfg.port_base + layer.obase + ind),
                     rewrite_count=cfg.rewrite_count)
            for ind in iota(layer.cnum)],


        windows(layer) :: [
            $.window("window-"+name(ind), tspan = cfg.tspan, tbuf = cfg.tbuf,
                     isocket = cfg.socker('connect','pull', cfg.port_base + layer.ibase + ind),
                     osocket = cfg.socker('bind',   'push', cfg.port_base + layer.obase + ind))
            for ind in iota(layer.cnum)],


        zipper(layer) :: {
            local isocks = [cfg.socker('connect','pull', cfg.port_base + layer.ibase + ind)
                            for ind in iota(layer.cnum)],
            local addrs = [s.connect for s in isocks],
            ret: [$.zipper("zipper", 
                           isocket = $.socket('connect', 'pull', addrs),
                           osocket = cfg.socker( 'bind', 'push', cfg.port_base + layer.obase),
                           sync_time = cfg.sync_time)],
        }.ret,

        sinks(layer) :: [
            $.czmqat("sink-"+name(ind),
                     isocket = cfg.socker('connect', 'pull', cfg.port_base + layer.ibase + ind))
            for ind in iota(layer.cnum)],

        local tap_layer(layer) = [
            $.tap(layer.ibase + ind,
                  cfg.socker('connect','pull', cfg.port_base + layer.ibase + ind),
                  cfg.socker('bind',   'push', cfg.port_base + layer.obase + ind),
                  layer=layer.name)
            for ind in iota(layer.cnum)],
            
        monitor(layers) :: $.monitor("monitor", cfg.mon_filename,
                                     std.flattenArrays([ tap_layer(layer) for layer in layers]),
                                     cfg.tap_attach),

    },

    just_files(cfg=$.default_topo_config) :: {
        local cm = $.component_makers(cfg),
        local layer = $.util.parlayer,
        ret: []
            + cm.fsenders(layer(obase=100))
            + cm.sinks(layer(ibase=200))
            + [ cm.monitor([ layer(100,200,name="files") ])],
    }.ret,
        
    just_replay(cfg=$.default_topo_config) :: {
        local cm = $.component_makers(cfg),
        local layer = $.util.parlayer,
        ret: []
            + cm.fsenders(layer(obase=100))
            + cm.replays(layer(ibase=100,obase=200))
            + cm.sinks(layer(ibase=300))
            + [ cm.monitor([ layer(200,300,name="replays") ])],
    }.ret,

    just_window(cfg=$.default_topo_config) :: {
        local cm = $.component_makers(cfg),
        local layer = $.util.parlayer,
        ret: []
            + cm.fsenders(layer(obase=100))
            + cm.replays(layer(ibase=100,obase=200))
            + cm.windows(layer(ibase=200,obase=300))
            + cm.sinks(layer(ibase=400))
            + [ cm.monitor([ layer(300,400,name="windows") ])],
    }.ret,

    just_zipper(cfg=$.default_topo_config) :: {
        local cm = $.component_makers(cfg),
        local layer = $.util.parlayer,
        ret: []
            + cm.fsenders(layer(obase=100))
            + cm.replays(layer(ibase=100,obase=200))
            + cm.windows(layer(ibase=200,obase=300))
            + cm.zipper(layer(ibase=300,obase=400))
            + cm.sinks(layer(ibase=500))
            + [ cm.monitor([ layer(400,500,name="zipper") ])],
    }.ret,

}
