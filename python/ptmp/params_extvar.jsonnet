// A high level configuration relevant to running on hierocles for testing

local pdsp_params = import "params_pdsp.jsonnet";

pdsp_params {

    cfg: super.cfg {
        // Want non-rediculous latency measurements for replayed data.
        rewrite_tstart: 1,
        link_integration_time:20000,
        chan_integration_time:20000,

        tspan: 50/0.02,         // window span in data time

    },

    // index by APA number.  There is no APA 0.
    local infiles=std.extVar("infiles"),

    apas: std.prune([
        if std.type(infiles[apa]) == "null"
        then null
        else apa
        for apa in std.range(0,6)]),

    // For fileplay, need to provide some files.  This default is for
    // hierocles.  Override this file if some other host is to be
    // supported.
    infiles: infiles,

    outfiles: {
        tps: "test-tps.dump",
        tcs: "test-tcs.dump",
        tds: "test-tds.dump",
    },

    // The PUB/SUB socket addresses
    addresses : {
        tps: [
            if std.type(infiles[apa]) == "null"
            then [null]
            else
                ["tcp://127.0.0.1:%d" % (7000+100*apa+link) for link in std.range(0,std.length(infiles[apa]))]
            for apa in std.range(0,6)],
        
        tc: [
            if std.type(infiles[apa]) == "null"
            then null
            else "tcp://127.0.0.1:%d" %(7700 + apa) for apa in std.range(0, 6)],
        td: "tcp://127.0.0.1:7999",

        stats: ["tcp://127.0.0.1:%d" %(6660 + apa) for apa in std.range(0,6)],
        // really it's "carbon" that receives the data.
        graphite: "tcp://127.0.0.1:2003",

    }
}
