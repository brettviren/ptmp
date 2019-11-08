// utility functions to contruct ptmper configuration
local ptmp = import "ptmp.jsonnet";
{
    // layer generators
    laygen : {

        // Generate config for ptmper proxies that will replay the given
        // set of file streams.  Intermediate IPC transport addresses will
        // be made unique via inclusion of "detid" which is some string
        // identifying an APA.  Streams are objects giving filename, link
        // number and output ZeroMQ address.
        fileplay :: function(unit) {

            // internal link
            local inproc = function(stream)
            "inproc://%s-link%02d" % [ unit.name, stream.link ],

            local aname = function(thing, stream)
            "%s-%s-link%02d" % [ thing, unit.name, stream.link ],

            local czmqats = [
                ptmp.czmqat(
                    aname("czmqat", stream),
                    osocket = ptmp.socket('bind','push', inproc(stream)),
                    cfg = stream.file_cfg)
                for stream in unit.streams],

            local replays = [
                ptmp.replay(
                    aname("replay", stream),
                    isocket = ptmp.socket('connect','pull',inproc(stream)),
                    osocket = ptmp.socket('bind','pub', stream.output),
                    cfg = stream.replay_cfg)
                for stream in unit.streams],

            res : czmqats + replays
        }.res,

        // Generate config for applying windows and a zipper to a set of
        // streams.  The unit object has detid, streams, output.
        winzip :: function(unit) {

            local inproc = function(stream)
            "inproc://%s-link%02d" % [ unit.name, stream.link ],
            local aname = function(thing, stream)
            "%s-%s-link%02d" % [ thing, unit.name, stream.link ],

            local windows = [
                ptmp.window(
                    aname("window", stream),
                    isocket = ptmp.socket('connect','sub',stream.input),
                    osocket = ptmp.socket('bind','pub',inproc(stream)),
                    cfg = stream.window_cfg) for stream in unit.streams],

            local zipper = ptmp.zipper(
                unit.name,
                isocket = ptmp.socket('connect','sub', [inproc(stream) for stream in unit.streams]),
                osocket = ptmp.socket('bind','pub', unit.output),
                cfg = unit.zipper_cfg),

            res: windows + [zipper]
        }.res,

        // A layer which runs a filter algorithm
        filtalg :: function(unit) [
            ptmp.nodeconfig("filter",
                            "filter-%s"%unit.detid, // fixme: pass in a unit unit name
                            isocket = ptmp.socket('connect','sub', unit.input),
                            osocket = ptmp.socket('bind','pub', unit.output),
                            cfg = unit.cfg)            
        ],

        zip :: function(unit) [
            ptmp.zipper(
                unit.name,
                isocket = ptmp.socket('connect','sub', [stream.input for stream in unit.streams]),
                osocket = ptmp.socket('bind','pub', unit.output),
                cfg = unit.cfg)
        ],
                

        sink :: function(unit) [
            ptmp.czmqat(unit.name, // fixme: generally should have streams input
                        isocket = ptmp.socket('connect','sub', unit.input),
                        cfg=unit.cfg)
        ],

    },


    local cfgfile = function(layer, name) "%s-%s.json" % [layer, name],
    cfggen :: function(layers) std.foldl(function(a,b) a+b, [
        {
            [cfgfile(l.layer, u.detid)]: {
                name: "%s-%s"%[l.layer, u.detid],
                proxies: $.laygen[l.layer](u),
                
            } for u in l.units
        } for l in layers], {}),



    procfile : function (layer) 
    std.join('\n', ["%s-%s: ptmper %s" % [ layer.layer, u.detid, cfgfile(layer.layer, u.detid)]
                    for u in layer.units]),
    procgen :: function(layers) std.join('\n', [$.procfile(layer) for layer in layers]),

}
