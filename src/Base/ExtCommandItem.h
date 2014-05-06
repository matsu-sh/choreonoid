/**
   @author Shin'ichiro Nakaoka
*/

#ifndef CNOID_BASE_EXT_COMMAND_ITEM_H_INCLUDED
#define CNOID_BASE_EXT_COMMAND_ITEM_H_INCLUDED

#include "Item.h"
#include "Process.h"
#include "exportdecl.h"

namespace cnoid {
    
class CNOID_EXPORT ExtCommandItem : public Item
{
public:
    static void initializeClass(ExtensionManager* ext);
        
    ExtCommandItem();
    ExtCommandItem(const ExtCommandItem& org);
    virtual ~ExtCommandItem();
        
    void setCommand(const std::string& command);
    const std::string& command() const { return command_; }
    bool execute();
    bool terminate();
        
protected:
    virtual void onDisconnectedFromRoot();
    virtual ItemPtr doDuplicate() const;
    virtual void doPutProperties(PutPropertyFunction& putProperty);
    virtual bool store(Archive& archive);
    virtual bool restore(const Archive& archive);
        
private:
    void onReadyReadServerProcessOutput();
        
    std::string command_;
    Process process;
    bool signalReadyStandardOutputConnected;
    bool doCheckExistingProcess;
    bool doExecuteOnLoading;
};
    
typedef ref_ptr<ExtCommandItem> ExtCommandItemPtr;
}

#endif