/*!
  @file
  @author Shin'ichiro Nakaoka
*/

#include "SceneGraph.h"
#include "SceneShape.h"
#include "SceneLight.h"
#include "SceneCamera.h"
#include "SceneVisitor.h"
#include "MeshNormalGenerator.h"
#include <boost/unordered_map.hpp>
#include <boost/make_shared.hpp>

using namespace std;
using namespace boost;
using namespace cnoid;

namespace {
const double PI = 3.14159265358979323846;
}


SgUpdate::~SgUpdate()
{

}

namespace {
typedef unordered_map<const SgObject*, SgObjectPtr> CloneMap;
}

namespace cnoid {
class SgCloneMapImpl : public CloneMap { };
}


SgCloneMap::SgCloneMap()
{
    cloneMap = new SgCloneMapImpl;
    isNonNodeCloningEnabled_ = true;
}


SgCloneMap::SgCloneMap(const SgCloneMap& org)
{
    cloneMap = new SgCloneMapImpl(*org.cloneMap);
    isNonNodeCloningEnabled_ = org.isNonNodeCloningEnabled_;
}


void SgCloneMap::clear()
{
    cloneMap->clear();
}


SgObject* SgCloneMap::findOrCreateClone(const SgObject* org)
{
    CloneMap::iterator p = cloneMap->find(org);
    if(p == cloneMap->end()){
        SgObject* clone = org->clone(*this);
        (*cloneMap)[org] = clone;
        return clone;
    } else {
        return p->second.get();
    }
}


SgCloneMap::~SgCloneMap()
{
    delete cloneMap;
}


SgObject::SgObject()
{

}


SgObject::SgObject(const SgObject& org)
    : name_(org.name_)
{

}


SgObject* SgObject::clone(SgCloneMap& cloneMap) const
{
    return new SgObject(*this);
}


int SgObject::numElements() const
{
    return 0;
}


SgObject* SgObject::element(int index)
{
    return 0;
}


void SgObject::transferUpdate(SgUpdate& update)
{
    update.push(this);
    sigUpdated_(update);
    for(const_ownerIter p = owners.begin(); p != owners.end(); ++p){
        (*p)->transferUpdate(update);
    }
    update.pop();
}


void SgObject::addOwner(SgObject* node)
{
    owners.insert(node);
    if(owners.size() == 1){
        sigGraphConnection_(true);
    }
}


void SgObject::addOwner(SgObject* node, SgUpdate& update)
{
    owners.insert(node);
    transferUpdate(update);
    if(owners.size() == 1){
        sigGraphConnection_(true);
    }
}


void SgObject::removeOwner(SgObject* node)
{
    owners.erase(node);
    if(owners.empty()){
        sigGraphConnection_(false);
    }
}


SgObject* SgNode::clone(SgCloneMap& cloneMap) const
{
    return new SgNode(*this);
}


void SgNode::accept(SceneVisitor& visitor)
{
    visitor.visitNode(this);
}


const BoundingBox& SgNode::boundingBox() const
{
    static const BoundingBox bbox; // empty one
    return bbox;
}


bool SgNode::isGroup() const
{
    return false;
}


SgGroup::SgGroup()
{
    isBboxCacheValid = false;
}


SgGroup::SgGroup(const SgGroup& org)
    : SgNode(org)
{
    children.reserve(org.numChildren());

    for(const_iterator p = org.begin(); p != org.end(); ++p){
        addChild(*p, false);
    }

    isBboxCacheValid = true;
    bboxCache = org.bboxCache;
}


SgGroup::SgGroup(const SgGroup& org, SgCloneMap& cloneMap)
    : SgNode(org)
{
    children.reserve(org.numChildren());

    for(const_iterator p = org.begin(); p != org.end(); ++p){
        addChild(cloneMap.getClone<SgNode>(p->get()), false);
    }

    isBboxCacheValid = true;
    bboxCache = org.bboxCache;
}


SgGroup::~SgGroup()
{
    for(const_iterator p = begin(); p != end(); ++p){
        (*p)->removeOwner(this);
    }
}


SgObject* SgGroup::clone(SgCloneMap& cloneMap) const
{
    return new SgGroup(*this, cloneMap);
}


int SgGroup::numElements() const
{
    return children.size();
}


SgObject* SgGroup::element(int index)
{
    return children[index].get();
}


void SgGroup::accept(SceneVisitor& visitor)
{
    visitor.visitGroup(this);
}


void SgGroup::transferUpdate(SgUpdate& update)
{
    //if(update.action() & SgUpdate::BBOX_UPDATED){
    invalidateBoundingBox();
    SgNode::transferUpdate(update);
    //}
}


const BoundingBox& SgGroup::boundingBox() const
{
    if(isBboxCacheValid){
        return bboxCache;
    }
    bboxCache.clear();
    for(const_iterator p = begin(); p != end(); ++p){
        bboxCache.expandBy((*p)->boundingBox());
    }
    isBboxCacheValid = true;
    return bboxCache;
}


