/**
   @file
   @author Shin'ichiro Nakaoka
*/

#include "PointSetUtil.h"
#include <cnoid/EasyScanner>
#include <cnoid/Exception>
#include <fstream>

using namespace std;
using namespace boost;
using namespace cnoid;

namespace {

enum Element { E_X, E_Y, E_Z, E_NORMAL_X,E_NORMAL_Y, E_NORMAL_Z };

void readPoints(SgPointSet* out_pointSet, EasyScanner& scanner, const std::vector<Element>& elements, int numPoints)
{
    SgVertexArrayPtr vertices = new SgVertexArray();
    vertices->reserve(numPoints);

    const int numElements = elements.size();

    SgNormalArrayPtr normals;
    bool hasNormals = false;
    for(int i=0; i < numElements; ++i){
        if(elements[i] >= E_NORMAL_X){
            hasNormals = true;
            normals = new SgNormalArray();
            normals->reserve(numPoints);
        }
    }

    Vector3f vertex = Vector3f::Zero();
    Vector3f normal = Vector3f::Zero();

    while(!scanner.isEOF()){
        scanner.skipBlankLines();

        bool hasIllegalValue = false;
        for(int i=0; i < numElements; ++i){
            //double value = scanner.readDoubleEx("Illeagel point values");

            if(!scanner.readDouble()){
                // put warning here
                hasIllegalValue = true;
                scanner.skipToLineEnd();
                break;
                    
            } else {
                double value = scanner.doubleValue;
                switch(elements[i]){
                case E_X: vertex.x() = value; break;
                case E_Y: vertex.y() = value; break;
                case E_Z: vertex.z() = value; break;
                case E_NORMAL_X: normal.x() = value; break;
                case E_NORMAL_Y: normal.y() = value; break;
                case E_NORMAL_Z: normal.z() = value; break;
                }
            }
        }
        if(!hasIllegalValue){
            vertices->push_back(vertex);
            if(hasNormals){
                normals->push_back(normal);
            }
        }
        scanner.readLFEOF();
    }

    if(vertices->empty()){
        throw file_read_error() << error_info_message("No valid points");
    } else {
        out_pointSet->setVertices(vertices);
        out_pointSet->setNormals(normals);
        out_pointSet->normalIndices().clear();
        out_pointSet->setColors(0);
        out_pointSet->colorIndices().clear();
    }
}
}


void cnoid::loadPCD(SgPointSet* out_pointSet, const std::string& filename)
{
    try {
        EasyScanner scanner(filename);
        scanner.setCommentChar('#');

        int numPoints = 0;

        std::vector<Element> elements;

        while(true){
            scanner.skipBlankLines();
            scanner.readWordEx("Illegal header key");

            if(scanner.stringValue == "FIELDS"){
                while(scanner.readWord()){
                    if(scanner.stringValue == "x"){
                        elements.push_back(E_X);
                    } else if(scanner.stringValue == "y"){
                        elements.push_back(E_Y);
                    } else if(scanner.stringValue == "z"){
                        elements.push_back(E_Z);
                    } else if(scanner.stringValue == "normal_x"){
                        elements.push_back(E_NORMAL_X);
                    } else if(scanner.stringValue == "normal_y"){
                        elements.push_back(E_NORMAL_Y);
                    } else if(scanner.stringValue == "normal_z"){
                        elements.push_back(E_NORMAL_Z);
                    }
                }
            } else if(scanner.stringValue == "POINTS"){
                numPoints = scanner.readIntEx("The 'POINTS' field is not correctly specified.");
            } else if(scanner.stringValue == "DATA"){
                scanner.readWordEx("The 'DATA' field is not correctly specified.");
                if(scanner.stringValue == "ascii"){
                    scanner.readLFex();
                    if(elements.empty()){
                        scanner.throwException("The specification of field elements is not found.");
                    }
                    readPoints(out_pointSet, scanner, elements, numPoints);
                    break;
                } else {
                    scanner.throwException("The 'ascii' format is only supported for the point DATA.");
                }
            } else {
                scanner.skipToLineEnd();
            }
            scanner.readLFEOFex("The field value is not correctly specified.");
        }
    } catch(EasyScanner::Exception& ex){
        throw file_read_error() << error_info_message(ex.getFullMessage());
    }
}


void cnoid::savePCD(SgPointSet* pointSet, const std::string& filename, const Affine3& viewpoint)
{
    if(!pointSet->hasVertices()){
        throw empty_data_error() << error_info_message("Empty pointset");
    }

    ofstream ofs;
    ofs.open(filename.c_str());

    ofs <<
        "# .PCD v.7 - Point Cloud Data file format\n"
        "VERSION .7\n"
        "FIELDS x y z\n"
        "SIZE 4 4 4\n"
        "TYPE F F F\n"
        "COUNT 1 1 1\n";

    const SgVertexArray& points = *pointSet->vertices();
    const int numPoints = points.size();
    ofs << "WIDTH " << numPoints << "\n";
    ofs << "HEIGHT 1\n";

    ofs << "VIEWPOINT ";
    Affine3::ConstTranslationPart t = viewpoint.translation();
    ofs << t.x() << " " << t.y() << " " << t.z() << " ";
    const Quaternion q(viewpoint.rotation());
    ofs << q.w() << " " << q.x() << " " << q.y() << " " << q.z() << "\n";

    ofs << "POINTS " << numPoints << "\n";
    
    ofs << "DATA ascii\n";

    for(int i=0; i < numPoints; ++i){
        const Vector3f& p = points[i];
        ofs << p.x() << " " << p.y() << " " << p.z() << "\n";
    }

    ofs.close();
}
