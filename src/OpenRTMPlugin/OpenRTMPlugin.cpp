/*!
  @file
  @author Shin'ichiro Nakaoka
*/

#include "BodyRTCItem.h"
#include "RTCItem.h"
#include "OpenHRPClockGeneratorItem.h"
#include "ChoreonoidExecutionContext.h"
#include "OpenRTMUtil.h"
#include <cnoid/Plugin>
#include <cnoid/ItemManager>
#include <cnoid/Archive>
#include <cnoid/MessageView>
#include <cnoid/MenuManager>
#include <cnoid/CorbaPlugin>
#include <cnoid/SimulationBar>
#include <cnoid/Sleep>
#include <QTcpSocket>
#include <rtm/ComponentActionListener.h>
#include <boost/thread/thread.hpp>
#include <boost/bind.hpp>
#include "gettext.h"

using namespace std;
using namespace boost;
using namespace cnoid;

namespace {

class ManagerEx : public RTC::Manager
{
public:
    RTM::ManagerServant* servant() {
        return m_mgrservant;
    }
};

ManagerEx* manager;

std::set<RTC::RTObject_impl*> managedComponents;


class PostComponentShutdownListenr : public RTC::PostComponentActionListener
{
    RTC::RTObject_impl* rtc;
public:
    PostComponentShutdownListenr(RTC::RTObject_impl* rtc) : rtc(rtc) { }
    virtual void operator()(UniqueId ec_id, ReturnCode_t ret){
        managedComponents.erase(rtc);
    }
};
    
    
void rtcManagerMainLoop() {
    manager->runManager();
}

class OpenRTMPlugin : public Plugin
{
    MessageView* mv;
    boost::thread rtcManagerMainLoopThread;
    Action* deleteRTCsOnSimulationStartCheck;
    boost::signals::connection connectionToSigSimulaionAboutToStart;
        
public:
    OpenRTMPlugin() : Plugin("OpenRTM") {
        require("Body");
        require("Corba");
        precede("Corba");
    }
        
    virtual bool initialize() {

        const char* argv[] = {
            "choreonoid",
            "-o", "manager.shutdown_on_nortcs: NO",
            "-o", "manager.shutdown_auto: NO",
            "-o", "naming.formats: %n.rtc",
#ifdef Q_OS_WIN32
            // To reduce the startup time on Windows
            "-o", "corba.args: -ORBclientCallTimeOutPeriod 100",
#endif
            "-o", "logger.enable: NO",
            //"-o", "corba.nameservers: localhost",
            //"-o", "exec_cxt.periodic.type: SynchExtTriggerEC",
            //"-o", "exec_cxt.periodic.rate: 1000000",
            //"-o", "manager.is_master: YES"
            //"-o", "logger.enable: YES",
            //"-o", "logger.file_name: stdout",
            //"-o", "logger.log_level: TRACE",
            //"-o", "corba.args: -ORBendPoint giop:tcp::2809 -ORBpoaUniquePersistentSystemIds 1"
        };

#ifdef Q_OS_WIN32
        int numArgs = 9;
#else
        int numArgs = 7;
#endif
        bool FORCE_DISABLE_LOG = true;
        if(FORCE_DISABLE_LOG){
            numArgs += 2;
        }

        mv = MessageView::instance();

        cnoid::checkOrInvokeCorbaNameServer();

        manager = static_cast<ManagerEx*>(RTC::Manager::init(numArgs, const_cast<char**>(argv)));

        RTM::Manager_ptr servantRef = manager->servant()->getObjRef();
        if(CORBA::is_nil(servantRef)){
            manager->servant()->createINSManager();
        }

        if(manager->registerECFactory("ChoreonoidExecutionContext",
                                      RTC::ECCreate<cnoid::ChoreonoidExecutionContext>,
                                      RTC::ECDelete<cnoid::ChoreonoidExecutionContext>)){
            mv->putln(_("ChoreonoidExecutionContext has been registered."));
        }
            
        manager->activateManager();
            
#ifdef Q_OS_WIN32
        omniORB::setClientCallTimeout(0); // reset the global timeout setting?
#endif
                
        if(!cnoid::takeOverCorbaPluginInitialization(manager->getORB())){
            return false;
        }

        BodyRTCItem::initialize(this);
        RTCItem::initialize(this);
        OpenHRPClockGeneratorItem::initialize(this);
            
        VirtualRobotRTC::registerFactory(manager, "VirtualRobot");
            
        rtcManagerMainLoopThread = boost::thread(rtcManagerMainLoop);

        menuManager().setPath("/Tools/OpenRTM").addItem(_("Delete unmanaged RT components"))
            ->sigTriggered().connect(bind(&OpenRTMPlugin::deleteUnmanagedRTCs, this, true));

        deleteRTCsOnSimulationStartCheck =
            menuManager().setPath("/Options/OpenRTM").addCheckItem(
                _("Delete unmanaged RT components on starting a simulation"));
        deleteRTCsOnSimulationStartCheck->sigToggled().connect(
            bind(&OpenRTMPlugin::onDeleteRTCsOnSimulationStartToggled, this, _1));

        setProjectArchiver(
            bind(&OpenRTMPlugin::store, this, _1),
            bind(&OpenRTMPlugin::restore, this, _1));
            
        return true;
    }