bool SgGroup::isGroup() const
{
    return true;
}


bool SgGroup::contains(SgNode* node) const
{
    for(const_iterator p = begin(); p != end(); ++p){
        if((*p) == node){
            return true;
        }
    }
    return false;
}


void SgGroup::clearChildren(bool doNotify)
{
    if(doNotify){
        SgUpdate update(SgUpdate::REMOVED);
        for(const_iterator p = begin(); p != end(); ++p){
            (*p)->notifyUpdate(update);
        }
    }
    children.clear();
}


void SgGroup::addChild(SgNode* node, bool doNotify)
{
    if(node){
        children.push_back(node);
        if(doNotify){
            SgUpdate update(SgUpdate::ADDED);
            node->addOwner(this, update);
        } else {
            node->addOwner(this);
        }
    }
}


bool SgGroup::removeChild(SgNode* node, bool doNotify)
{
    bool removed = false;
    iterator p = children.begin();
    while(p != children.end()){
        if((*p) == node){
            SgNode* child = *p;
            if(doNotify){
                child->notifyUpdate(SgUpdate::REMOVED);
            }
            p = children.erase(p);
            child->removeOwner(this);
            removed = true;
        } else {
            ++p;
        }
    }
    return removed;
}
    

void SgGroup::removeChildAt(int index, bool doNotify)
{
    iterator p = children.begin() + index;
    if(doNotify){
        (*p)->notifyUpdate(SgUpdate::REMOVED);
    }
    (*p)->removeOwner(this);
    children.erase(p);
}


SgInvariantGroup::SgInvariantGroup()
{

}


SgInvariantGroup::SgInvariantGroup(const SgInvariantGroup& org)
    : SgGroup(org)
{

}


SgInvariantGroup::SgInvariantGroup(const SgInvariantGroup& org, SgCloneMap& cloneMap)
    : SgGroup(org, cloneMap)
{

}


SgObject* SgInvariantGroup::clone(SgCloneMap& cloneMap) const
{
    return new SgInvariantGroup(*this, cloneMap);
}


void SgInvariantGroup::accept(SceneVisitor& visitor)
{
    visitor.visitInvariantGroup(this);
}


SgTransform::SgTransform()
{

}


SgTransform::SgTransform(const SgTransform& org)
    : SgGroup(org)
{
    untransformedBboxCache = org.untransformedBboxCache;
}
    

SgTransform::SgTransform(const SgTransform& org, SgCloneMap& cloneMap)
    : SgGroup(org, cloneMap)
{
    untransformedBboxCache = org.untransformedBboxCache;
}


const BoundingBox& SgTransform::untransformedBoundingBox() const
{
    if(!isBboxCacheValid){
        boundingBox();
    }
    return untransformedBboxCache;
}


SgPosTransform::SgPosTransform()
    : T_(Affine3::Identity())
{

}


SgPosTransform::SgPosTransform(const Affine3& T)
    : T_(T)
{

}


SgPosTransform::SgPosTransform(const SgPosTransform& org)
    : SgTransform(org),
      T_(org.T_)
{

}


SgPosTransform::SgPosTransform(const SgPosTransform& org, SgCloneMap& cloneMap)
    : SgTransform(org, cloneMap),
      T_(org.T_)
{

}


SgObject* SgPosTransform::clone(SgCloneMap& cloneMap) const
{
    return new SgPosTransform(*this, cloneMap);
}


void SgPosTransform::accept(SceneVisitor& visitor)
{
    visitor.visitPosTransform(this);
}


const BoundingBox& SgPosTransform::boundingBox() const
{
    if(isBboxCacheValid){
        return bboxCache;
    }
    bboxCache.clear();
    for(const_iterator p = begin(); p != end(); ++p){
        bboxCache.expandBy((*p)->boundingBox());
    }
    untransformedBboxCache = bboxCache;
    bboxCache.transform(T_);
    isBboxCacheValid = true;
    return bboxCache;
}


void SgPosTransform::getTransform(Affine3& out_T) const
{
    out_T = T_;
}


SgScaleTransform::SgScaleTransform()
{
    scale_.setOnes();
}


SgScaleTransform::SgScaleTransform(const SgScaleTransform& org)
    : SgTransform(org),
      scale_(org.scale_)
{

}
      
    
SgScaleTransform::SgScaleTransform(const SgScaleTransform& org, SgCloneMap& cloneMap)
    : SgTransform(org, cloneMap),
      scale_(org.scale_)
{

}


SgObject* SgScaleTransform::clone(SgCloneMap& cloneMap) const
{
    return new SgScaleTransform(*this, cloneMap);
}


void SgScaleTransform::accept(SceneVisitor& visitor)
{
    visitor.visitScaleTransform(this);
}


const BoundingBox& SgScaleTransform::boundingBox() const
{
    if(isBboxCacheValid){
        return bboxCache;
    }
    bboxCache.clear();
    for(const_iterator p = begin(); p != end(); ++p){
        bboxCache.expandBy((*p)->boundingBox());
    }
    untransformedBboxCache = bboxCache;
    bboxCache.transform(Affine3(scale_.asDiagonal()));
    isBboxCacheValid = true;
    return bboxCache;
}


