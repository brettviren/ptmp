local input = std.extVar("input");
local output = std.extVar("output");
local ptmp = import "ptmp.jsonnet";

local components = ptmp.files_sorted_monitored(input, output, nmsgs=200000, rewrite_count=1);

{
    "test-ptmper-monitored.json" : ptmp.ptmper(components, ttl=30, reprieve=1),
}

