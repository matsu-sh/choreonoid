/*!
  @file
  @author Shin'ichiro Nakaoka
*/

#include "MeshGenerator.h"
#include "MeshNormalGenerator.h"
#include "Triangulator.h"

using namespace std;
using namespace cnoid;

namespace {
const double PI = 3.14159265358979323846;
}

MeshGenerator::MeshGenerator()
{
    isNormanGenerationEnabled_ = true;
    normalGenerator = 0;
    divisionNumber_ = 20;
}


MeshGenerator::~MeshGenerator()
{
    if(normalGenerator){
        delete normalGenerator;
    }
}


void MeshGenerator::setDivisionNumber(int n)
{
    divisionNumber_ = n;
}


int MeshGenerator::divisionNumber() const
{
    return divisionNumber_;
}


void MeshGenerator::enableNormalGeneration(bool on)
{
    isNormanGenerationEnabled_ = on;
}


bool MeshGenerator::isNormanGenerationEnabled() const
{
    return isNormanGenerationEnabled_;
}


void MeshGenerator::generateNormals(SgMesh* mesh, double creaseAngle)
{
    if(isNormanGenerationEnabled_){
        if(!normalGenerator){
            normalGenerator = new MeshNormalGenerator;
        }
        normalGenerator->generateNormals(mesh, creaseAngle);
    }
}


SgMesh* MeshGenerator::generateBox(Vector3 size)
{
    if(size.x() < 0.0 || size.y() < 0.0 || size.z() < 0.0){
        return 0;
    }

    const float x = size.x() * 0.5;
    const float y = size.y() * 0.5;
    const float z = size.z() * 0.5;

    SgMesh* mesh = new SgMesh;
    
    SgVertexArray& vertices = *mesh->setVertices(new SgVertexArray());
    vertices.reserve(8);

    vertices.push_back(Vector3f( x, y, z));
    vertices.push_back(Vector3f(-x, y, z));
    vertices.push_back(Vector3f(-x,-y, z));
    vertices.push_back(Vector3f( x,-y, z));
    vertices.push_back(Vector3f( x, y,-z));
    vertices.push_back(Vector3f(-x, y,-z));
    vertices.push_back(Vector3f(-x,-y,-z));
    vertices.push_back(Vector3f( x,-y,-z));

    mesh->triangleVertices().reserve(12);
    mesh->addTriangle(0,1,2);
    mesh->addTriangle(2,3,0);
    mesh->addTriangle(0,5,1);
    mesh->addTriangle(0,4,5);
    mesh->addTriangle(1,5,6);
    mesh->addTriangle(1,6,2);
    mesh->addTriangle(2,6,7);
    mesh->addTriangle(2,7,3);
    mesh->addTriangle(3,7,4);
    mesh->addTriangle(3,4,0);
    mesh->addTriangle(4,6,5);
    mesh->addTriangle(4,7,6);

    mesh->setPrimitive(SgMesh::Box(size));
    mesh->updateBoundingBox();

    generateNormals(mesh, 0.0);

    return mesh;
}


SgMesh* MeshGenerator::generateSphere(double radius)
{
    if(radius < 0.0 || divisionNumber_ < 4){
        return 0;
    }

    SgMesh* mesh = new SgMesh();
    
    const int vdn = divisionNumber_ / 2;  // latitudinal division number
    const int hdn = divisionNumber_;      // longitudinal division number

    SgVertexArray& vertices = *mesh->setVertices(new SgVertexArray());
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

    mesh->triangleVertices().reserve(vdn * hdn * 2 * 3);

    // top faces
    for(int i=0; i < hdn; ++i){
        mesh->addTriangle(topIndex, (i+1) % hdn, i);
    }

    // side faces
    for(int i=0; i < vdn - 2; ++i){
        const int upper = i * hdn;
        const int lower = (i + 1) * hdn;
        for(int j=0; j < hdn; ++j) {
            // upward convex triangle
            mesh->addTriangle(j + upper, ((j + 1) % hdn) + lower, j + lower);
            // downward convex triangle
            mesh->addTriangle(j + upper, ((j + 1) % hdn) + upper, ((j + 1) % hdn) + lower);
        }
    }
    
    // bottom faces
    const int offset = (vdn - 2) * hdn;
    for(int i=0; i < hdn; ++i){
        mesh->addTriangle(bottomIndex, (i % hdn) + offset, ((i+1) % hdn) + offset);
    }

    mesh->setPrimitive(SgMesh::Sphere(radius));
    mesh->updateBoundingBox();

    //! \todo set normals directly without using the following function
    generateNormals(mesh, PI);

    return mesh;
}


