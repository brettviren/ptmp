// micro plug in factor

#ifndef upif_h_seen
#define upif_h_seen

#include <dlfcn.h>
#include <string>
#include <unordered_map>

namespace upif {

    template <class T>
    class singleton {
    public:
        static T& instance() {
            static T m_instance;
            return m_instance;
        }

    private:
        singleton(){}
        ~singleton(){}
        singleton(singleton const&){}
        singleton& operator=(singleton const&){}
    };

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
            for (auto pit : m_plugins) {
                plugin* maybe = pit.second;
                if (maybe->rawsym(symbol_name)) {
                    return maybe;
                }
            }
            return nullptr;
        }
    };


    // Singleton access

    inline
    plugin* add(const std::string& plugin_name, const std::string& libname = "")
    {
        auto& c = singleton<cache>::instance();
        return c.add(plugin_name, libname);
    }

    inline
    void* rawsym(const std::string& symbol_name) {
        auto& c = singleton<cache>::instance();
        plugin* pi = c.find(symbol_name);
        if (!pi) { return nullptr; }
        return pi->rawsym(symbol_name);
    }

    template<typename T>
    bool symbol(const std::string& symbol_name, T& ret) {
        auto& c = singleton<cache>::instance();
        plugin* pi = c.find(symbol_name);
        if (!pi) return false;
        return pi->symbol(symbol_name, ret);
    }
    

}

#endif
