#include <iostream>
#include <cmath>
#include <string>
#include <fstream>
#include <vector>

struct 3DPoint {
    float x; 
    float y;
    float z; 
};

struct GDTResult { 
    std::string type;
    float fromErr;
    bool isPassed;
};

std::vector<3DPoint> readGivenFile(std::string filename) {
    
    std::ifstream inFile(filename);

    if(!inFile.is_open()) {
        std::cerr << "Failed to Open Point File" << std::endl;
        return 1;
    }

     
    3DPoint temporaryPoint;
    vector<3DPoint> points;

    while (inFile >> temporaryPoint.x >> temporaryPoint.y >> temporaryPoint.z) {
        points.push_back(temporaryPoint);
    }

    inFile.close();

    std::cout << "File Succesfully Read" << " " << points.size(); << " " << "Points Detected" << std::endl;
    
    return points;

} 

int main() {



}