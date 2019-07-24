// micro plug in factory

#ifndef upif_h_seen
#define upif_h_seen

#include <dlfcn.h>
#include <string>
#include <unordered_map>

namespace upif {

    // Turn a library symbol name into symbol.  Symbol must be copyable.
    class plugin {
        void* m_lib;
    public:
        plugin(void* lib): m_lib(lib) {}
        ~plugin() { dlclose(m_lib); }

        void* rawsym(const std::string& symbol_name) {
            return dlsym(m_lib, symbol_name.c_str());
        }

	template<typename T>
	bool symbol(const std::string& symbol_name, T& ret) {
	    void* thing = rawsym(symbol_name);
	    if (!thing) { return false; } 
	    ret = reinterpret_cast<T>(thing);
	    return true;
	}        
    };

    // Cache lookup from plugin name to plugin object
    class cache {
	std::unordered_map<std::string, plugin*> m_plugins;
    public:
        ~cache() {
            for (auto pit : m_plugins) {
                delete pit.second;
                pit.second = nullptr;
            }
        }
	plugin* add(const std::string& plugin_name, const std::string& libname = "") {
            plugin* pi = get(plugin_name);
            if (pi) { return pi; }
            std::string exts[2] = {".so",".dylib"};
            for (int ind=0; ind<2; ++ind) {
                std::string ext = exts[ind];
                std::string lname = "";
                if (libname == "") {
                    lname = "lib";
                    lname += plugin_name;
                    lname += ext;
                }
                else {
                    lname = libname;
                }

                void* lib = dlopen(lname.c_str(), RTLD_NOW);
                if (lib) {
                    plugin* pl = new plugin(lib);
                    m_plugins[plugin_name] = pl;
                    return pl;
                }
            }
            return nullptr;
        }

        plugin* get(const std::string& plugin_name) {
            auto pit = m_plugins.find(plugin_name);
            if (pit == m_plugins.end()) {
                return nullptr;
            }
            return pit->second;
        }
        plugin* find(const std::string& symbol_name) {
            // zsys_debug("finding in %ld plugins", m_plugins.size());
            for (auto pit : m_plugins) {
                // zsys_debug("checking plugin \"%s\"", pit.first.c_str());
                plugin* maybe = pit.second;
                if (maybe->rawsym(symbol_name)) {
                    return maybe;
                }
            }
            return nullptr;
        }
    };

}

#endif
