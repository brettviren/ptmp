// micro plug in factory

#ifndef upif_h_seen
#define upif_h_seen

#include <dlfcn.h>
#include <string>
#include <unordered_map>

namespace upif {

    // fixme: this class should probably go someplace more generic.
    template <class T>
    class Singleton
    {
    public:
	static T& Instance() {
	    static T instance;
	    return instance;
	}

    private:
	Singleton(){}
	~Singleton(){}
	Singleton(Singleton const&){}
	Singleton& operator=(Singleton const&){}
    };

    // Turn a library symbol name into symbol.  Symbol must be copyable.
    class Plugin {
        void* m_lib;
    public:
        Plugin(void* lib): m_lib(lib) {}
        ~Plugin() { dlclose(m_lib); }

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

    // Plugin manager cache lookup from plugin name to plugin object
    class PluginManager {
	std::unordered_map<std::string, Plugin*> m_plugins;
    public:
        ~PluginManager() {
            for (auto pit : m_plugins) {
                delete pit.second;
                pit.second = nullptr;
            }
        }
	Plugin* add(const std::string& plugin_name, const std::string& libname = "") {
            Plugin* pi = get(plugin_name);
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
                    Plugin* pl = new Plugin(lib);
                    m_plugins[plugin_name] = pl;
                    return pl;
                }
            }
            return nullptr;
        }

        Plugin* get(const std::string& plugin_name) {
            auto pit = m_plugins.find(plugin_name);
            if (pit == m_plugins.end()) {
                return nullptr;
            }
            return pit->second;
        }
        Plugin* find(const std::string& symbol_name) {
            // zsys_debug("finding in %ld plugins", m_plugins.size());
            for (auto pit : m_plugins) {
                // zsys_debug("checking plugin \"%s\"", pit.first.c_str());
                Plugin* maybe = pit.second;
                if (maybe->rawsym(symbol_name)) {
                    return maybe;
                }
            }
            return nullptr;
        }
    };


    inline
    PluginManager& plugins() {
        return Singleton<upif::PluginManager>::Instance();
    }

}

#endif