    bool store(Archive& archive)
        {
            archive.write("deleteUnmanagedRTCsOnStartingSimulation", deleteRTCsOnSimulationStartCheck->isChecked());
            return true;
        }


    void restore(const Archive& archive)
        {
            bool checked = deleteRTCsOnSimulationStartCheck->isChecked();
            if(!archive.read("deleteUnmanagedRTCsOnStartingSimulation", checked)){
                // for reading the old version format
                const Archive& oldNode = *archive.findSubArchive("OpenRTMPlugin");
                if(oldNode.isValid()){
                    oldNode.read("deleteUnmanagedRTCsOnStartingSimulation", checked);
                }
            }
            deleteRTCsOnSimulationStartCheck->setChecked(checked);
        }

        
    void onDeleteRTCsOnSimulationStartToggled(bool on)
        {
            connectionToSigSimulaionAboutToStart.disconnect();
            if(on){
                connectionToSigSimulaionAboutToStart = 
                    SimulationBar::instance()->sigSimulationAboutToStart().connect(
                        bind(&OpenRTMPlugin::deleteUnmanagedRTCs, this, false));
            }
        }


    void onSimulationAboutToStart()
        {
            if(deleteUnmanagedRTCs(false) > 0){
                mv->flush();
            }
        }

    int deleteUnmanagedRTCs(bool doPutMessageWhenNoUnmanagedComponents)
        {
            int n = cnoid::numUnmanagedRTCs();

            if(n == 0){
                if(doPutMessageWhenNoUnmanagedComponents){
                    mv->notify("There are no RT components which are not managed by Choreonoid.");
                }
            } else {
                if(n == 1){
                    mv->notify(_("An RT component which is not managed by Choreonoid is being deleted."));
                } else {
                    mv->notify(format(_("%1% RT components which are not managed by Choreonoid are being deleted.")) % n);
                }
                mv->flush();
                cnoid::deleteUnmanagedRTCs();
                if(n == 1){
                    mv->notify(_("The unmanaged RT component has been deleted."));
                } else {
                    mv->notify(_("The unmanaged RT components have been deleted."));
                }
            }

            return n;
        }

        
    virtual bool finalize() {

        connectionToSigSimulaionAboutToStart.disconnect();

        std::vector<RTObject_impl*> rtcs = manager->getComponents();
        for(int i=0; i < rtcs.size(); ++i){
            RTObject_impl* rtc = rtcs[i];
            RTC::ExecutionContextList_var eclist = rtc->get_participating_contexts();
            if(eclist->length() > 0){
                for(CORBA::ULong j=0; j < eclist->length(); ++j){
                    if(!CORBA::is_nil(eclist[j])){
                        eclist[j]->remove_component(rtc->getObjRef());
                    }
                }
            }
        }

        // delete all the components owned by exisiting BodyRTCItems
        itemManager().detachAllManagedTypeItemsFromRoot();

        manager->shutdown();
        manager->unloadAll();

        return true;
    }
};
}