void SgScaleTransform::getTransform(Affine3& out_T) const
{
    out_T = scale_.asDiagonal();
}


SgUnpickableGroup::SgUnpickableGroup()
{

}


SgUnpickableGroup::SgUnpickableGroup(const SgUnpickableGroup& org)
    : SgGroup(org)
{

}


SgUnpickableGroup::SgUnpickableGroup(const SgUnpickableGroup& org, SgCloneMap& cloneMap)
    : SgGroup(org, cloneMap)
{

}


SgObject* SgUnpickableGroup::clone(SgCloneMap& cloneMap) const
{
    return new SgUnpickableGroup(*this, cloneMap);
}


void SgUnpickableGroup::accept(SceneVisitor& visitor)
{
    visitor.visitUnpickableGroup(this);
}


SgMaterial::SgMaterial()
{
    ambientIntensity_ = 0.02;
    diffuseColor_ << 0.8, 0.8, 0.8;
    emissiveColor_.setZero();
    specularColor_.setZero();
    shininess_ = 0.2;
    transparency_ = 0.0;
}


SgMaterial::SgMaterial(const SgMaterial& org)
    : SgObject(org)
{
    ambientIntensity_ = org.ambientIntensity_;
    diffuseColor_ = org.diffuseColor_;
    emissiveColor_ = org.emissiveColor_;
    specularColor_ = org.specularColor_;
    shininess_ = org.shininess_;
    transparency_ = org.transparency_;
}


SgObject* SgMaterial::clone(SgCloneMap& cloneMap) const
{
    return new SgMaterial(*this);
}


SgImage::SgImage()
    : image_(make_shared<Image>())
{

}


SgImage::SgImage(const Image& image)
    : image_(make_shared<Image>(image))
{

}


SgImage::SgImage(boost::shared_ptr<Image> sharedImage)
    : image_(sharedImage)
{

}


SgImage::SgImage(const SgImage& org)
    : SgObject(org),
      image_(org.image_)
{

}


SgObject* SgImage::clone(SgCloneMap& cloneMap) const
{
    return new SgImage(*this);
}


Image& SgImage::image()
{
    if(image_.use_count() > 1){
        image_ = make_shared<Image>(*image_);
    }
    return *image_;
}


unsigned char* SgImage::pixels()
{
    if(image_.use_count() > 1){
        image_ = make_shared<Image>(*image_);
    }
    return image_->pixels();
}


void SgImage::setSize(int width, int height, int nComponents)
{
    image().setSize(width, height, nComponents);
}


void SgImage::setSize(int width, int height)
{
    image().setSize(width, height);
}


SgTextureTransform::SgTextureTransform()
{
    center_ << 0.0, 0.0; 
    rotation_ = 0;
    scale_ << 1.0, 1.0;
    translation_ << 0.0, 0.0;
}


SgTextureTransform::SgTextureTransform(const SgTextureTransform& org)
    : SgObject(org)
{
    center_ = org.center_;
    rotation_ = org.rotation_;
    scale_ = org.scale_;
    translation_ = org.translation_;
}


SgObject* SgTextureTransform::clone(SgCloneMap& cloneMap) const
{
    return new SgTextureTransform(*this);
}


SgTexture::SgTexture()
{
    repeatS_ = true; 
    repeatT_ = true; 
}


SgTexture::SgTexture(const SgTexture& org, SgCloneMap& cloneMap)
    : SgObject(org)
{
    if(cloneMap.isNonNodeCloningEnabled()){
        if(org.image()){
            setImage(cloneMap.getClone<SgImage>(org.image()));
        }
        if(org.textureTransform()){
            setTextureTransform(cloneMap.getClone<SgTextureTransform>(org.textureTransform()));
        }
    } else {
        setImage(const_cast<SgImage*>(org.image()));
        setTextureTransform(const_cast<SgTextureTransform*>(org.textureTransform()));
    }
    
    repeatS_ = org.repeatS_;
    repeatT_ = org.repeatT_;
}


SgObject* SgTexture::clone(SgCloneMap& cloneMap) const
{
    return new SgTexture(*this, cloneMap);
}


int SgTexture::numElements() const
{
    int n = 0;
    if(image_) ++n;
    if(textureTransform_) ++n;
    return n;
}


SgObject* SgTexture::element(int index)
{
    SgObject* elements[2] = { 0, 0 };
    int i = 0;
    if(image_) elements[i++] = image_;
    if(textureTransform_) elements[i++] = textureTransform_;
    return elements[index];
}


SgImage* SgTexture::setImage(SgImage* image)
{
    if(image_){
        image_->removeOwner(this);
    }
    image_ = image;
    if(image){
        image->addOwner(this);
    }
    return image;
}


SgImage* SgTexture::getOrCreateImage()
{
    if(!image_){
        setImage(new SgImage);
    }
    return image_;
}    