SgMesh* MeshGenerator::generateCylinder(double radius, double height, bool bottom, bool side, bool top)
{
    if(height < 0.0 || radius < 0.0){
        return 0;
    }

    SgMesh* mesh = new SgMesh();
    
    SgVertexArray& vertices = *mesh->setVertices(new SgVertexArray());
    vertices.resize(divisionNumber_ * 2);

    const double y = height / 2.0;
    for(int i=0 ; i < divisionNumber_ ; i++ ){
        const double angle = i * 2.0 * PI / divisionNumber_;
        Vector3f& vtop = vertices[i];
        Vector3f& vbottom = vertices[i + divisionNumber_];
        vtop[0] = vbottom[0] = radius * cos(angle);
        vtop[2] = vbottom[2] = radius * sin(angle);
        vtop[1]    =  y;
        vbottom[1] = -y;
    }

    const int topCenterIndex = vertices.size();
    vertices.push_back(Vector3f(0.0f,  y, 0.0f));
    const int bottomCenterIndex = vertices.size();
    vertices.push_back(Vector3f(0.0f, -y, 0.0f));

    mesh->triangleVertices().reserve((divisionNumber_ * 4) * 4);

    for(int i=0; i < divisionNumber_; ++i){
        // top face
        if(top){
            mesh->addTriangle(topCenterIndex, (i+1) % divisionNumber_, i);
        }
        // side face (upward convex triangle)
        if(side){        
            mesh->addTriangle(i, ((i+1) % divisionNumber_) + divisionNumber_, i + divisionNumber_);
            // side face (downward convex triangle)
            mesh->addTriangle(i, (i+1) % divisionNumber_, ((i + 1) % divisionNumber_) + divisionNumber_);
        }
        // bottom face
        if(bottom){
            mesh->addTriangle(bottomCenterIndex, i + divisionNumber_, ((i+1) % divisionNumber_) + divisionNumber_);
        }
    }

    mesh->setPrimitive(SgMesh::Cylinder(radius, height));
    mesh->updateBoundingBox();

    generateNormals(mesh, PI / 2.0);
    
    return mesh;
}


SgMesh* MeshGenerator::generateCone(double radius, double height, bool bottom, bool side)
{
    if(radius < 0.0 || height < 0.0){
        return 0;
    }

    SgMesh* mesh = new SgMesh();
    
    SgVertexArray& vertices = *mesh->setVertices(new SgVertexArray());
    vertices.reserve(divisionNumber_ + 1);

    for(int i=0;  i < divisionNumber_; ++i){
        const double angle = i * 2.0 * PI / divisionNumber_;
        vertices.push_back(Vector3f(radius * cos(angle), -height / 2.0, radius * sin(angle)));
    }

    const int topIndex = vertices.size();
    vertices.push_back(Vector3f(0.0f, height / 2.0, 0.0f));
    const int bottomCenterIndex = vertices.size();
    vertices.push_back(Vector3f(0.0f, -height / 2.0, 0.0f));

    mesh->triangleVertices().reserve((divisionNumber_ * 2) * 4);

    for(int i=0; i < divisionNumber_; ++i){
        // side faces
        if(side){
            mesh->addTriangle(topIndex, (i + 1) % divisionNumber_, i);
        }
        // bottom faces
        if(bottom){
            mesh->addTriangle(bottomCenterIndex, i, (i + 1) % divisionNumber_);
        }
    }

    mesh->setPrimitive(SgMesh::Cone(radius, height));
    mesh->updateBoundingBox();

    generateNormals(mesh, PI / 2.0);

    return mesh;
}