CNOID_IMPLEMENT_PLUGIN_ENTRY(OpenRTMPlugin);


RTM::Manager_ptr cnoid::getRTCManagerServant()
{
    return RTM::Manager::_duplicate(manager->servant()->getObjRef());
}


RTC::RTObject_impl* cnoid::createManagedRTC(const char* comp_args)
{
    RTC::RTObject_impl* rtc = manager->createComponent(comp_args);
    if(rtc){
        managedComponents.insert(rtc);
        rtc->addPostComponentActionListener(POST_ON_SHUTDOWN, new PostComponentShutdownListenr(rtc));
    }
    return rtc;
}


CNOID_EXPORT int cnoid::numUnmanagedRTCs()
{
    int n = 0;
    std::vector<RTObject_impl*> rtcs = manager->getComponents();
    for(int i=0; i < rtcs.size(); ++i){
        RTObject_impl* rtc = rtcs[i];
        if(managedComponents.find(rtc) == managedComponents.end()){
            ++n;
        }
    }
    return n;
}


CNOID_EXPORT int cnoid::deleteUnmanagedRTCs()
{
    int numDeleted = 0;
    
    std::vector<RTObject_impl*> rtcs = manager->getComponents();
    for(int i=0; i < rtcs.size(); ++i){
        RTObject_impl* rtc = rtcs[i];
        if(managedComponents.find(rtc) == managedComponents.end()){
            RTC::ExecutionContextList_var eclist = rtc->get_participating_contexts();
            if(eclist->length() > 0){
                for(CORBA::ULong j=0; j < eclist->length(); ++j){
                    if(!CORBA::is_nil(eclist[j])){
                        eclist[j]->remove_component(rtc->getObjRef());
                    }
                }
            }
            PortServiceList_var ports = rtc->get_ports();
            for(CORBA::ULong j=0; j < ports->length(); ++j){
                ports[j]->disconnect_all();
            }
            rtc->exit();
            ++numDeleted;
        }
    }
    return numDeleted;
}


bool cnoid::deleteRTC(RTC::RtcBase* rtc, bool waitToBeDeleted)
{
    if(rtc){
        string rtcName(rtc->getInstanceName());
        rtc->exit();

        if(!waitToBeDeleted){
            return true;
        } else {
            Manager& rtcManager = RTC::Manager::instance();
            for(int i=0; i < 100; ++i){
                RTC::RtcBase* component = rtcManager.getComponent(rtcName.c_str());
                if(component){
                    msleep(20);
                } else {
                    return true;
                }
            }
        }
    }
    return false;
}


namespace cnoid {
    
template<> CORBA::Object::_ptr_type findRTCService<CORBA::Object>(RTC::RTObject_ptr rtc, const std::string& name)
{
    CORBA::Object_ptr service = CORBA::Object::_nil();
            
    RTC::PortServiceList ports;
    ports = *(rtc->get_ports());
        
    RTC::ComponentProfile* cprof;
    cprof = rtc->get_component_profile();
    std::string portname = std::string(cprof->instance_name) + "." + name;
        
    for(unsigned int i=0; i < ports.length(); i++){
        RTC::PortService_var port = ports[i];
        RTC::PortProfile* prof = port->get_port_profile();
        if(std::string(prof->name) == portname){
            RTC::ConnectorProfile connProfile;
            connProfile.name = "noname";
            connProfile.connector_id = "";
            connProfile.ports.length(1);
            connProfile.ports[0] = port;
            connProfile.properties = 0;
            port->connect(connProfile);

            const char* ior = 0;
            connProfile.properties[0].value >>= ior;
            if(ior){
                service = getORB()->string_to_object(ior);
            }
            port->disconnect(connProfile.connector_id);
            break;
        }
    }
        
    return service;
}
}