SgTextureTransform* SgTexture::setTextureTransform(SgTextureTransform* textureTransform)
{
    if(textureTransform_){
        textureTransform_->removeOwner(this);
    }
    textureTransform_ = textureTransform;
    if(textureTransform){
        textureTransform->addOwner(this);
    }
    return textureTransform;
}


SgMeshBase::SgMeshBase()
{
    isSolid_ = false;
}


SgMeshBase::SgMeshBase(const SgMeshBase& org, SgCloneMap& cloneMap)
    : SgObject(org),
      normalIndices_(org.normalIndices_),
      colorIndices_(org.colorIndices_)
{
    if(cloneMap.isNonNodeCloningEnabled()){
        if(org.vertices_){
            setVertices(cloneMap.getClone<SgVertexArray>(org.vertices()));
        }
        if(org.normals_){
            setNormals(cloneMap.getClone<SgNormalArray>(org.normals()));
        }
        if(org.colors_){
            setColors(cloneMap.getClone<SgColorArray>(org.colors()));
        }
    } else {
        setVertices(const_cast<SgVertexArray*>(org.vertices()));
        setNormals(const_cast<SgNormalArray*>(org.normals()));
        setColors(const_cast<SgColorArray*>(org.colors()));
    }
    isSolid_ = org.isSolid_;
    bbox = org.bbox;
}

    
int SgMeshBase::numElements() const
{
    int n = 0;
    if(vertices_) ++n;
    if(normals_) ++n;
    if(colors_) ++n;
    return n;
}


SgObject* SgMeshBase::element(int index)
{
    SgObject* elements[3] = { 0, 0, 0 };
    int i = 0;
    if(vertices_) elements[i++] = vertices_.get();
    if(normals_) elements[i++] = normals_.get();
    if(colors_) elements[i++] = colors_.get();
    return elements[index];
}


const BoundingBox& SgMeshBase::boundingBox() const
{
    return bbox;
}


void SgMeshBase::updateBoundingBox()
{
    if(!vertices_){
        bbox.clear();
    } else {
        BoundingBoxf bboxf;
        for(SgVertexArray::const_iterator p = vertices_->begin(); p != vertices_->end(); ++p){
            bboxf.expandBy(*p);
        }
        bbox = bboxf;
    }
}


SgVertexArray* SgMeshBase::setVertices(SgVertexArray* vertices)
{
    if(vertices_){
        vertices_->removeOwner(this);
    }
    vertices_ = vertices;
    if(vertices){
        vertices->addOwner(this);
    }
    return vertices;
}


SgVertexArray* SgMeshBase::getOrCreateVertices()
{
    if(!vertices_){
        setVertices(new SgVertexArray);
    }
    return vertices_;
}


SgNormalArray* SgMeshBase::setNormals(SgNormalArray* normals)
{
    if(normals_){
        normals_->removeOwner(this);
    }
    normals_ = normals;
    if(normals){
        normals->addOwner(this);
    }
    return normals;
}


SgNormalArray* SgMeshBase::getOrCreateNormals()
{
    if(!normals_){
        setNormals(new SgNormalArray);
    }
    return normals_;
}


SgColorArray* SgMeshBase::setColors(SgColorArray* colors)
{
    if(colors_){
        colors_->removeOwner(this);
    }
    colors_ = colors;
    if(colors){
        colors->addOwner(this);
    }
    return colors;
}


SgColorArray* SgMeshBase::getOrCreateColors()
{
    if(!colors_){
        setColors(new SgColorArray);
    }
    return colors_;
}


SgTexCoordArray* SgMeshBase::setTexCoords(SgTexCoordArray* texCoords)
{
    if(texCoords_){
        texCoords_->removeOwner(this);
    }
    texCoords_ = texCoords;
    if(texCoords){
        texCoords->addOwner(this);
    }
    return texCoords;
}


SgMesh::SgMesh()
{

}


SgMesh::SgMesh(const SgMesh& org, SgCloneMap& cloneMap)
    : SgMeshBase(org, cloneMap),
      triangleVertices_(org.triangleVertices_),
      primitive_(org.primitive_)
{

}


SgObject* SgMesh::clone(SgCloneMap& cloneMap) const
{
    return new SgMesh(*this, cloneMap);
}


bool SgMesh::setBox(Vector3 size)
{
    if(size.x() < 0.0 || size.y() < 0.0 || size.z() < 0.0){
        return false;
    }
    
    const float x = size.x() * 0.5;
    const float y = size.y() * 0.5;
    const float z = size.z() * 0.5;

    SgVertexArray& vertices = *setVertices(new SgVertexArray());
    vertices.reserve(8);

    vertices.push_back(Vector3f( x, y, z));
    vertices.push_back(Vector3f(-x, y, z));
    vertices.push_back(Vector3f(-x,-y, z));
    vertices.push_back(Vector3f( x,-y, z));
    vertices.push_back(Vector3f( x, y,-z));
    vertices.push_back(Vector3f(-x, y,-z));
    vertices.push_back(Vector3f(-x,-y,-z));
    vertices.push_back(Vector3f( x,-y,-z));

    triangleVertices_.clear();
    triangleVertices_.reserve(12);
    addTriangle(0,1,2);
    addTriangle(2,3,0);
    addTriangle(0,5,1);
    addTriangle(0,4,5);
    addTriangle(1,5,6);
    addTriangle(1,6,2);
    addTriangle(2,6,7);
    addTriangle(2,7,3);
    addTriangle(3,7,4);
    addTriangle(3,4,0);
    addTriangle(4,6,5);
    addTriangle(4,7,6);

    primitive_ = Box(size);

    MeshNormalGenerator generator;
    generator.generateNormals(this, 0.0);

    updateBoundingBox();

    return true;
}


