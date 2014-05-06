/**
   \file
   \author Shin'ichiro Nakaoka
*/

#include "BodyLoader.h"
#include "VRMLBodyLoader.h"
#include "ColladaBodyLoader.h"
#include <cnoid/Exception>
#include <cnoid/YAMLReader>
#include <cnoid/FileUtil>
#include <cnoid/NullOut>
#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>
#include <boost/make_shared.hpp>
#include "gettext.h"

using namespace std;
using namespace boost;
using namespace cnoid;

namespace {

typedef boost::function<AbstractBodyLoaderPtr()> LoaderFactory;
typedef map<string, LoaderFactory> LoaderFactoryMap;
LoaderFactoryMap loaderFactoryMap;
mutex loaderFactoryMapMutex;

AbstractBodyLoaderPtr vrmlBodyLoaderFactory()
{
    return make_shared<VRMLBodyLoader>();
}

AbstractBodyLoaderPtr colladaBodyLoaderFactory()
{
    return make_shared<ColladaBodyLoader>();
}

struct FactoryRegistration
{
    FactoryRegistration(){
        BodyLoader::registerLoader("wrl", vrmlBodyLoaderFactory);
        BodyLoader::registerLoader("dae", colladaBodyLoaderFactory);
    }
} factoryRegistration;
    
}


bool BodyLoader::registerLoader(const std::string& extension, boost::function<AbstractBodyLoaderPtr()> factory)
{
    lock_guard<mutex> lock(loaderFactoryMapMutex);
    loaderFactoryMap[extension] = factory;
    return  true;
}
    

namespace cnoid {

class BodyLoaderImpl
{
public:
    ostream* os;
    AbstractBodyLoaderPtr loader;
    bool isVerbose;
    bool isShapeLoadingEnabled;
    int defaultDivisionNumber;
    double defaultCreaseAngle;

    typedef map<string, AbstractBodyLoaderPtr> LoaderMap;
    LoaderMap loaderMap;
        
    BodyLoaderImpl();
    ~BodyLoaderImpl();
    bool load(BodyPtr& body, const std::string& filename);
};
}


BodyLoader::BodyLoader()
{
    impl = new BodyLoaderImpl();
}


BodyLoaderImpl::BodyLoaderImpl()
{
    os = &nullout();
    isVerbose = false;
    isShapeLoadingEnabled = true;
    defaultDivisionNumber = -1;
    defaultCreaseAngle = -1.0;
}


BodyLoader::~BodyLoader()
{
    delete impl;
}


BodyLoaderImpl::~BodyLoaderImpl()
{

}


const char* BodyLoader::format() const
{
    return "General";
}


void BodyLoader::setMessageSink(std::ostream& os)
{
    impl->os = &os;
}


void BodyLoader::setVerbose(bool on)
{
    impl->isVerbose = on;
}


void BodyLoader::enableShapeLoading(bool on)
{
    impl->isShapeLoadingEnabled = on;
}
    

void BodyLoader::setDefaultDivisionNumber(int n)
{
    impl->defaultDivisionNumber = n;
}


void BodyLoader::setDefaultCreaseAngle(double theta)
{
    impl->defaultCreaseAngle = theta;
}


bool BodyLoader::load(BodyPtr body, const std::string& filename)
{
    return impl->load(body, filename);
}


BodyPtr BodyLoader::load(const std::string& filename)
{
    BodyPtr body = new Body();
    if(load(body, filename)){
        return body;
    }
    return BodyPtr();
}


bool BodyLoaderImpl::load(BodyPtr& body, const std::string& filename)
{
    bool result = false;

    filesystem::path orgpath(filename);
    string ext = getExtension(orgpath);
    string modelFilename;
    MappingPtr info;

    try {
        if(ext != "yaml"){
            modelFilename = filename;
        } else {
            YAMLReader parser;
            info = parser.loadDocument(filename)->toMapping();
            filesystem::path mpath(info->get("modelFile").toString());
            if(mpath.has_root_path()){
                modelFilename = getNativePathString(mpath);
            } else {
                modelFilename = getNativePathString(orgpath.parent_path() / mpath);
            }
            ext = getExtension(mpath);
        }

        AbstractBodyLoaderPtr loader;
        LoaderMap::iterator p = loaderMap.find(ext);
        if(p != loaderMap.end()){
            loader = p->second;
        } else {
            lock_guard<mutex> lock(loaderFactoryMapMutex);
            LoaderFactoryMap::iterator q = loaderFactoryMap.find(ext);
            if(q != loaderFactoryMap.end()){
                LoaderFactory factory = q->second;
                loader = factory();
                loaderMap[ext] = loader;
            }
        }

        if(!loader){
            (*os) << str(format(_("The file format of \"%1%\" is not supported by the body loader.\n"))
                         % getFilename(filesystem::path(modelFilename)));

        } else {
            loader->setMessageSink(*os);
            loader->setVerbose(isVerbose);
            loader->setShapeLoadingEnabled(isShapeLoadingEnabled);
            
            int dn = defaultDivisionNumber;
            if(info){
                Mapping& geometryInfo = *info->findMapping("geometry");
                if(geometryInfo.isValid()){
                    geometryInfo.read("divisionNumber", dn);
                }
            }
            if(dn > 0){
                loader->setDefaultDivisionNumber(dn);
            }

            if(defaultCreaseAngle >= 0.0){
                loader->setDefaultCreaseAngle(defaultCreaseAngle);
            }
            
            body->clearDevices();
            body->clearExtraJoints();
            if(info){
                body->resetInfo(info.get());
            } else {
                body->info()->clear();
            }

            result = loader->load(body, modelFilename);
        }
        
    } catch(const ValueNode::Exception& ex){
        (*os) << ex.message();
    } catch(const nonexistent_key_error& error){
        if(const std::string* message = get_error_info<error_info_message>(error)){
            (*os) << *message;
        }
    } catch(const std::exception& ex){
        (*os) << ex.what();
    }
    os->flush();
    
    return result;
}


AbstractBodyLoaderPtr BodyLoader::lastActualBodyLoader() const
{
    return impl->loader;
}