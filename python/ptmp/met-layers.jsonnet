// local p = import "ptmper.jsonnet";

//local datapat="/data/fast/bviren/ptmp-dumps/2019-11-06/run10230-felix%(apa)d%(link)02d.dump";
local datapat="/data/fast/bviren/ptmp-dumps/2019-07-19/FELIX_BR_%(apa)d%(link)02d.dump";
local links = [1,3,5,7,9];
local apas = [5, 6];

local link_tp_wz    = "tcp://127.0.0.1:50%(apa)d%(link)d";
local link_wz_tc    = "tcp://127.0.0.1:51%(apa)d0";
local link_tc_zip   = "tcp://127.0.0.1:52%(apa)d0";
local link_zip_dfo  = "tcp://127.0.0.1:5300";

[
    {
        layer: "fileplay",
        units : [ {
            detid: "apa%d"%apa,
            name: "src-apa%d" % apa,
            streams: [{
                link: link,
                output: link_tp_wz % {apa:apa, link:link},
                file_cfg: {
                    name: "file-apa%d-link%d" % [apa, link],
                    ifile: datapat % {apa:apa, link:link}
                },
                replay_cfg: {
                    name: "replay-apa%d-link%d"%[apa, link],
                },
            } for link in links],
        } for apa in apas],
    },
    {
        layer: "winzip",
        units: [{
            detid: "apa%d"%apa,
            name: "winzip-apa%d" % apa,
            streams: [{
                link:link,
                input: link_tp_wz % {apa:apa, link:link},
                window_cfg: {
                    name: 'win-apa%d-link%d'%[apa, link]
                },
            } for link in links],
            zipper_cfg: { name: 'zip-apa%d'%apa },
            output: link_wz_tc % {apa:apa},
        } for apa in apas],
    },
    {
        layer: "filtalg",
        units: [{
            detid: "apa%d"%apa,
            name: "metc-apa%d"%apa,
            input: link_wz_tc % {apa:apa},
            output: link_tc_zip % {apa:apa},
            cfg: {
                engine:"met_michel_tc",
            }
        } for apa in apas],
    },
    {
        layer: "zip",
        units: [{
            detid: "pdsp",
            name: "mlt",
            streams: [{
                input: link_tc_zip % {apa:apa}
            } for apa in apas],
            output: link_zip_dfo,
            cfg: {name:"mlt-zipper"}
        }],
    },

    {
        layer: "sink",
        units: [{
            detid: "pdsp",
            name: "dfo",
            input: link_zip_dfo,
            cfg: {name:"dfo-sink"}
        }],
    },
            
]


// p.fileplay_files(units)