bool SgMesh::setSphere(double radius, int divisionNumber)
{
    if(radius < 0.0 || divisionNumber < 4){
        return false;
    }
    
    const int vdn = divisionNumber / 2;  // latitudinal division number
    const int hdn = divisionNumber;      // longitudinal division number

    SgVertexArray& vertices = *setVertices(new SgVertexArray());
    vertices.reserve((vdn - 1) * hdn + 2);

    for(int i=1; i < vdn; i++){ // latitudinal direction
        const double tv = i * PI / vdn;
        for(int j=0; j < hdn; j++){ // longitudinal direction
            const double th = j * 2.0 * PI / hdn;
            vertices.push_back(Vector3f(radius * sin(tv) * cos(th), radius * cos(tv), radius * sin(tv) * sin(th)));
        }
    }
    
    const int topIndex  = vertices.size();
    vertices.push_back(Vector3f(0.0f,  radius, 0.0f));
    const int bottomIndex = vertices.size();
    vertices.push_back(Vector3f(0.0f, -radius, 0.0f));

    triangleVertices_.clear();
    triangleVertices_.reserve(vdn * hdn * 2 * 3);

    // top faces
    for(int i=0; i < hdn; ++i){
        addTriangle(topIndex, (i+1) % hdn, i);
    }

    // side faces
    for(int i=0; i < vdn - 2; ++i){
        const int upper = i * hdn;
        const int lower = (i + 1) * hdn;
        for(int j=0; j < hdn; ++j) {
            // upward convex triangle
            addTriangle(j + upper, ((j + 1) % hdn) + lower, j + lower);
            // downward convex triangle
            addTriangle(j + upper, ((j + 1) % hdn) + upper, ((j + 1) % hdn) + lower);
        }
    }
    
    // bottom faces
    const int offset = (vdn - 2) * hdn;
    for(int i=0; i < hdn; ++i){
        addTriangle(bottomIndex, (i % hdn) + offset, ((i+1) % hdn) + offset);
    }

    primitive_ = Sphere(radius);

    //! \todo set normals directly without using the following function
    MeshNormalGenerator generator;
    generator.generateNormals(this, PI);

    updateBoundingBox();
    
    return true;
}


bool SgMesh::setCylinder(double radius, double height, bool bottom, bool side, bool top, int divisionNumber)
{
    if(height < 0.0 || radius < 0.0){
        return false;
    }
    
    SgVertexArray& vertices = *setVertices(new SgVertexArray());
    vertices.resize(divisionNumber * 2);

    const double y = height / 2.0;
    for(int i=0 ; i < divisionNumber ; i++ ){
        const double angle = i * 2.0 * PI / divisionNumber;
        Vector3f& vtop = vertices[i];
        Vector3f& vbottom = vertices[i + divisionNumber];
        vtop[0] = vbottom[0] = radius * cos(angle);
        vtop[2] = vbottom[2] = radius * sin(angle);
        vtop[1]    =  y;
        vbottom[1] = -y;
    }

    const int topCenterIndex = vertices.size();
    vertices.push_back(Vector3f(0.0f,  y, 0.0f));
    const int bottomCenterIndex = vertices.size();
    vertices.push_back(Vector3f(0.0f, -y, 0.0f));

    triangleVertices_.clear();
    triangleVertices_.reserve((divisionNumber * 4) * 4);

    for(int i=0; i < divisionNumber; ++i){
        // top face
        if(top){
            addTriangle(topCenterIndex, (i+1) % divisionNumber, i);
        }
        // side face (upward convex triangle)
        if(side){        
            addTriangle(i, ((i+1) % divisionNumber) + divisionNumber, i + divisionNumber);
            // side face (downward convex triangle)
            addTriangle(i, (i+1) % divisionNumber, ((i + 1) % divisionNumber) + divisionNumber);
        }
        // bottom face
        if(bottom){
            addTriangle(bottomCenterIndex, i + divisionNumber, ((i+1) % divisionNumber) + divisionNumber);
        }
    }

    primitive_ = Cylinder(radius, height);

    MeshNormalGenerator generator;
    generator.generateNormals(this, PI / 2.0);

    updateBoundingBox();

    return true;
}


