#include <iostream>
#include <cmath>
#include <string>
#include <fstream>
#include <vector>
#include <Eigen/Dense>

struct Point3D { //struct for Point3D
    float x; 
    float y;
    float z; 
};

struct GDTResult { //struct for result
    std::string type;
    float fromErr;
    bool isPassed;
};

struct Plane { //Plane struct by Plane Equition
    float a, b, c, d; 
}

struct Circle { //Circle  struct
    float xc, yc, radius;
}


inline std::vector<Point3D> readGivenFile(std::string filename) { //read point from text file.
    
    std::ifstream inFile(filename);

    if(!inFile.is_open()) {
        std::cerr << "Failed to Open Point File" << std::endl;
        return 1;
    }

     
    3DPoint temporaryPoint;
    vector<Point3D> points;

    while (inFile >> temporaryPoint.x >> temporaryPoint.y >> temporaryPoint.z) {
        points.push_back(temporaryPoint);
    }

    inFile.close();

    std::cout << "File Succesfully Read" << " " << points.size(); << " " << "Points Detected" << std::endl;
    
    return points;

} 

inline Point3D CalculateCenteroid(const std::vector<Point3D>& pointList) { //calculate centeroid point values. 

    if (pointList.empty()) {
        return Point3D(0.0f, 0.0f, 0.0f);
    }

    int NumOfPoints = pointList.size();
    float sumOfX = 0.0f;
    float sumOfY = 0.0f;
    float sumOfZ = 0.0f;

    for (size_t i=0; i<NumOfPoints; ++i) {

        sumOfX+=pointList[i].x;
        sumOfY+=pointList[i].y;
        sumOfZ+=pointList[i].z;

    }

    float CenteroidX = std::round((sumOfX / NumOfPoints) * 1000.0f) / 1000.0f; //for each value to be 3 digits after point (as 0.000)
    float CenteroidY = std::round((sumOfY / NumOfPoints)* 1000.0f) / 1000.0f;
    float CenteroidZ = std::round((sumOfZ / NumOfPoints)* 1000.0f) / 1000.0f;

    Point3D CenteroidPoint = Point3D(CenteroidX, CenteroidY, CenteroidZ);

    return CenteroidPoint;

}

inline Point3D normalize(const Point3D& givenVec) { //normalize vactor 

    float vectorLength = std::sqrt(std::pow(givenVec.x, 2) + std::pow(givenVec.y, 2) + std::pow(givenVec.z, 2));

    if (vectorLength < 1e-8f) {
        return {0.0f , 0.0f, 0.0f};
    }

    float normalizedX = givenVec.x / vectorLength;
    float normalizedY = givenVec.y / vectorLength;
    float normalizedZ = givenVec.z / vectorLength;

    return Point3D(normalizedX, normalizedY, normalizedZ);

}

inline Plane fitPlane(const std::vector<Point3D>& pointList) { //fit optimal plane for point List

    int ListSize = pointList.size();

    if (size < 3) { //must have 3 or more vector points. 
        return {0.0f, 0.0f, 1.0f, 0.0f};
    }

    Point3D Centeroid = CalculateCenteroid(pointList);

    Eigen::Matrix3f CovarianceMatrix = Eigen::Matrix3f::Zero();
    
    for (const auto& p: pointList) { //create Covariance Matrix

        Eigen::Vector3f diff(p.x - Centeroid.x, p.y - Centeroid.y, p.z - Centeroid.z);
        CovarianceMatrix+=diff * diff.transpose();

    }

    Eigen::SelfAdjointEigenSolver<Eigen::Martix3f> eigenSolver(CovarianceMatrix); //find Eigenvalues and EigenVectors

    Eigen::Vector3f normal = eigenSolver.eigenvector().col(0); //take minimal value EigenVector as normal (located in col(0) allways with Eigen)

    normal.normalize(); //normalize the vector to achieve length of 1. 

    float calcD = -(normal[0] * Centeroid.x + normal[1] * Centeroid.y + normal[2] * Centeroid.z); 

    return {normal[0], normal[1], normal[2], calcD}; 

}

float evaluateFlatness(const std::vector<Point3D>& pointList, const Plane& idealPlane) { //calculate Flatness of the plane 

    if (pointList.empty()) {
        return 0.0f;
    }

    float normLen = std::sqrt(idealPlane.a * idealPlane.a + idealPlane.b * idealPlane.b + idealPlane.c * idealPlane.c);  
    if (normLen < 1e-8f) {
        return 0.0f;
    }

    float minDist = 1e9f;
    float maxDist = -1e9f;

    for (const auto& p: pointList) {

        float distanceCal = (idealPlane.a * p.x + idealPlane.b * p.y + idealPlane.c * p.z + idealPlane.d) / normLen;

        if (distanceCal < minDist) { minDist = distanceCal; }
        if (distanceCal > maxDist) {maxDist = distanceCal; }
    }

    return maxDist - minDist;

}

inline Circle fitCicle(const std::vector<Point3D>& pointList, const Circle& idealCircle) { //fit optimal circle for points list

    if (pointList.size() < 3) {
        return {0.0f, 0.0f, 0.0,f};
    }


    Point3D PointCenteroid = CalculateCenteroid(pointList);

    float xc = PointCenteroid.x;
    float yc = PointCenteroid.y;

    float radius = 0.0f;

    for (const auto& p : pointList) {

        float pitaguras = std::sqrt(std::pow(p.x - xc, 2) + std::pow(p.y - xy, 2));
        radius += pitaguras;

    }

    float avrageRadius = sumRadius / pointsList.size();

    return {xc, yc, avrageRadius};

}




int main() {



}