SgMesh* MeshGenerator::generateExtrusion(const Extrusion& extrusion)
{
    const int numSpines = extrusion.spine.size();
    const int numCrosses = extrusion.crossSection.size();
    
    bool isClosed = false;
    if(extrusion.spine[0][0] == extrusion.spine[numSpines - 1][0] &&
       extrusion.spine[0][1] == extrusion.spine[numSpines - 1][1] &&
       extrusion.spine[0][2] == extrusion.spine[numSpines - 1][2] ){
        isClosed = true;
    }
    bool crossSectionisClosed = false;
    if(extrusion.crossSection[0][0] == extrusion.crossSection[numCrosses - 1][0] &&
       extrusion.crossSection[0][1] == extrusion.crossSection[numCrosses - 1][1] ){
        crossSectionisClosed = true;
    }

    SgMesh* mesh = new SgMesh;
    SgVertexArray& vertices = *mesh->setVertices(new SgVertexArray());
    vertices.reserve(numSpines * numCrosses);

    Vector3 preZaxis(Vector3::Zero());
    int definedZaxis = -1;
    vector<Vector3> Yaxisarray;
    vector<Vector3> Zaxisarray;
    if(numSpines > 2){
        for(int i=0; i < numSpines; ++i){
            Vector3 Yaxis, Zaxis;
            if(i == 0){
                if(isClosed){
                    const Vector3& spine1 = extrusion.spine[numSpines - 2];
                    const Vector3& spine2 = extrusion.spine[0];
                    const Vector3& spine3 = extrusion.spine[1];
                    Yaxis = spine3 - spine1;
                    Zaxis = (spine3 - spine2).cross(spine1 - spine2);
                } else {
                    const Vector3& spine1 = extrusion.spine[0];
                    const Vector3& spine2 = extrusion.spine[1];
                    const Vector3& spine3 = extrusion.spine[2];
                    Yaxis = spine2 - spine1;
                    Zaxis = (spine3 - spine2).cross(spine1 - spine2);
                }
            } else if(i == numSpines - 1){
                if(isClosed){
                    const Vector3& spine1 = extrusion.spine[numSpines - 2];
                    const Vector3& spine2 = extrusion.spine[0];
                    const Vector3& spine3 = extrusion.spine[1];
                    Yaxis = spine3 - spine1;
                    Zaxis = (spine3 - spine2).cross(spine1 - spine2);
                } else {
                    const Vector3& spine1 = extrusion.spine[numSpines - 3];
                    const Vector3& spine2 = extrusion.spine[numSpines - 2];
                    const Vector3& spine3 = extrusion.spine[numSpines - 1];
                    Yaxis = spine3 - spine2;
                    Zaxis = (spine3 - spine2).cross(spine1 - spine2);
                }
            } else {
                const Vector3& spine1 = extrusion.spine[i - 1];
                const Vector3& spine2 = extrusion.spine[i];
                const Vector3& spine3 = extrusion.spine[i + 1];
                Yaxis = spine3 - spine1;
                Zaxis = (spine3-spine2).cross(spine1-spine2);
            }
            if(!Zaxis.norm()){
                if(definedZaxis != -1)
                    Zaxis = preZaxis;
            } else {
                if(definedZaxis == -1){
                    definedZaxis = i;
                }
                preZaxis = Zaxis;
            }
            Yaxisarray.push_back(Yaxis);
            Zaxisarray.push_back(Zaxis);
        }
    } else {
        const Vector3 Yaxis(extrusion.spine[1] - extrusion.spine[0]);
        Yaxisarray.push_back(Yaxis);
        Yaxisarray.push_back(Yaxis);
    }

    const int numScales = extrusion.scale.size();
    const int numOrientations = extrusion.orientation.size();
    Vector3 scale(1.0, 0.0, 1.0);
    AngleAxis orientation(0.0, Vector3::UnitZ());

    for(int i=0; i < numSpines; ++i){
        Matrix3 Scp;
        Vector3 y = Yaxisarray[i].normalized();
        if(definedZaxis == -1){
            AngleAxis R(acos(y[1]), Vector3(y[2], 0.0, -y[0]));
            Scp = R.toRotationMatrix();
        } else {
            if(i < definedZaxis){
                Zaxisarray[i] = Zaxisarray[definedZaxis];
            }
            if(i && (Zaxisarray[i].dot(Zaxisarray[i - 1]) < 0.0)){
                Zaxisarray[i] *= -1.0;
            }
            Vector3 z = Zaxisarray[i].normalized();
            Vector3 x = y.cross(z);
            Scp << x, y, z;
        }

        const Vector3& spine = extrusion.spine[i];

        if(numScales == 1){
            scale << extrusion.scale[0][0], 0.0, extrusion.scale[0][1];
        } else if(numScales > 1){
            scale << extrusion.scale[i][0], 0.0, extrusion.scale[i][1];
        }
        if(numOrientations == 1){
            orientation = extrusion.orientation[0];
        } else if(numOrientations > 1){
            orientation = extrusion.orientation[i];
        }

        for(int j=0; j < numCrosses; ++j){
            const Vector3 crossSection(extrusion.crossSection[j][0], 0.0, extrusion.crossSection[j][1]);
            const Vector3 v1(crossSection[0] * scale[0], 0.0, crossSection[2] * scale[2]);
            const Vector3 v = Scp * orientation.toRotationMatrix() * v1 + spine;
            vertices.push_back(v.cast<float>());
        }
    }

    for(int i=0; i < numSpines - 1 ; ++i){
        const int upper = i * numCrosses;
        const int lower = (i + 1) * numCrosses;
        for(int j=0; j < numCrosses - 1; ++j) {
            mesh->addTriangle(j + upper, j + lower, (j + 1) + lower);
            mesh->addTriangle(j + upper, (j + 1) + lower, j + 1 + upper);
        }
    }

    int j = 0;
    if(crossSectionisClosed){
        j = 1;
    }

    Triangulator<SgVertexArray> triangulator;
    vector<int> polygon;
        
    if(extrusion.beginCap && !isClosed){
        triangulator.setVertices(vertices);
        polygon.clear();
        for(int i=0; i < numCrosses - j; ++i){
            polygon.push_back(i);
        }
        triangulator.apply(polygon);
        const vector<int>& triangles = triangulator.triangles();
        for(size_t i=0; i < triangles.size(); i += 3){
            mesh->addTriangle(polygon[triangles[i]], polygon[triangles[i+1]], polygon[triangles[i+2]]);
        }
    }

    if(extrusion.endCap && !isClosed){
        triangulator.setVertices(vertices);
        polygon.clear();
        for(int i=0; i < numCrosses - j; ++i){
            polygon.push_back(numCrosses * (numSpines - 1) + i);
        }
        triangulator.apply(polygon);
        const vector<int>& triangles = triangulator.triangles();
        for(size_t i=0; i < triangles.size(); i +=3){
            mesh->addTriangle(polygon[triangles[i]], polygon[triangles[i+2]], polygon[triangles[i+1]]);
        }
    }

    mesh->updateBoundingBox();

    generateNormals(mesh, extrusion.creaseAngle);

    return mesh;
}


SgLineSet* MeshGenerator::generateExtrusionLineSet(const Extrusion& extrusion, SgMesh* mesh)
{
    const int nc = extrusion.crossSection.size();
    const int ns = extrusion.spine.size();
    if(nc < 4 || ns < 2){
        return 0;
    }
    SgLineSet* lineSet = new SgLineSet;
    lineSet->setVertices(mesh->vertices());

    const int n = ns - 1;
    int o = 0;
    for(int i=0; i < n; ++i){
        for(int j=0; j < nc; ++j){
            lineSet->addLine(o + j, o + (j + 1) % nc);
            lineSet->addLine(o + j, o + j + nc);
        }
        o += nc;
    }
    for(int j=0; j < nc; ++j){
        lineSet->addLine(o + j, o + (j + 1) % nc);
    }

    return lineSet;
}