bool SgMesh::setCone(double radius, double height, bool bottom, bool side, int divisionNumber)
{
    if(radius < 0.0 || height < 0.0){
        return false;
    }
    
    SgVertexArray& vertices = *setVertices(new SgVertexArray());
    vertices.reserve(divisionNumber + 1);

    for(int i=0;  i < divisionNumber; ++i){
        const double angle = i * 2.0 * PI / divisionNumber;
        vertices.push_back(Vector3f(radius * cos(angle), -height/2.0, radius * sin(angle)));
    }

    const int topIndex = vertices.size();
    vertices.push_back(Vector3f(0.0f, height / 2.0, 0.0f));
    const int bottomCenterIndex = vertices.size();
    vertices.push_back(Vector3f(0.0f, -height / 2.0, 0.0f));

    triangleVertices_.clear();
    triangleVertices_.reserve((divisionNumber * 2) * 4);

    for(int i=0; i < divisionNumber; ++i){
        // side faces
        if(side){
            addTriangle(topIndex, (i + 1) % divisionNumber, i);
        }
        // bottom faces
        if(bottom){
            addTriangle(bottomCenterIndex, i, (i + 1) % divisionNumber);
        }
    }

    primitive_ = Cone(radius, height);

    MeshNormalGenerator generator;
    generator.generateNormals(this, PI / 2.0);

    updateBoundingBox();

    return true;
}


SgPolygonMesh::SgPolygonMesh()
{

}


SgPolygonMesh::SgPolygonMesh(const SgPolygonMesh& org, SgCloneMap& cloneMap)
    : SgMeshBase(org, cloneMap),
      polygonVertices_(org.polygonVertices_)
{

}
    

SgObject* SgPolygonMesh::clone(SgCloneMap& cloneMap) const
{
    return new SgPolygonMesh(*this, cloneMap);
}


SgShape::SgShape()
{

}


SgShape::SgShape(const SgShape& org, SgCloneMap& cloneMap)
    : SgNode(org)
{
    if(cloneMap.isNonNodeCloningEnabled()){
        if(org.mesh()){
            setMesh(cloneMap.getClone<SgMesh>(org.mesh()));
        }
        if(org.material()){
            setMaterial(cloneMap.getClone<SgMaterial>(org.material()));
        }
        if(org.texture()){
            setTexture(cloneMap.getClone<SgTexture>(org.texture()));
        }
    } else {
        setMesh(const_cast<SgMesh*>(org.mesh()));
        setMaterial(const_cast<SgMaterial*>(org.material()));
        setTexture(const_cast<SgTexture*>(org.texture()));
    }
}


SgObject* SgShape::clone(SgCloneMap& cloneMap) const
{
    return new SgShape(*this, cloneMap);
}


int SgShape::numElements() const
{
    int n = 0;
    if(mesh_) ++n;
    if(material_) ++n;
    if(texture_) ++n;
    return n;
}


SgObject* SgShape::element(int index)
{
    SgObject* elements[3] = { 0, 0, 0 };
    int i = 0;
    if(mesh_) elements[i++] = mesh_.get();
    if(material_) elements[i++] = material_.get();
    if(texture_) elements[i++] = texture_.get();
    return elements[index];
}


void SgShape::accept(SceneVisitor& visitor)
{
    visitor.visitShape(this);
}


const BoundingBox& SgShape::boundingBox() const
{
    if(mesh()){
        return mesh()->boundingBox();
    }
    return SgNode::boundingBox();
}


SgMesh* SgShape::setMesh(SgMesh* mesh)
{
    if(mesh_){
        mesh_->removeOwner(this);
    }
    mesh_ = mesh;
    if(mesh){
        mesh->addOwner(this);
    }
    return mesh;
}


SgMesh* SgShape::getOrCreateMesh()
{
    if(!mesh_){
        setMesh(new SgMesh);
    }
    return mesh_;
}


SgMaterial* SgShape::setMaterial(SgMaterial* material)
{
    if(material_){
        material_->removeOwner(this);
    }
    material_ = material;
    if(material){
        material->addOwner(this);
    }
    return material;
}


SgMaterial* SgShape::getOrCreateMaterial()
{
    if(!material_){
        setMaterial(new SgMaterial);
    }
    return material_;
}


SgTexture* SgShape::setTexture(SgTexture* texture)
{
    if(texture_){
        texture_->removeOwner(this);
    }
    texture_ = texture;
    if(texture){
        texture->addOwner(this);
    }
    return texture;
}


SgTexture* SgShape::getOrCreateTexture()
{
    if(!texture_){
        setTexture(new SgTexture);
    }
    return texture_;
}


SgPlot::SgPlot()
{

}
        

