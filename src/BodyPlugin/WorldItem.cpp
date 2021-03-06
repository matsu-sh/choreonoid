/**
   @file
   @author Shin'ichiro Nakaoka
*/

#include "WorldItem.h"
#include "KinematicsBar.h"
#include <cnoid/MessageView>
#include <cnoid/ItemManager>
#include <cnoid/RootItem>
#include <cnoid/PutPropertyFunction>
#include <cnoid/YAMLReader>
#include <cnoid/Archive>
#include <cnoid/ConnectionSet>
#include <cnoid/LazyCaller>
#include <cnoid/BodyCollisionDetectorUtil>
#include <cnoid/SceneCollision>
#include <boost/bind.hpp>
#include <boost/dynamic_bitset.hpp>
#include <boost/make_shared.hpp>
#include <algorithm>
#include "gettext.h"

using namespace std;
using namespace cnoid;

namespace {

const bool TRACE_FUNCTIONS = false;

struct BodyItemInfo {
    BodyItemInfo() {
        kinematicStateChanged = false;
    }
    int geometryId;
    bool kinematicStateChanged;
};
typedef map<BodyItem*, BodyItemInfo> BodyItemInfoMap;
}


namespace cnoid {

class WorldItemImpl
{
public:
    WorldItemImpl(WorldItem* self);
    WorldItemImpl(WorldItem* self, const WorldItemImpl& org);
    ~WorldItemImpl();
            
    WorldItem* self;
    ostream& os;

    ItemList<BodyItem> collisionBodyItems;
    boost::dynamic_bitset<> collisionBodyItemsSelfCollisionFlags;

    Connection sigItemTreeChangedConnection;
    ConnectionSet sigKinematicStateChangedConnections;

    bool isCollisionDetectionEnabled;
    LazyCaller updateCollisionsLater;
    KinematicsBar* kinematicsBar;

    BodyItemInfoMap bodyItemInfoMap;

    Selection collisionDetectorType;
    CollisionDetectorPtr collisionDetector;
    vector<BodyItemInfoMap::iterator> geometryIdToBodyInfoMap;
    boost::shared_ptr< vector<CollisionLinkPairPtr> > collisions;
    Signal<void()> sigCollisionsUpdated;
    LazyCaller updateCollisionDetectorLater;

    SceneCollisionPtr sceneCollision;

    void init();
    bool selectCollisionDetector(int index);
    void enableCollisionDetection(bool on);
    void clearCollisionDetector();
    void updateCollisionDetector(bool forceUpdate);
    void updateCollisionBodyItems();
    void onBodyKinematicStateChanged(BodyItem* bodyItem);
    void updateCollisions(bool forceUpdate);
    void extractCollisions(const CollisionPair& collisionPair);
};
}


void WorldItem::initializeClass(ExtensionManager* ext)
{
    ext->itemManager().registerClass<WorldItem>(N_("WorldItem"));
    ext->itemManager().addCreationPanel<WorldItem>();
}


WorldItem::WorldItem()
{
    impl = new WorldItemImpl(this);
    setName("World");
}


WorldItemImpl::WorldItemImpl(WorldItem* self)
    : self(self),
      os(MessageView::mainInstance()->cout()),
      updateCollisionsLater(boost::bind(&WorldItemImpl::updateCollisions, this, false)),
      updateCollisionDetectorLater(boost::bind(&WorldItemImpl::updateCollisionDetector, this, false))
{
    const int n = CollisionDetector::numFactories();
    collisionDetectorType.resize(n);
    for(int i=0; i < n; ++i){
        collisionDetectorType.setSymbol(i, CollisionDetector::factoryName(i));
    };
    collisionDetectorType.select("AISTCollisionDetector");
    isCollisionDetectionEnabled = false;

    init();
}


WorldItem::WorldItem(const WorldItem& org)
    : Item(org)
{
    impl = new WorldItemImpl(this, *org.impl);
}


WorldItemImpl::WorldItemImpl(WorldItem* self, const WorldItemImpl& org)
    : self(self),
      os(org.os),
      updateCollisionsLater(boost::bind(&WorldItemImpl::updateCollisions, this, false)),
      updateCollisionDetectorLater(boost::bind(&WorldItemImpl::updateCollisionDetector, this, false))
{
    collisionDetectorType = org.collisionDetectorType;
    isCollisionDetectionEnabled = org.isCollisionDetectionEnabled;
    
    init();
}


void WorldItemImpl::init()
{
    kinematicsBar = KinematicsBar::instance();
    collisionDetector = CollisionDetector::create(collisionDetectorType.selectedIndex());
    collisions = boost::make_shared< vector<CollisionLinkPairPtr> >();
    sceneCollision = new SceneCollision(collisions);
    sceneCollision->setName("Collisions");
}

    
WorldItem::~WorldItem()
{
    delete impl;
}


