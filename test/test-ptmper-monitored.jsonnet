local input = std.extVar("input");
local output = std.extVar("output");
local ptmp = import "ptmp.jsonnet";

local components = ptmp.files_sorted_monitored(input, output);

{
    "test-ptmper-monitored.json" : ptmp.ptmper(components, ttl=10, reprieve=1),
}

