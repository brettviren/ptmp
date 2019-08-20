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

    // Build a mixin if a node has a source of metrics.  The result is
    // usually held in a "metrics" attribute of the data cfg object.
    metrics(prefix, osocket, proto="JSON", period=10000) : {
        prefix: prefix,  // deliminate a tree path with "." or "/", implicitily appended to "ptmp."  
        proto: proto,    // usually "JSON" with a converter or "GLOT" to go directly to Graphite
        period: period,  // ms
        output: {socket:osocket}, // must be STREAM if using GLOT, o.w. any sending ZMQ socket, but usually PUB.
    },

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
                    {sync_time:5000/0.02, tardy_policy:"drop"} + cfg),

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

    
    // make a "tap" description
    make_tap(id, isock, osock, name="") :: {
        id: id, layer: name,
    } + $.maybe_sockets(isock, osock),
    
    make_taps(lp) :: [
        $.make_tap(lp[0].ident(ind), lp[0].socket(ind), lp[1].socket(ind), name=lp[0].layer_name)
        for ind in lp[0].iota
        ],
    

    // make some topologies
    topo_defaults: {
        nmsgs: 100000,
        tspan: 50/0.02,
        tbuf: 5000/0.02,
        rewrite_count: 0,
        sync_time: 10,
        filenames: [], // list of file names (could be input or output)
        
        mon_filename: "foo.mon",
        tap_attach: "pushpull",

        // layer defaults
        addrpat: "inproc://thread%05d",
        port_base: 7000,
        hwm: 1000,
    },

    topo_context(cfg) :: {
        // make a layer in the topo
        layer(nam, offset, bc, stype) :: {
            // data can be overriden
            layer_name: nam,
            mult: std.length(cfg.filenames),

            iota : std.range(0, self.mult-1),

            ident(ind) :: cfg.port_base + offset + ind,
            local _id(ind) = self.ident(ind),

            name(ind) :: "%s-%d" % [ nam, _id(ind) ],

            socket(ind) :: {
                local ap = cfg.addrpat%_id(ind),
                type: std.asciiUpper(stype),
                hwm: cfg.hwm,
                [bc]: [ap],
            },

            socket_maddrs(seq = self.iota) :: {
                type: std.asciiUpper(stype),
                hwm: cfg.hwm,
                [bc]: [cfg.addrpat%_id(ind) for ind in seq],
            },

        },
        
        fsenders(layer) :: [
            $.czmqat(layer.name(ind), osocket = layer.socket(ind),
                     cfg=cfg+ {ifile: cfg.filenames[ind], number: cfg.nmsgs})
            for ind in layer.iota],


        replays(ilayer, olayer) :: [
            $.replay(ilayer.name(ind),
                     isocket = ilayer.socket(ind),
                     osocket = olayer.socket(ind),
                     cfg = cfg)
            for ind in ilayer.iota],


        windows(ilayer,olayer) :: [
            $.window(ilayer.name(ind), 
                     isocket = ilayer.socket(ind),
                     osocket = olayer.socket(ind),
                     cfg=cfg)
            for ind in ilayer.iota],

        
        zipper(ilayer,olayer) :: [
            $.zipper(ilayer.layer_name,
                     isocket = ilayer.socket_maddrs(),
                     osocket = ilayer.socket(0),
                     cfg = cfg)],

        sinks(ilayer) :: [
            $.czmqat(ilayer.name(ind), isocket = ilayer.socket(ind))
            for ind in ilayer.iota],


        // cfg={filename:null, taps:[], tap_attach:"pushpull"}) :: {
        monitor(layer_pairs) :: $.monitorz("monitor", 
                                           cfg.mon_filename,
                                           std.flattenArrays([ $.make_taps(lp) for lp in layer_pairs ]),
                                           cfg.tap_attach),
        
    },                          // topo_context

    monitor(ctx):: {

        files: {

            input: ctx.fsenders(ctx.layer('files', 100, 'bind', 'push')),
            
            raw: self.input
                + ctx.sinks(     ctx.layer('sink',   200, 'connect', 'pull'))
                + [ ctx.monitor([[ctx.layer('fmonin', 100, 'connect', 'pull'),
                                  ctx.layer('fmonout',200, 'bind',    'push')]]) ],
        
            replay: self.input
                + ctx.replays(   ctx.layer('replayin',  100, 'connect', 'pull'),
                                 ctx.layer('replayout', 200, 'bind',    'pub'))
                + ctx.sinks(     ctx.layer('sink',      300, 'connect', 'pub'))
                + [ ctx.monitor([[ctx.layer('fmonin',    200, 'connect', 'sub'),
                                  ctx.layer('fmonout',   300, 'bind',    'pub')]]) ],

            window: self.input
                + ctx.replays(ctx.layer('replayin',100, 'connect', 'pull'),
                              ctx.layer('replayout',200, 'bind', 'pub'))
                + ctx.windows(ctx.layer('windowin',200, 'connect', 'sub'),
                              ctx.layer('windowout',300, 'bind', 'push'))
                + ctx.sinks(   ctx.layer('sink',  400, 'connect', 'pub'))
                + [ ctx.monitor([[ctx.layer('fmonin',300,'connect','pull'),
                                 ctx.layer('fmonout',400,'bind','pub')]]) ],

            zipper: self.input
                + ctx.replays(ctx.layer('replayin',100, 'connect', 'pull'),
                              ctx.layer('replayout',200, 'bind', 'pub'))
                + ctx.windows(ctx.layer('windowin',200, 'connect', 'sub'),
                              ctx.layer('windowout',300, 'bind', 'push'))
                + ctx.zipper(ctx.layer('zipperin',300, 'connect', 'pull'),
                             ctx.layer('zipperout',400,'bind', 'pub'))
                + ctx.sinks(   ctx.layer('sink',  500, 'connect', 'sub'))
                + [ ctx.monitor([[ctx.layer('fmonin',400,'connect','sub'),
                                 ctx.layer('fmonout',500,'bind','pub')]]) ],
        },

        streams: self.files { input: ctx.sinput() },
    },

    // eventually for online use
    online(ctx) :: {

        streams: {
            input: ctx.sinput(),

            zipper: self.input
                + ctx.replays(ctx.layer('replayin',100, 'connect', 'pull'),
                              ctx.layer('replayout',200, 'bind', 'pub'))
                + ctx.windows(ctx.layer('windowin',200, 'connect', 'sub'),
                              ctx.layer('windowout',300, 'bind', 'push'))
                + ctx.zipper(ctx.layer('zipperin',300, 'connect', 'pull'),
                             ctx.layer('zipperout',400,'bind', 'pub'))
        }
    },
}
