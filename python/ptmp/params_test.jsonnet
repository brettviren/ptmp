// A high level configuration relevant to running on hierocles for testing

local pdsp_params = import "params_pdsp.jsonnet";

pdsp_params {

    cfg: super.cfg {
        // force replayed tpsets to get recent past tstarts based on
        // current real time.  Note, this will almost certainly mess
        // up inter-message sync based on this tstart!
        rewrite_tstart: 1,
    },

    apas: [5],

    // For fileplay, need to provide some files.  This default is for
    // hierocles.  Override this file if some other host is to be
    // supported.
    infiles: [
        null,null,null,null,null,
        ["/data/fast/bviren/ptmp-dumps/2019-07-05/FELIX_BR_5%02d.dump" % link
         for link in std.range(1,10)],
        null,
    ],

    outfiles: {
        tps: "test-tps.dump",
        tcs: "test-tcs.dump",
        tds: "test-tds.dump",
    },

    // The PUB/SUB socket addresses
    addresses : {
        tps: [[null]] + [
            ["tcp://127.0.0.1:%d" % (7000+100*apa+link) for link in std.range(0,9)]
                         for apa in std.range(1,6)],
        
        tc: ["tcp://127.0.0.1:%d" %(7700 + apa) for apa in std.range(1,6)],
        td: "tcp://127.0.0.1:7999",
        stats: ["tcp://127.0.0.1:%d" %(6660 + apa) for apa in std.range(0,6)],
        // really it's "carbon" that receives the data.
        graphite: "tcp://127.0.0.1:2003",
    }
}
