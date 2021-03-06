/**
   \file
   \author Shin'ichiro Nakaoka
*/

#ifndef CNOID_BODY_PLUGIN_SIMULATION_SCRIPT_ITEM_H_INCLUDED
#define CNOID_BODY_PLUGIN_SIMULATION_SCRIPT_ITEM_H_INCLUDED

#include <cnoid/ScriptItem>
#include "exportdecl.h"

namespace cnoid {

class SimulationScriptItemImpl;

class CNOID_EXPORT SimulationScriptItem : public ScriptItem
{
public:
    SimulationScriptItem();
    SimulationScriptItem(const SimulationScriptItem& org);

    enum ExecTiming {
        BEFORE_INITIALIZATION,
        DURING_INITIALIZATION,
        AFTER_INITIALIZATION,
        DURING_FINALIZATION,
        AFTER_FINALIZATION,
        NUM_TIMINGS,
    };

    ExecTiming execTiming() const;
    double execDelay() const;

    virtual bool execute();
    virtual bool executeAsSimulationScript() = 0;

protected:
    virtual ~SimulationScriptItem();

    virtual void doPutProperties(PutPropertyFunction& putProperty);
    virtual bool store(Archive& archive);
    virtual bool restore(const Archive& archive);
            
private:
    SimulationScriptItemImpl* impl;
    friend class SimulationScriptItemImpl;
};

typedef ref_ptr<SimulationScriptItem> SimulationScriptItemPtr;
}

#endif