SgPlot::SgPlot(const SgPlot& org, SgCloneMap& cloneMap)
    : SgNode(org)
{
    if(cloneMap.isNonNodeCloningEnabled()){
        if(org.vertices()){
            setVertices(cloneMap.getClone<SgVertexArray>(org.vertices()));
        }
        if(org.colors()){
            setColors(cloneMap.getClone<SgColorArray>(org.colors()));
        }
        if(org.material()){
            setMaterial(cloneMap.getClone<SgMaterial>(org.material()));
        }
    } else {
        setVertices(const_cast<SgVertexArray*>(org.vertices()));
        setColors(const_cast<SgColorArray*>(org.colors()));
        setMaterial(const_cast<SgMaterial*>(org.material()));
    }
    normalIndices_ = org.normalIndices_;
    colorIndices_ = org.colorIndices_;
    bbox = org.bbox;
}


int SgPlot::numElements() const
{
    int n = 0;
    if(vertices_) ++n;
    if(colors_) ++n;
    return n;
}
    

SgObject* SgPlot::element(int index)
{
    SgObject* elements[2] = { 0, 0 };
    int i = 0;
    if(vertices_) elements[i++] = vertices_.get();
    if(colors_) elements[i++] = colors_.get();
    return elements[index];
}
    

const BoundingBox& SgPlot::boundingBox() const
{
    return bbox;
}


void SgPlot::updateBoundingBox()
{
    if(!vertices_){
        bbox.clear();
    } else {
        BoundingBoxf bboxf;
        for(SgVertexArray::const_iterator p = vertices_->begin(); p != vertices_->end(); ++p){
            bboxf.expandBy(*p);
        }
        bbox = bboxf;
    }
}


SgVertexArray* SgPlot::setVertices(SgVertexArray* vertices)
{
    if(vertices_){
        vertices_->removeOwner(this);
    }
    vertices_ = vertices;
    if(vertices){
        vertices->addOwner(this);
    }
    return vertices;
}


SgVertexArray* SgPlot::getOrCreateVertices()
{
    if(!vertices_){
        setVertices(new SgVertexArray);
    }
    return vertices_;
}


SgNormalArray* SgPlot::setNormals(SgNormalArray* normals)
{
    if(normals_){
        normals_->removeOwner(this);
    }
    normals_ = normals;
    if(normals){
        normals->addOwner(this);
    }
    return normals;
}


SgNormalArray* SgPlot::getOrCreateNormals()
{
    if(!normals_){
        setNormals(new SgNormalArray);
    }
    return normals_;
}


SgMaterial* SgPlot::setMaterial(SgMaterial* material)
{
    if(material_){
        material_->removeOwner(this);
    }
    material_ = material;
    if(material){
        material->addOwner(this);
    }
    return material;
}


SgColorArray* SgPlot::setColors(SgColorArray* colors)
{
    if(colors_){
        colors_->removeOwner(this);
    }
    colors_ = colors;
    if(colors){
        colors->addOwner(this);
    }
    return colors;
}


SgColorArray* SgPlot::getOrCreateColors()
{
    if(!colors_){
        setColors(new SgColorArray);
    }
    return colors_;
}


SgPointSet::SgPointSet()
{
    pointSize_ = 0.0;
}


SgPointSet::SgPointSet(const SgPointSet& org, SgCloneMap& cloneMap)
    : SgPlot(org, cloneMap)
{
    pointSize_ = org.pointSize_;
}


SgObject* SgPointSet::clone(SgCloneMap& cloneMap) const
{
    return new SgPointSet(*this, cloneMap);
}


void SgPointSet::accept(SceneVisitor& visitor)
{
    visitor.visitPointSet(this);
}

   
SgLineSet::SgLineSet()
{
    lineWidth_ = 0.0;
}


SgLineSet::SgLineSet(const SgLineSet& org, SgCloneMap& cloneMap)
    : SgPlot(org, cloneMap)
{
    lineWidth_ = org.lineWidth_;
}

    
SgObject* SgLineSet::clone(SgCloneMap& cloneMap) const
{
    return new SgLineSet(*this, cloneMap);
}
    

void SgLineSet::accept(SceneVisitor& visitor)
{
    visitor.visitLineSet(this);
}


SgPreprocessed::SgPreprocessed()
{

}


SgPreprocessed::SgPreprocessed(const SgPreprocessed& org)
    : SgNode(org)
{

}


SgObject* SgPreprocessed::clone(SgCloneMap& cloneMap) const
{
    return new SgPreprocessed(*this);
}


void SgPreprocessed::accept(SceneVisitor& visitor)
{
    visitor.visitPreprocessed(this);
}


SgLight::SgLight()
{
    on_ = true;
    color_.setOnes();
    intensity_ = 1.0f;
    ambientIntensity_ = 0.0f;
}


SgLight::SgLight(const SgLight& org)
    : SgPreprocessed(org)
{
    on_ = org.on_;
    color_ = org.color_;
    intensity_ = org.intensity_;
    ambientIntensity_ = org.ambientIntensity_;
}


SgObject* SgLight::clone(SgCloneMap& cloneMap) const
{
    return new SgLight(*this);
}


void SgLight::accept(SceneVisitor& visitor)
{
    visitor.visitLight(this);
}


SgDirectionalLight::SgDirectionalLight()
{
    direction_ << 0.0, 0.0, -1.0;
}