WorldItemImpl::~WorldItemImpl()
{
    sigKinematicStateChangedConnections.disconnect();
    sigItemTreeChangedConnection.disconnect();
}


const ItemList<BodyItem>& WorldItem::collisionBodyItems() const
{
    return impl->collisionBodyItems;
}


const std::vector<CollisionLinkPairPtr>& WorldItem::collisions() const
{
    return *impl->collisions;
}


bool WorldItem::selectCollisionDetector(const std::string& name)
{
    return impl->selectCollisionDetector(CollisionDetector::factoryIndex(name));
}


bool WorldItemImpl::selectCollisionDetector(int index)
{
    if(index >= 0 && index < collisionDetectorType.size()){
        CollisionDetectorPtr newCollisionDetector = CollisionDetector::create(index);
        if(newCollisionDetector){
            collisionDetector = newCollisionDetector;
            collisionDetectorType.select(index);
            if(isCollisionDetectionEnabled){
                updateCollisionDetector(true);
            }
            return true;
        }
    }
    return false;
}


CollisionDetectorPtr WorldItem::collisionDetector()
{
    if(impl->isCollisionDetectionEnabled){
        impl->updateCollisionDetectorLater.flush();
    }
    return impl->collisionDetector;
}


void WorldItem::enableCollisionDetection(bool on)
{
    impl->enableCollisionDetection(on);
}


void WorldItemImpl::enableCollisionDetection(bool on)
{
    if(TRACE_FUNCTIONS){
        os << "WorldItemImpl::enableCollisionDetection(" << on << ")" << endl;
    }

    bool changed = false;
    
    if(isCollisionDetectionEnabled && !on){
        clearCollisionDetector();
        sigItemTreeChangedConnection.disconnect();
        isCollisionDetectionEnabled = false;
        changed = true;
        
    } else if(!isCollisionDetectionEnabled && on){
        isCollisionDetectionEnabled = true;

        updateCollisionDetector(true);
        sigItemTreeChangedConnection =
            RootItem::mainInstance()->sigTreeChanged().connect(
                boost::bind(&WorldItem::updateCollisionDetectorLater, self));
        changed = true;
    }

    if(changed){
        self->notifyUpdate();
        sigCollisionsUpdated();
    }
}


bool WorldItem::isCollisionDetectionEnabled()
{
    return impl->isCollisionDetectionEnabled;
}


void WorldItemImpl::clearCollisionDetector()
{
    if(TRACE_FUNCTIONS){
        os << "WorldItemImpl::clearCollisionDetector()" << endl;
    }

    collisionDetector->clearGeometries();
    geometryIdToBodyInfoMap.clear();
    sigKinematicStateChangedConnections.disconnect();
    bodyItemInfoMap.clear();

    for(size_t i=0; i < collisionBodyItems.size(); ++i){
        collisionBodyItems[i]->clearCollisions();
    }
}


void WorldItem::updateCollisionDetectorLater()
{
    impl->updateCollisionDetectorLater();
}


void WorldItem::updateCollisionDetector()
{
    impl->updateCollisionDetector(true);
}


/**
   \todo reduce the number of calling this function when the project is loaded.
*/
void WorldItemImpl::updateCollisionDetector(bool forceUpdate)
{
    if(TRACE_FUNCTIONS){
        os << "WorldItemImpl::updateCollisionDetector()" << endl;
    }

    if(!isCollisionDetectionEnabled){
        return;
    }

    if(!forceUpdate){
        ItemList<BodyItem> prevBodyItems = collisionBodyItems;
        boost::dynamic_bitset<> prevSelfCollisionFlags = collisionBodyItemsSelfCollisionFlags;
        updateCollisionBodyItems();
        if(collisionBodyItems == prevBodyItems &&
            collisionBodyItemsSelfCollisionFlags == prevSelfCollisionFlags){
            return;
        }
    } else {
        updateCollisionBodyItems();
    }

    clearCollisionDetector();

    for(size_t i=0; i < collisionBodyItems.size(); ++i){
        BodyItem* bodyItem = collisionBodyItems.get(i);
        const BodyPtr& body = bodyItem->body();
        const int numLinks = body->numLinks();
            
        pair<BodyItemInfoMap::iterator, bool> inserted =
            bodyItemInfoMap.insert(make_pair(bodyItem, BodyItemInfo()));
        BodyItemInfo& info = inserted.first->second;
            
        info.geometryId = addBodyToCollisionDetector(
            *body, *collisionDetector, bodyItem->isSelfCollisionDetectionEnabled());
        geometryIdToBodyInfoMap.resize(collisionDetector->numGeometries(), inserted.first);
        
        sigKinematicStateChangedConnections.add(
            bodyItem->sigKinematicStateChanged().connect(
                boost::bind(&WorldItemImpl::onBodyKinematicStateChanged, this, bodyItem)));
    }

    collisionDetector->makeReady();
    updateCollisions(true);
}


