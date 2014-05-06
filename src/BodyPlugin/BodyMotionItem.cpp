/**
   @file
   @author Shin'ichiro Nakaoka
*/

#include "BodyMotionItem.h"
#include "BodyItem.h"
#include "KinematicFaultChecker.h"
#include <cnoid/MultiSeqItemCreationPanel>
#include <cnoid/BodyMotionUtil>
#include <cnoid/ItemManager>
#include <cnoid/Archive>
#include <cnoid/ZMPSeq>
#include <cnoid/LazyCaller>
#include <cnoid/MessageView>
#include <boost/bind.hpp>
#include <boost/format.hpp>
#include <QMessageBox>
#include "gettext.h"

using namespace std;
using namespace boost;
using namespace cnoid;


namespace {

typedef boost::function<AbstractSeqItemPtr(AbstractSeqPtr seq)> ExtraSeqItemFactory;
typedef map<string, ExtraSeqItemFactory> ExtraSeqItemFactoryMap;
ExtraSeqItemFactoryMap extraSeqItemFactories;

struct ExtraSeqItemInfo : public Referenced
{
    string key;
    AbstractSeqItemPtr item;
    boost::signals::connection sigUpdateConnection;

    ExtraSeqItemInfo(const string& key, AbstractSeqItemPtr& item) : key(key), item(item) { }
    ~ExtraSeqItemInfo() {
        sigUpdateConnection.disconnect();
        item->detachFromParentItem();
    }
};

typedef ref_ptr<ExtraSeqItemInfo> ExtraSeqItemInfoPtr;
    
typedef std::map<std::string, ExtraSeqItemInfoPtr> ExtraSeqItemInfoMap;
}


namespace cnoid {

class BodyMotionItemImpl
{
public:
    BodyMotionItem* self;
        
    boost::signals::connection jointPosSeqUpdateConnection;
    boost::signals::connection linkPosSeqUpdateConnection;

    ExtraSeqItemInfoMap extraSeqItemInfoMap;
    vector<ExtraSeqItemInfoPtr> extraSeqItemInfos;
    boost::signal<void()> sigExtraSeqItemsChanged;
    boost::signals::connection extraSeqsChangedConnection;

    BodyMotionItemImpl(BodyMotionItem* self);
    ~BodyMotionItemImpl();
    void initialize();
    void onSubItemUpdated();
    void onExtraSeqItemSetChanged();
    void updateExtraSeqItems();
};
}


static bool confirm(const std::string& message)
{
    return (QMessageBox::warning(
                0, _("Warning"), message.c_str(),
                QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Ok) == QMessageBox::Ok);
}


static bool fileIoSub(BodyMotionItem* item, std::ostream& os, bool loaded, bool isLoading)
{
    if(!loaded){
        os << item->motion()->seqMessage();
    }
    return loaded;
}
                

static bool loadStandardYamlFormat(BodyMotionItem* item, const std::string& filename, std::ostream& os)
{
    return fileIoSub(item, os, item->motion()->loadStandardYAMLformat(filename), true);
}
    

static bool saveAsStandardYamlFormat(BodyMotionItem* item, const std::string& filename, std::ostream& os)
{
    return fileIoSub(item, os, item->motion()->saveAsStandardYAMLformat(filename), false);
}


