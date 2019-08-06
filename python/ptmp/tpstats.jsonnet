local context = std.extVar('context');

local all_params = import "params.jsonnet";
local params = all_params[context];

local ptmp = import "ptmp.jsonnet";


{
    ["%s-tpstats-%s.json"%[context, apa]]: {
        name: "tpstats-apa%d-tcs"%apa,
        proxies: [ptmp.stats("tpstats-apa%d"%apa,
                             ptmp.socket('connect','sub', params.addresses.tps[apa]),
                             ptmp.socket('bind','pub', params.addresses.stats[apa]),
                             cfg = {topkey:"apa%s"%apa,
                                    link_integration_time:10000,
                                    chan_integration_time:0,
                                   }
                            )],
        
    } for apa in params.apas
} + {
    // accumulate stats to stdout
    ["%s-czmqat-stdout.json"%context]: {
        name: "stats-cat",
        proxies: [ptmp.czmqat("statscat-apa%d"%apa,
                              ptmp.socket('connect','sub', params.addresses.stats[apa]),
                              cfg={ofile:"/dev/stdout"}
                             )
                  for apa in params.apas],
    }
} + {
    // accumulate stats to graphite
    ["%s-graphite.json"%context]: {
        name: "graphite",
        proxies: [ptmp.graphite("graphite-export",
                                ptmp.socket('connect','sub',
                                            [params.addresses.stats[apa]
                                             for apa in params.apas]),
                                ptmp.socket('connect','stream',
                                            params.addresses.graphite))]
    }
        
}    
