#include <iostream>
#include <cmath>
#include <string>
#include <fstream>
#include <vector>
#include <Eigen/Dense>

//DATA STRACTURES: Point3D, GDTResult, Plane, Circle

struct Point3D { 
    float x; 
    float y;
    float z; 
};

struct GDTResult { 
    std::string type;
    float formErr;
    bool isPassed;
};

struct Plane { 
    float a, b, c, d; 
}

struct Circle { 
    float xc, yc, radius;
}

struct Cylinder {
    Point3D axisPoint;
    Point3D axisDirection;
    float radius;
}

//FUNCTION: read points from given txt file

inline std::vector<Point3D> readGivenFile(std::string filename) { 
    
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

//FUNCTION: calculate centeroid by point list

inline Point3D CalculateCenteroid(const std::vector<Point3D>& pointList) { 

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

//FUNCTION: Normalize Vector

inline Point3D normalize(const Point3D& givenVec) { 

    float vectorLength = std::sqrt(std::pow(givenVec.x, 2) + std::pow(givenVec.y, 2) + std::pow(givenVec.z, 2));

    if (vectorLength < 1e-8f) {
        return {0.0f , 0.0f, 0.0f};
    }

    float normalizedX = givenVec.x / vectorLength;
    float normalizedY = givenVec.y / vectorLength;
    float normalizedZ = givenVec.z / vectorLength;

    return Point3D(normalizedX, normalizedY, normalizedZ);

}

//FUNCTION: Vector Cross Multiplication (Mul 2 Vectors)

inline Point3D vectorCrossMul(const 3DPoint& Vec1, const 3DPoint& Vec2) {

    return { Vec1.y * Vec2.z - Vec1.z * Vec2.y, 
             Vec1.z * Vec2.x - Vec1.x - Vec2.z,
             Vec1.x * Vec2.y - Vec1.y - Vec2.x };

}


//FUNCTION: fit optimal plane 

inline Plane fitPlane(const std::vector<Point3D>& pointList) { 
    int ListSize = pointList.size();

    if (size < 3) {
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

//FUNCTION: calculate flatness of the plane

float evaluateFlatness(const std::vector<Point3D>& pointList, const Plane& idealPlane) {  

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

//FUNCTION: fit optimal circle for points list

inline Circle fitCicle(const std::vector<Point3D>& pointList) { 

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

//FUNCTION: calculate roundness

inline float EvaluateRoundness(const Circle& C, const std::vector<Point3D>& pointList) {
    if (pointList.empty()) {
        return 0.0f;
    }

    float maxRadius = -1e9f;
    float minRadius = 1e9f;

    for (const auto& p : pointList) {

        float PointToCirclePitaguras = std::sqrt(std::pow(p.x - C.xc, 2) + std::pow(p.y - C.yc, 2));

        if (PointToCirclePitaguras > maxRadius) { maxRadius = PointToCirclePitaguras}
        if (PointToCirclePitaguras < minRadius) { minRadius = PointToCirclePitaguras}

    }

    return maxRadius - minRadius;

}



int main() {



}