static bool importHrpsysSeqFileSet(BodyMotionItem* item, const std::string& filename, std::ostream& os)
{
    if(loadHrpsysSeqFileSet(*item->motion(), filename, os)){
        return true;
    }
    return false;
}
    
    
static bool exportHrpsysSeqFileSet(BodyMotionItem* item, const std::string& filename, std::ostream& os)
{
    double frameRate = item->motion()->frameRate();
    if(frameRate != 200.0){
        static format m1(_("The frame rate of a body motion exported as HRPSYS files should be standard value 200, "
                           "but the frame rate of \"%1%\" is %2%. The exported data may cause a problem.\n\n"
                           "Do you continue to export ?"));
        
        if(!confirm(str(m1 % item->name() % frameRate))){
            return false;
        }
    }
    
    BodyPtr body;
    BodyItem* bodyItem = item->findOwnerItem<BodyItem>();
    if(bodyItem){
        body = bodyItem->body();
        KinematicFaultChecker* checker = KinematicFaultChecker::instance();
        int numFaults = checker->checkFaults(bodyItem, item, os);
        if(numFaults > 0){
            static string m2(_("A fault has been detected. Please check the report in the MessageView.\n\n"
                               "Do you continue to export ?"));
            static format m3(_("%1% faults have been detected. Please check the report in the MessageView.\n\n"
                               "Do you continue to export ?"));
            
            bool result;
            
            if(numFaults == 1){
                result = confirm(m2);
            } else {
                result = confirm(str(m3 % numFaults));
            }
            
            if(!result){
                return false;
            }
        }
    }
    
    if(!getZMPSeq(*item->motion())){
        if(!confirm(_("There is no ZMP data. Do you continue to export ?"))){
            return false;
        }
    }
    
    return saveHrpsysSeqFileSet(*item->motion(), body, filename, os);
}


static bool bodyMotionItemPreFilter(BodyMotionItem* protoItem, Item* parentItem)
{
    BodyItemPtr bodyItem = dynamic_cast<BodyItem*>(parentItem);
    if(!bodyItem){
        bodyItem = parentItem->findOwnerItem<BodyItem>();
    }
    if(bodyItem){
        int prevNumJoints = protoItem->jointPosSeq()->numParts();
        int numJoints = bodyItem->body()->numJoints();
        if(numJoints != prevNumJoints){
            protoItem->jointPosSeq()->setNumParts(numJoints, true);
        }
    }
    return true;
}


/*
  static bool bodyMotionItemPostFilter(BodyMotionItem* protoItem, Item* parentItem)
  {
  BodyItemPtr bodyItem = dynamic_cast<BodyItem*>(parentItem);
  if(!bodyItem){
  bodyItem = parentItem->findOwnerItem<BodyItem>();
  }
  if(bodyItem){
  BodyPtr body = bodyItem->body();
  MultiValueSeqPtr qseq = protoItem->jointPosSeq();
  int n = std::min(body->numJoints(), qseq->numParts());
  for(int i=0; i < n; ++i){
  Link* joint = body->joint(i);
  if(joint->defaultJointValue != 0.0){
  MultiValueSeq::Part part = qseq->part(i);
  std::fill(part.begin(), part.end(), joint->defaultJointValue);
  }
  }
  }
    
  return true;
  }
*/


void BodyMotionItem::initializeClass(ExtensionManager* ext)
{
    static bool initialized = false;
    
    if(initialized){
        return;
    }
    
    ItemManager& im = ext->itemManager();
    
    im.registerClass<BodyMotionItem>(N_("BodyMotionItem"));

    im.addCreationPanel<BodyMotionItem>(new MultiSeqItemCreationPanel(_("Number of joints")));
    im.addCreationPanelPreFilter<BodyMotionItem>(bodyMotionItemPreFilter);
    //im.addCreationPanelPostFilter<BodyMotionItem>(bodyMotionItemPostFilter);

    im.addLoaderAndSaver<BodyMotionItem>(
        _("Body Motion"), "BODY-MOTION-YAML", "yaml",
        bind(loadStandardYamlFormat, _1, _2, _3),  bind(saveAsStandardYamlFormat, _1, _2, _3));

    im.addLoaderAndSaver<BodyMotionItem>(
        _("HRPSYS Sequence File Set"), "HRPSYS-SEQ-FILE-SET", "pos;vel;acc;hip;waist;gsens;zmp",
        bind(importHrpsysSeqFileSet, _1, _2, _3), bind(exportHrpsysSeqFileSet, _1, _2, _3),
        ItemManager::PRIORITY_CONVERSION);

    initialized = true;
}


void BodyMotionItem::addExtraSeqItemFactory
(const std::string& key, boost::function<AbstractSeqItemPtr(AbstractSeqPtr seq)> factory)
{
    extraSeqItemFactories[key] = factory;
}


