#ifndef PTMP_FACTORY
#define PTMP_FACTORY

#include "ptmp/api.h"
#include "ptmp/upif.h"

namespace ptmp {


    namespace factory {

        typedef void* (*maker_func)(const char*);

        class FactoryBase {
        public:
            FactoryBase();
            void* make_void(const std::string& alias, const std::string& config);

            void add_plugin(const std::string& pi);

        };

        template<class TYPE>
        class Factory : public FactoryBase {
        public:
            Factory() : FactoryBase() {};
            TYPE* make(const std::string& alias, const std::string& config) {
                void* vptr = make_void(alias, config);
                return reinterpret_cast<TYPE*>(vptr);
            }
        };

        // User code does auto* thing = ptmp::factor::make<TYPE>(name, cfg);
        template<class TYPE>
        TYPE* make(const std::string& alias, const std::string& config) {
            typedef ptmp::factory::Factory<TYPE> FTYPE;
            FTYPE& f = upif::Singleton<FTYPE>::Instance();
            return f.make(alias, config);
        }
    }
}



#endif 
