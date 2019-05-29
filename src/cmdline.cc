#include "ptmp/cmdline.h"

void ptmp::cmdline::add_socket_options(CLI::App* app, ptmp::cmdline::sock_options_t& opt)
{
    app->add_option("-m,--socket-hwm", opt.hwm,
                    "The ZeroMQ socket highwater mark");    
    app->add_option("-p,--socket-pattern", opt.socktype,
                    "The ZeroMQ socket pattern for endpoint [PUB, PAIR, PUSH]");
    app->add_option("-a,--socket-attachment", opt.attach,
                    "The socket attachement method [bind|connect] for this endpoint");
    app->add_option("-e,--socket-endpoints", opt.endpoints,
                    "The socket endpoint addresses in Zero MQ format (tcp:// or ipc://)")->expected(-1);
}

using json = nlohmann::json;

json ptmp::cmdline::to_json(ptmp::cmdline::sock_options_t& opt)
{
    json jsock = json::object();
    jsock["hwm"] = opt.hwm;
    jsock["type"] = opt.socktype;
    json eps = json::array();
    for (auto ep : opt.endpoints) {
        eps.push_back(ep);
    }
    jsock[opt.attach] = eps;
    json jcfg;
    jcfg["socket"] = jsock;
    return jcfg;
}