BodyMotionItem::BodyMotionItem()
    : bodyMotion_(new BodyMotion())
{
    impl = new BodyMotionItemImpl(this);
}


BodyMotionItem::BodyMotionItem(BodyMotionPtr bodyMotion)
    : bodyMotion_(bodyMotion)
{
    impl = new BodyMotionItemImpl(this);
}


BodyMotionItem::BodyMotionItem(const BodyMotionItem& org)
    : AbstractMultiSeqItem(org),
      bodyMotion_(new BodyMotion(*org.bodyMotion_))
{
    impl = new BodyMotionItemImpl(this);
}


BodyMotionItemImpl::BodyMotionItemImpl(BodyMotionItem* self)
    : self(self)
{
    initialize();
}


void BodyMotionItemImpl::initialize()
{
    self->jointPosSeqItem_ = new MultiValueSeqItem(self->bodyMotion_->jointPosSeq());
    self->jointPosSeqItem_->setName("Joint");
    self->addSubItem(self->jointPosSeqItem_);

    jointPosSeqUpdateConnection =
        self->jointPosSeqItem_->sigUpdated().connect(
            bind(&BodyMotionItemImpl::onSubItemUpdated, this));

    self->linkPosSeqItem_ = new MultiSE3SeqItem(self->bodyMotion_->linkPosSeq());
    self->linkPosSeqItem_->setName("Cartesian");
    self->addSubItem(self->linkPosSeqItem_);

    linkPosSeqUpdateConnection = 
        self->linkPosSeqItem_->sigUpdated().connect(
            bind(&BodyMotionItemImpl::onSubItemUpdated, this));

    extraSeqsChangedConnection =
        self->bodyMotion_->sigExtraSeqsChanged().connect(
            bind(&BodyMotionItemImpl::onExtraSeqItemSetChanged, this));

    updateExtraSeqItems();
}


BodyMotionItem::~BodyMotionItem()
{
    delete impl;
}


BodyMotionItemImpl::~BodyMotionItemImpl()
{
    extraSeqsChangedConnection.disconnect();
    jointPosSeqUpdateConnection.disconnect();
    linkPosSeqUpdateConnection.disconnect();
}


AbstractMultiSeqPtr BodyMotionItem::abstractMultiSeq()
{
    return bodyMotion_;
}


void BodyMotionItem::notifyUpdate()
{
    impl->jointPosSeqUpdateConnection.block();
    jointPosSeqItem_->notifyUpdate();
    impl->jointPosSeqUpdateConnection.unblock();

    impl->linkPosSeqUpdateConnection.block();
    linkPosSeqItem_->notifyUpdate();
    impl->linkPosSeqUpdateConnection.unblock();

    vector<ExtraSeqItemInfoPtr>& extraSeqItemInfos = impl->extraSeqItemInfos;
    for(size_t i=0; i < extraSeqItemInfos.size(); ++i){
        ExtraSeqItemInfo* info = extraSeqItemInfos[i];
        info->sigUpdateConnection.block();
        info->item->notifyUpdate();
        info->sigUpdateConnection.unblock();
    }

    Item::notifyUpdate();
}


void BodyMotionItemImpl::onSubItemUpdated()
{
    self->suggestFileUpdate();
    self->Item::notifyUpdate();
}


int BodyMotionItem::numExtraSeqItems() const
{
    return impl->extraSeqItemInfos.size();
}


const std::string& BodyMotionItem::extraSeqKey(int index) const
{
    return impl->extraSeqItemInfos[index]->key;
}


AbstractSeqItem* BodyMotionItem::extraSeqItem(int index)
{
    return impl->extraSeqItemInfos[index]->item;
}


const AbstractSeqItem* BodyMotionItem::extraSeqItem(int index) const
{
    return impl->extraSeqItemInfos[index]->item;
}


SignalProxy< boost::signal<void()> > BodyMotionItem::sigExtraSeqItemsChanged()
{
    return impl->sigExtraSeqItemsChanged;
}


