/**
   @author Shizuko Hattori
   @author Shin'ichiro Nakaoka
*/

#include <cnoid/SimpleController>
#include <cnoid/Joystick>

using namespace std;
using namespace cnoid;

namespace {
const int axisIds[] = {1,0};
}

class SampleCrawlerJoystickController : public cnoid::SimpleController
{ 
    Link* crawlerL;
    Link* crawlerR;
    double qRef[2];
    Joystick joystick;

public:
    
    virtual bool initialize() {

        crawlerL = ioBody()->link("CRAWLER_TRACK_L");
        crawlerR = ioBody()->link("CRAWLER_TRACK_R");

        if(!crawlerL || !crawlerR){
            os() << "Crawlers are not found" << endl;
            return false;
        }

        for(int i=0; i < 2; i++){
            qRef[i] = 0;
        }

        if(!joystick.isReady()){
            os() << "Joystick is not ready: " << joystick.errorMessage() << endl;
        }
        if(joystick.numAxes() < 5){
            os() << "The number of the joystick axes is not sufficient for controlling the robot." << endl;
        }
        
        return true;
    }

    virtual bool control() {

        joystick.readCurrentState();

        double pos[2];
        for(int i=0; i < 2; ++i){
            pos[i] = joystick.getPosition(i);
            if(fabs(pos[i]) < 0.2){
                pos[i] = 0.0;
            }
        }
        // set the velocity of each crawlers
        crawlerL->u() = -2.0 * pos[1] + pos[0];
        crawlerR->u() = -2.0 * pos[1] - pos[0];

        return true;
    }
};

CNOID_IMPLEMENT_SIMPLE_CONTROLLER_FACTORY(SampleCrawlerJoystickController)
