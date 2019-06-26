{
    // Create a port structure.  A port has a name which should be
    // unique in the node.  The sock_type is the usual ZMQ name (eg
    // "PUB").  An attachments (atts) is an array of (bc,addr) pairs
    // where 'bc' is either 'bind' or 'connect' and 'addr' is a zeromq
    // address string.  If the type is omitted or atts are incomplete
    // then the port is not fully formed.
    port(name, type=null, atts=[]):: {
        name:name, type:type, atts:atts
    },
    
    // Create a node.  A node represents some unit of action (eg, an
    // application or an actor within a larger application).  A fully
    // formed node has a number of fully formed ports constructed as
    // above.
    node(name, ports=[], data={}):: {
        name: name,
        ports: ports,
        data: data,
    },


    cmdargstr(iport, iaddr=0) ::
    "-m {hwm} -p {ports[%d][type]} -a {ports[%d][atts][%d][0]} -e {ports[%d][atts][%d][1]}" %
        [ iport, iport, iaddr, iport, iaddr],
    

    // Create a file source subgraph with a node.data suitable for
    // generating a command line.  A single socket is plugged and no
    // addresses attached.
    //
    // FIXME:, currently czmqat doesn't support mixed bind/connect
    // (although zmq does).
    //
    // FIXME: this structure only supports a single endpoint (although
    // czmqat supports multiple).
    fsource(name, filename, port, clidata={}, program="czmqat", cliargs="") :: self.node(
        name,
        ports=[port],
        data = {
            app_type: "subprocess",
            cmdline: "{program} {cliargs} ifile -f {filename} osock " + $.cmdargstr(0),
            hwm: 1000,
            filename: filename,
            program: program,
            cliargs: cliargs
        } + clidata),
    

    csource(name, port, clidata={}, program="check_send_rates", cliargs="") :: self.node(
        name,
        ports=[port],
        data = {
            app_type: "subprocess",
            cmdline: "{program} {cliargs} " + $.cmdargstr(0) + " {timing} -t {time} -s {skip} ",
            timing: "uniform",
            time: 10,
            skip: 0,
            hwm: 1000,
            program: program,
            cliargs: cliargs
        } + clidata),
    
    csink(name, port, clidata={}, program="check_recv", cliargs="") :: self.node(
        name,
        ports = [port],
        data = {
            app_type: "subprocess",
            cmdline: "{program} {cliargs} " + $.cmdargstr(0),
            hwm: 1000,
            program: program,
            cliargs: cliargs
        } + clidata),

    // Create a check_* type proxy
    cproxy(name, ports, clidata={}, program='check_replay', cliargs="") :: self.node(
        name,
        ports=ports,
        data = {
            app_type: "subprocess",
            cmdline: "{program} {cliargs} input " + $.cmdargstr(0) + " output " + $.cmdargstr(1),
            hwm: 1000,
            program: program,
            cliargs: cliargs
        } + clidata),

}