void BodyMotionItemImpl::onExtraSeqItemSetChanged()
{
    callLater(bind(&BodyMotionItemImpl::updateExtraSeqItems, this));
}


void BodyMotionItem::updateExtraSeqItems()
{
    impl->updateExtraSeqItems();
}


void BodyMotionItemImpl::updateExtraSeqItems()
{
    extraSeqItemInfos.clear();

    BodyMotion& bodyMotion = *self->bodyMotion_;
    BodyMotion::ConstSeqIterator p;
    for(p = bodyMotion.extraSeqBegin(); p != bodyMotion.extraSeqEnd(); ++p){
        const string& key = p->first;
        const AbstractSeqPtr& newSeq = p->second;
        AbstractSeqItemPtr newItem;
        ExtraSeqItemInfoMap::iterator p = extraSeqItemInfoMap.find(key);
        if(p != extraSeqItemInfoMap.end()){
            ExtraSeqItemInfo* info = p->second;
            AbstractSeqItemPtr& prevItem = info->item;
            if(typeid(prevItem->abstractSeq()) == typeid(newSeq)){
                extraSeqItemInfos.push_back(info);
                newItem = prevItem;
            }
        }
        if(!newItem){
            ExtraSeqItemFactoryMap::iterator q = extraSeqItemFactories.find(key);
            if(q != extraSeqItemFactories.end()){
                ExtraSeqItemFactory& factory = q->second;
                newItem = factory(newSeq);
                if(newItem){
                    self->addSubItem(newItem);
                    ExtraSeqItemInfo* info = new ExtraSeqItemInfo(key, newItem);
                    info->sigUpdateConnection =
                        newItem->sigUpdated().connect(
                            bind(&BodyMotionItemImpl::onSubItemUpdated, this));
                    extraSeqItemInfos.push_back(info);
                }
            }
        }
    }
    extraSeqItemInfoMap.clear();
    for(size_t i=0; i < extraSeqItemInfos.size(); ++i){
        ExtraSeqItemInfo* info = extraSeqItemInfos[i];
        extraSeqItemInfoMap.insert(make_pair(info->key, info));
    }

    sigExtraSeqItemsChanged();
}


bool BodyMotionItem::onChildItemAboutToBeAdded(Item* childItem_, bool isManualOperation)
{
    if(isManualOperation){
        AbstractSeqItem* seqItem = dynamic_cast<AbstractSeqItem*>(childItem_);
        if(seqItem){
            if(!dynamic_cast<BodyMotionItem*>(seqItem)){
                bool existingFound = false;
                for(Item* item = childItem(); item; item = item->nextItem()){
                    if(item->isSubItem() && item->name() == seqItem->name()){
                        if(AbstractSeqItem* orgSeqItem = dynamic_cast<AbstractSeqItem*>(item)){
                            existingFound = true;
                            if(showConfirmDialog(
                                   _("Confirm"),
                                   str(format(_("Do you want to replace the data of %1%?")) % item->name()))){
                                *orgSeqItem->abstractSeq() = *seqItem->abstractSeq();
                                return false;
                            }
                        }
                    }
                }
                if(!existingFound){
                    if(showConfirmDialog(
                           _("Confirm"),
                           str(format(_("Do you want to set %1% as a sequence data of %2%?"))
                               % childItem_->name() % this->name()))){
                        motion()->setExtraSeq(seqItem->abstractSeq());
                        return false;
                    }
                }
            }
        }
    }
    return true;
}


ItemPtr BodyMotionItem::doDuplicate() const
{
    return new BodyMotionItem(*this);
}


bool BodyMotionItem::store(Archive& archive)
{
    if(overwrite() || !lastAccessedFilePath().empty()){
        archive.writeRelocatablePath("filename", lastAccessedFilePath());
        archive.write("format", lastAccessedFileFormatId());
        return true;
    }
    return false;
}


bool BodyMotionItem::restore(const Archive& archive)
{
    std::string filename, formatId;
    if(archive.readRelocatablePath("filename", filename) && archive.read("format", formatId)){
        if(load(filename, formatId)){
            return true;
        }
    }
    return false;
}