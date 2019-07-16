/**
Configure "ptmper" instances to run "adjacency" triggering.  

The word "adjacency" refers to the use of the triggering algorithms
from ProtoDuneTrigger packge which is based on finding adjacent
trigger primitives to form trigger candidates and then stitch these
into trigger decisions in order to attempt to trigger on horizontal
muons.

The overall topology is a fan-in and the the configuration is
abstracted into numbered "layers" receiving or sending data and that
data is fragmented over a number of sockets in that layer.  Each layer
connects to its neighbors either via network message transmission or
internally through an application.

The layers are:

0. PUB sockets provided by the FELIX_BR hit finder and producing
trigger primitives.

1. SUB sockets provided by ptmper hosting message handling and trigger
candidate algorithm.

2. PUB sockets provided by the above applications producing trigger
candidates.

3. SUB sockets provided by ptmper hosting message handling and module
level trigger algorithm.

4. PUB socket providing final trigger decision.

Note, components in both ptmp and ptmp-tcs plugins are needed and thus
their libraries must be found via the user's LD_LIBRARY_PATH.  

*/

// A number of meta configurations are defined and switched based on a
// single input 'context' (eg "test", "prod").
local context = std.extVar('context');
local params_context = {
    prod: { // bogus for now, waiting on info
        pubsub_hwm: 10000,      // high-water mark for pub/sub sockets
        tspan: 50/0.02,         // window span in data time
        tbuf: 5000/0.02,        // turn around buffer in data time
        toffset: 0,
        napas: 6,               // number of APAs to span
        nlinks: 10,             // number of links per APA
        sync_time: 10, // real time in ms to wait for tardy input streams
        detid: -1,              // use what's in TPSet
        tardy_policy:"drop",    // anything else destroys output ordering

        // derived
        local link_iota = std.range(0, self.nlinks-1),

        // Return an address for a PUB or SUB socket
        addresses : {           // this is made up, waiting for input
            tps:: function(apa) ["tcp://tphost-apa%d:%d" % [ apa, 7000+100*apa+link]
                                 for link in link_iota],
            tc:: function(apa) "tcp://tchost-apa%d:%d"%[apa,7700+apa],
            td:: "tcp://tdhost:7999",
        }

    },
    test: self.prod {           // override some things for local testing
        local link_iota = std.range(0, self.nlinks-1),

        napas: 1,
        addresses : {
            tps:: function(apa) ["tcp://127.0.0.1:%d" % (7000+100*apa+link)
                                 for link in link_iota],
            tc:: function(apa) "tcp://127.0.0.1:%d"%(7700+apa),
            td:: "tcp://127.0.0.1:7999",
        }
    },
};
local params = params_context[context];


local ptmp = import "ptmp.jsonnet";


local make_tc = function(apa, inputs, output, params) {
    local iota = std.range(0, std.length(inputs)-1),
    local inproc_wz = ["inproc://tc-window-zipper-apa%d-link%02d"%[apa,link] for link in iota],
    local windows = [ptmp.window('tc-window-apa%d-link%02d'%[apa, link],
                                 isocket = ptmp.socket('connect', 'sub', inputs[link],
                                                       hwm = params.pubsub_hwm),
                                 osocket = ptmp.socket('bind', 'push', inproc_wz[link]),
                                 cfg = params)
                     for link in iota],
    local inproc_ztc = "inproc://tc-zipper-tcfinder-apa%d" % apa,
    local zipper = ptmp.zipper("tc-zipper-apa%d"%apa,
                               isocket = ptmp.socket('connect','pull', inproc_wz),
                               osocket = ptmp.socket('bind','push', inproc_ztc),
                               cfg = params),

    local tcfinder = ptmp.nodeconfig("filter", "tc-finder-apa%d"%apa,
                                     isocket = ptmp.socket('connect','pull', inproc_ztc),
                                     osocket = ptmp.socket('bind','pub', output,
                                                           hwm = params.pubsub_hwm),
                                     cfg = {method: "pdune-adjacency-tc"}),
    components: windows + [zipper, tcfinder],
}.components;

local make_td = function(inputs, output, params) {
    local inproc_ztd = "inproc://td-zipper-tdfinder",
    local zipper = ptmp.zipper("td-zipper",
                               isocket = ptmp.socket('connect', 'sub', inputs,
                                                     hwm = params.pubsub_hwm),
                               osocket = ptmp.socket('bind','push', inproc_ztd),
                               cfg = params),

    local tdfinder = ptmp.nodeconfig("filter", "td-finder",
                                     isocket = ptmp.socket('connect','pull', inproc_ztd),
                                     osocket = ptmp.socket('bind','pub', output,
                                                           hwm = params.pubsub_hwm),
                                     cfg = {method: "pdune-adjacency-td"}),
    components: [zipper, tdfinder],
}.components;


{
    ["%s-tc-apa%d.json"%[context, apa]]: {
        plugins: ["ptmp-tcs"],
        proxies: make_tc(apa, params.addresses.tps(apa), params.addresses.tc(apa), params)
    } for apa in std.range(0, params.napas-1)
} + {
    ["%s-td.json"%context]: {
        plugins: ["ptmp-tcs"],
        proxies: make_td([params.addresses.tc(apa) for apa in std.range(0, params.napas-1)],
                         params.addresses.td, params)
    }
}