void WorldItemImpl::updateCollisionBodyItems()
{
    collisionBodyItemsSelfCollisionFlags.clear();
    collisionBodyItems.extractChildItems(self);
    ItemList<BodyItem>::iterator p = collisionBodyItems.begin();
    while(p != collisionBodyItems.end()){
        BodyItemPtr& bodyItem = *p;
        if(bodyItem->isCollisionDetectionEnabled()){
            collisionBodyItemsSelfCollisionFlags.push_back(
                bodyItem->isSelfCollisionDetectionEnabled());
            ++p;
        } else {
            p = collisionBodyItems.erase(p);
        }
    }
}


/**
   \todo Check if BodyItemInfoMap::iterator can be used as the function parameter
*/
void WorldItemImpl::onBodyKinematicStateChanged(BodyItem* bodyItem)
{
    if(TRACE_FUNCTIONS){
        os << "WorldItemImpl::onBodyKinematicStateChanged()" << endl;
    }
    
    BodyItemInfoMap::iterator p = bodyItemInfoMap.find(bodyItem);
    if(p != bodyItemInfoMap.end()){
        BodyItemInfo& info = p->second;
        info.kinematicStateChanged = true;
        updateCollisionsLater.setPriority(kinematicsBar->collisionDetectionPriority());
        updateCollisionsLater();
    }
}


void WorldItem::updateCollisions()
{
    impl->updateCollisions(true);
}


void WorldItemImpl::updateCollisions(bool forceUpdate)
{
    for(BodyItemInfoMap::iterator p = bodyItemInfoMap.begin(); p != bodyItemInfoMap.end(); ++p){
        BodyItemInfo& info = p->second;
        BodyItem* bodyItem = p->first;
        bodyItem->clearCollisions();
        if(info.kinematicStateChanged || forceUpdate){
            const BodyPtr& body = bodyItem->body();
            for(int i=0; i < body->numLinks(); ++i){
                collisionDetector->updatePosition(info.geometryId + i, body->link(i)->T());
            }
        }
        info.kinematicStateChanged = false;
    }

    collisions->clear();

    collisionDetector->detectCollisions(boost::bind(&WorldItemImpl::extractCollisions, this, _1));

    sceneCollision->setDirty();

    for(BodyItemInfoMap::iterator p = bodyItemInfoMap.begin(); p != bodyItemInfoMap.end(); ++p){
        BodyItem* bodyItem = p->first;
        BodyItemInfo& info = p->second;
        bodyItem->notifyCollisionUpdate();
    }
    
    sigCollisionsUpdated();
}


void WorldItemImpl::extractCollisions(const CollisionPair& collisionPair)
{
    CollisionLinkPairPtr collisionLinkPair = boost::make_shared<CollisionLinkPair>();
    collisionLinkPair->collisions = collisionPair.collisions;
    BodyItem* bodyItem = 0;
    for(int i=0; i < 2; ++i){
        const int geometryId = collisionPair.geometryId[i];
        BodyItemInfoMap::const_iterator p = geometryIdToBodyInfoMap[geometryId];
        const BodyItemInfo& info = p->second;
        const int linkIndex = geometryId - info.geometryId;
        if(p->first != bodyItem){
            bodyItem = p->first;
            bodyItem->collisions().push_back(collisionLinkPair);
        }
        const BodyPtr& body = bodyItem->body();
        collisionLinkPair->body[i] = body;
        collisionLinkPair->link[i] = body->link(linkIndex);
        bodyItem->collisionsOfLink(linkIndex).push_back(collisionLinkPair);
        bodyItem->collisionLinkBitSet().set(linkIndex);
    }
    collisions->push_back(collisionLinkPair);
}


SignalProxy<void()> WorldItem::sigCollisionsUpdated()
{
    return impl->sigCollisionsUpdated;
}


SgNode* WorldItem::getScene()
{
    return impl->sceneCollision;
}


ItemPtr WorldItem::doDuplicate() const
{
    return new WorldItem(*this);
}


void WorldItem::doPutProperties(PutPropertyFunction& putProperty)
{
    putProperty(_("Collision detection"), isCollisionDetectionEnabled(),
                boost::bind(&WorldItem::enableCollisionDetection, this, _1), true);
    putProperty(_("Collision detector"), impl->collisionDetectorType,
                boost::bind(&WorldItemImpl::selectCollisionDetector, impl, _1));
}


bool WorldItem::store(Archive& archive)
{
    archive.write("collisionDetection", isCollisionDetectionEnabled());
    archive.write("collisionDetector", impl->collisionDetectorType.selectedSymbol());
    return true;
}


bool WorldItem::restore(const Archive& archive)
{
    string symbol;
    if(archive.read("collisionDetector", symbol)){
        selectCollisionDetector(symbol);
    }
    if(archive.get("collisionDetection", false)){
        archive.addPostProcess(boost::bind(&WorldItemImpl::enableCollisionDetection, impl, true));
    }
    return true;
}