SgDirectionalLight::SgDirectionalLight(const SgDirectionalLight& org)
    : SgLight(org)
{
    direction_ = org.direction_;
}


SgObject* SgDirectionalLight::clone(SgCloneMap& cloneMap) const
{
    return new SgDirectionalLight(*this);
}


void SgDirectionalLight::accept(SceneVisitor& visitor)
{
    visitor.visitLight(this);
}


SgPointLight::SgPointLight()
{
    constantAttenuation_ = 1.0f;
    linearAttenuation_ = 0.0f;
    quadraticAttenuation_ = 0.0f;
}


SgPointLight::SgPointLight(const SgPointLight& org)
    : SgLight(org)
{
    constantAttenuation_ = org.constantAttenuation_;
    linearAttenuation_ = org.linearAttenuation_;
    quadraticAttenuation_ = org.quadraticAttenuation_;
}


SgObject* SgPointLight::clone(SgCloneMap& cloneMap) const
{
    return new SgPointLight(*this);
}


void SgPointLight::accept(SceneVisitor& visitor)
{
    visitor.visitLight(this);
}


SgSpotLight::SgSpotLight()
{
    direction_ << 0.0, 0.0, -1.0;
    beamWidth_ = 1.570796f;
    cutOffAngle_ = 0.785398f;
}


SgSpotLight::SgSpotLight(const SgSpotLight& org)
    : SgPointLight(org)
{
    direction_ = org.direction_;
    beamWidth_ = org.beamWidth_;
    cutOffAngle_ = org.cutOffAngle_;
}


SgObject* SgSpotLight::clone(SgCloneMap& cloneMap) const
{
    return new SgSpotLight(*this);
}


void SgSpotLight::accept(SceneVisitor& visitor)
{
    visitor.visitLight(this);
}


SgCamera::SgCamera()
{
    nearDistance_ = 0.01;
    farDistance_ = 100.0;
}


SgCamera::SgCamera(const SgCamera& org)
    : SgPreprocessed(org)
{
    nearDistance_ = org.nearDistance_;
    farDistance_ = org.farDistance_;
}


void SgCamera::accept(SceneVisitor& visitor)
{
    visitor.visitCamera(this);
}


Affine3 SgCamera::positionLookingFor(const Vector3& eye, const Vector3& direction, const Vector3& up)
{
    Vector3 d = direction.normalized();
    Vector3 c = d.cross(up).normalized();
    Vector3 u = c.cross(d);
    Affine3 C;
    C.linear() << c, u, -d;
    C.translation() = eye;
    return C;
}


Affine3 SgCamera::positionLookingAt(const Vector3& eye, const Vector3& center, const Vector3& up)
{
    return positionLookingFor(eye, (center - eye), up);
}


SgPerspectiveCamera::SgPerspectiveCamera()
{
    fieldOfView_ = 0.785398;
}


SgPerspectiveCamera::SgPerspectiveCamera(const SgPerspectiveCamera& org)
    : SgCamera(org)
{
    fieldOfView_ = org.fieldOfView_;
}


SgObject* SgPerspectiveCamera::clone(SgCloneMap& cloneMap) const
{
    return new SgPerspectiveCamera(*this);
}


void SgPerspectiveCamera::accept(SceneVisitor& visitor)
{
    visitor.visitCamera(this);
}


/**
   @param aspectRatio width / height
*/
double SgPerspectiveCamera::fovy(double aspectRatio, double fieldOfView)
{
    if(aspectRatio >= 1.0){
        return fieldOfView;
    } else {
        return 2.0 * atan(tan(fieldOfView / 2.0) / aspectRatio);
    }
}


SgOrthographicCamera::SgOrthographicCamera()
{
    height_ = 2.0;
}


SgOrthographicCamera::SgOrthographicCamera(const SgOrthographicCamera& org)
    : SgCamera(org)
{
    height_ = org.height_;
}


SgObject* SgOrthographicCamera::clone(SgCloneMap& cloneMap) const
{
    return new SgOrthographicCamera(*this);
}


void SgOrthographicCamera::accept(SceneVisitor& visitor)
{
    visitor.visitCamera(this);
}


SgFog::SgFog()
{

}


SgFog::SgFog(const SgFog& org)
    : SgPreprocessed(org)
{

}


SgObject* SgFog::clone(SgCloneMap& cloneMap) const
{
    return new SgFog(*this);
}


void SgFog::accept(SceneVisitor& visitor)
{
    
}


SgOverlay::SgOverlay()
{

}


SgOverlay::SgOverlay(const SgOverlay& org, SgCloneMap& cloneMap)
    : SgGroup(org, cloneMap)
{

}


SgOverlay::~SgOverlay()
{

}


SgObject* SgOverlay::clone(SgCloneMap& cloneMap) const
{
    return new SgOverlay(*this, cloneMap);
}


void SgOverlay::accept(SceneVisitor& visitor)
{
    visitor.visitOverlay(this);
}


void SgOverlay::calcViewVolume(double viewportWidth, double viewportHeight, ViewVolume& io_volume)
{

}