#include "ptmp/filter.h"

ptmp::filter::engine_t::~engine_t()
{
}


class DummyFilterEngine : public ptmp::filter::engine_t
{
public:
    virtual ~DummyFilterEngine() {}
    virtual void operator()(const ptmp::data::TPSet& input_tpset,
                            std::vector<ptmp::data::TPSet>& output_tpsets) {
        output_tpsets.push_back(input_tpset);
    }
    
};

PTMP_FILTER_NOCONFIG(DummyFilterEngine, dummy_filter)
