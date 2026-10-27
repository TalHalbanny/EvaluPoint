#pragma once

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace gdt {

constexpr float kPi = 3.14159265358979323846f;

// DATA STRACTURES: 

struct Point3D {
    float x;
    float y;
    float z;

    Point3D() : x(0.f), y(0.f), z(0.f) {}
    Point3D(float xIn, float yIn, float zIn) : x(xIn), y(yIn), z(zIn) {}
};

struct GDTResult {
    std::string type;
    float formErr;
    bool isPassed;
};

struct Plane {
    float a;
    float b;
    float c;
    float d;
};

struct Circle {
    float xc;
    float yc;
    float radius;
};

struct Cylinder {
    Point3D axisPoint;
    Point3D axisDirection;
    float radius;
};

struct SquareShape {
    Point3D center;
    Point3D normal;
    Point3D axisU;
    Point3D axisV;
    float halfSide;
    Plane plane;
};

struct Sphere {
    Point3D center;
    float radius;
};

struct Cone {
    Point3D axisPoint;
    Point3D axisDirection;
    float radiusAtAxisPoint;
    float slope;
    float semiAngleDeg;
};

enum class ShapeKind {
    Cylinder,
    Square,
    Sphere,
    Cone
};

struct ToleranceZone {
    float low;
    float high;
    float center;
};

struct PointVerdict {
    Point3D position;
    float deviation;
    bool inside;
};

struct Evaluation {
    ShapeKind kind = ShapeKind::Cylinder;
    bool fitOk = false;
    GDTResult result{"", 0.f, false};
    float tolerance = 0.f;
    std::string definition;
    std::vector<std::string> detailLines;
    std::vector<std::string> reportLines;
    std::vector<PointVerdict> verdicts;
    ToleranceZone zone{0.f, 0.f, 0.f};
    Cylinder cylinder{};
    SquareShape square{};
    Sphere sphere{};
    Cone cone{};
    float heightMin = 0.f;
    float heightMax = 0.f;
    Point3D cloudCenter;
    Point3D boundsMin;
    Point3D boundsMax;
    float modelRadius = 1.f;
};

struct SymmetricEigen3 {
    double value[3];
    Point3D axis[3];
};

// FUNCTION: operator+ adds two vectors, one component at a time

inline Point3D operator+(const Point3D& a, const Point3D& b) {
    return Point3D(a.x + b.x, a.y + b.y, a.z + b.z);
}

// FUNCTION: operator- subtracts the second vector from the first

inline Point3D operator-(const Point3D& a, const Point3D& b) {
    return Point3D(a.x - b.x, a.y - b.y, a.z - b.z);
}

// FUNCTION: operator* scales a vector by a number

inline Point3D operator*(const Point3D& a, float scale) {
    return Point3D(a.x * scale, a.y * scale, a.z * scale);
}

// FUNCTION: operator* scales a vector when the number is written on the left

inline Point3D operator*(float scale, const Point3D& a) {
    return a * scale;
}

// FUNCTION: dot multiplies two vectors and adds the products, which tells how much they point the same way

inline float dot(const Point3D& a, const Point3D& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

// FUNCTION: crossProduct returns a vector that stands perpendicular to both inputs

inline Point3D crossProduct(const Point3D& a, const Point3D& b) {
    return Point3D(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x);
}

// FUNCTION: length returns how long a vector is

inline float length(const Point3D& a) {
    return std::sqrt(dot(a, a));
}

// FUNCTION: normalizeVector shrinks a vector to length 1. A near-zero vector comes back as zero so the caller can see the direction was useless

inline Point3D normalizeVector(const Point3D& a) {
    float vectorLength = length(a);
    if (vectorLength < 1e-8f) {
        return Point3D(0.f, 0.f, 0.f);
    }
    return a * (1.f / vectorLength);
}

// FUNCTION: orthonormalBasis builds two unit vectors at right angles to an axis, which is the frame a circle around a cylinder or cone is drawn in

inline void orthonormalBasis(const Point3D& axis, Point3D& tangent, Point3D& bitangent) {
    Point3D hint = (std::fabs(axis.z) < 0.9f) ? Point3D(0.f, 0.f, 1.f) : Point3D(1.f, 0.f, 0.f);
    tangent = normalizeVector(crossProduct(hint, axis));
    if (length(tangent) < 0.5f) {
        hint = Point3D(0.f, 1.f, 0.f);
        tangent = normalizeVector(crossProduct(hint, axis));
    }
    bitangent = normalizeVector(crossProduct(axis, tangent));
}

// FUNCTION: preferPositiveAxis flips an axis toward +Z, then +Y, then +X, so the same fit does not flip upside down from one run to the next

inline Point3D preferPositiveAxis(Point3D direction) {
    direction = normalizeVector(direction);
    if (direction.z < -1e-4f) {
        return direction * -1.f;
    }
    if (std::fabs(direction.z) <= 1e-4f && direction.y < -1e-4f) {
        return direction * -1.f;
    }
    if (std::fabs(direction.z) <= 1e-4f && std::fabs(direction.y) <= 1e-4f && direction.x < 0.f) {
        return direction * -1.f;
    }
    return direction;
}

// FUNCTION: formatNumber prints a value with four digits after the decimal point

inline std::string formatNumber(float value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(4) << value;
    return stream.str();
}

// FUNCTION: formatPoint prints a point as (x, y, z)

inline std::string formatPoint(const Point3D& point) {
    return "(" + formatNumber(point.x) + ", " + formatNumber(point.y) + ", " + formatNumber(point.z) + ")";
}

// FUNCTION: eigenSymmetric3 finds the three axes of a symmetric 3x3 matrix. The first axis is the direction of least spread, and that is the plane normal. Cylinder and cone fits try all three and keep the one with the smallest form error

inline SymmetricEigen3 eigenSymmetric3(const double input[3][3]) {
    double matrix[3][3];
    double vectors[3][3] = {
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
        {0.0, 0.0, 1.0}
    };
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            matrix[row][col] = input[row][col];
        }
    }

    for (int iteration = 0; iteration < 40; ++iteration) {
        int p = 0;
        int q = 1;
        double largest = std::fabs(matrix[0][1]);
        if (std::fabs(matrix[0][2]) > largest) {
            largest = std::fabs(matrix[0][2]);
            p = 0;
            q = 2;
        }
        if (std::fabs(matrix[1][2]) > largest) {
            largest = std::fabs(matrix[1][2]);
            p = 1;
            q = 2;
        }
        if (largest < 1e-15) {
            break;
        }

        double app = matrix[p][p];
        double aqq = matrix[q][q];
        double apq = matrix[p][q];
        double tau = (aqq - app) / (2.0 * apq);
        double tangent = (tau >= 0.0)
            ? 1.0 / (tau + std::sqrt(1.0 + tau * tau))
            : -1.0 / (-tau + std::sqrt(1.0 + tau * tau));
        double cosine = 1.0 / std::sqrt(1.0 + tangent * tangent);
        double sine = tangent * cosine;

        matrix[p][p] = app - tangent * apq;
        matrix[q][q] = aqq + tangent * apq;
        matrix[p][q] = 0.0;
        matrix[q][p] = 0.0;

        for (int row = 0; row < 3; ++row) {
            if (row == p || row == q) {
                continue;
            }
            double arp = matrix[row][p];
            double arq = matrix[row][q];
            matrix[row][p] = matrix[p][row] = cosine * arp - sine * arq;
            matrix[row][q] = matrix[q][row] = sine * arp + cosine * arq;
        }
        for (int row = 0; row < 3; ++row) {
            double vip = vectors[row][p];
            double viq = vectors[row][q];
            vectors[row][p] = cosine * vip - sine * viq;
            vectors[row][q] = sine * vip + cosine * viq;
        }
    }

    int order[3] = {0, 1, 2};
    for (int i = 0; i < 3; ++i) {
        for (int j = i + 1; j < 3; ++j) {
            if (matrix[order[j]][order[j]] < matrix[order[i]][order[i]]) {
                std::swap(order[i], order[j]);
            }
        }
    }

    SymmetricEigen3 result;
    for (int i = 0; i < 3; ++i) {
        result.value[i] = matrix[order[i]][order[i]];
        result.axis[i] = normalizeVector(Point3D(
            static_cast<float>(vectors[0][order[i]]),
            static_cast<float>(vectors[1][order[i]]),
            static_cast<float>(vectors[2][order[i]])));
    }
    return result;
}

// FUNCTION: calculateCentroid returns the average of the points. An empty list returns the origin, and the value is kept at full precision

inline Point3D calculateCentroid(const std::vector<Point3D>& pointList) {
    if (pointList.empty()) {
        return Point3D(0.f, 0.f, 0.f);
    }

    double sumX = 0.0;
    double sumY = 0.0;
    double sumZ = 0.0;
    for (const Point3D& point : pointList) {
        sumX += point.x;
        sumY += point.y;
        sumZ += point.z;
    }
    double count = static_cast<double>(pointList.size());
    return Point3D(
        static_cast<float>(sumX / count),
        static_cast<float>(sumY / count),
        static_cast<float>(sumZ / count));
}

// FUNCTION: principalAxes orders the cloud from the direction of least spread to the direction of most spread

inline bool principalAxes(const std::vector<Point3D>& pointList, const Point3D& centroid, Point3D axes[3]) {
    double covariance[3][3] = {};
    for (const Point3D& point : pointList) {
        double delta[3] = {
            point.x - centroid.x,
            point.y - centroid.y,
            point.z - centroid.z
        };
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 3; ++col) {
                covariance[row][col] += delta[row] * delta[col];
            }
        }
    }

    SymmetricEigen3 eigen = eigenSymmetric3(covariance);
    for (int i = 0; i < 3; ++i) {
        axes[i] = eigen.axis[i];
        if (!std::isfinite(axes[i].x) || !std::isfinite(axes[i].y) || !std::isfinite(axes[i].z)) {
            return false;
        }
    }
    return length(axes[0]) > 0.5f;
}

// FUNCTION: radialToAxis measures how far a point sits from an axis, which is its radius on a cylinder or a cone

inline float radialToAxis(const Point3D& origin, const Point3D& unitDirection, const Point3D& point) {
    Point3D relative = point - origin;
    float height = dot(relative, unitDirection);
    return length(relative - unitDirection * height);
}

// FUNCTION: heightOnAxis measures how far a point has moved along an axis, starting from the given origin

inline float heightOnAxis(const Point3D& origin, const Point3D& unitDirection, const Point3D& point) {
    return dot(point - origin, unitDirection);
}

// FUNCTION: deviationSpan returns the gap between the smallest and largest deviation. That gap is the form error for every shape here

inline float deviationSpan(const std::vector<float>& values, float& minimum, float& maximum) {
    minimum = values.front();
    maximum = values.front();
    for (float value : values) {
        minimum = std::min(minimum, value);
        maximum = std::max(maximum, value);
    }
    return maximum - minimum;
}

// FUNCTION: refineAxisDirection nudges an axis in shrinking steps until the form error stops getting smaller

template <typename ScoreFn>
Point3D refineAxisDirection(Point3D direction, ScoreFn score) {
    direction = normalizeVector(direction);
    Point3D tangent;
    Point3D bitangent;
    orthonormalBasis(direction, tangent, bitangent);
    float bestScore = score(direction);
    float step = 0.30f;

    for (int iteration = 0; iteration < 5; ++iteration) {
        Point3D bestDirection = direction;
        for (int i = -1; i <= 1; ++i) {
            for (int j = -1; j <= 1; ++j) {
                if (i == 0 && j == 0) {
                    continue;
                }
                Point3D candidate = normalizeVector(
                    direction + tangent * (step * static_cast<float>(i)) + bitangent * (step * static_cast<float>(j)));
                float candidateScore = score(candidate);
                if (candidateScore < bestScore) {
                    bestScore = candidateScore;
                    bestDirection = candidate;
                }
            }
        }
        direction = bestDirection;
        orthonormalBasis(direction, tangent, bitangent);
        step *= 0.40f;
    }
    return direction;
}

// FUNCTION: chooseAxis tries the three principal directions, keeps the one with the smallest form error, and then refines it

template <typename ScoreFn>
Point3D chooseAxis(const std::vector<Point3D>& pointList, const Point3D& centroid, ScoreFn score) {
    Point3D axes[3];
    if (!principalAxes(pointList, centroid, axes)) {
        return Point3D(0.f, 0.f, 1.f);
    }

    Point3D best = axes[0];
    float bestScore = score(best);
    for (int i = 1; i < 3; ++i) {
        float candidateScore = score(axes[i]);
        if (candidateScore < bestScore) {
            bestScore = candidateScore;
            best = axes[i];
        }
    }
    return preferPositiveAxis(refineAxisDirection(best, score));
}

// FUNCTION: readGivenFile loads points from a text file. Each line is x y z, commas count as spaces, and blank lines or lines that start with # are skipped. If the file will not open, the list comes back empty

inline std::vector<Point3D> readGivenFile(const std::string& filename) {
    std::ifstream inFile(filename);
    if (!inFile.is_open()) {
        std::cerr << "Failed to open point file: " << filename << std::endl;
        return {};
    }

    std::vector<Point3D> points;
    std::string line;
    bool firstLine = true;
    while (std::getline(inFile, line)) {
        if (firstLine) {
            firstLine = false;
            if (line.size() >= 3 &&
                static_cast<unsigned char>(line[0]) == 0xEF &&
                static_cast<unsigned char>(line[1]) == 0xBB &&
                static_cast<unsigned char>(line[2]) == 0xBF) {
                line.erase(0, 3);
            }
        }

        std::size_t start = line.find_first_not_of(" \t\r");
        if (start == std::string::npos || line[start] == '#') {
            continue;
        }
        for (char& character : line) {
            if (character == ',') {
                character = ' ';
            }
        }

        std::istringstream stream(line.substr(start));
        float x = 0.f;
        float y = 0.f;
        float z = 0.f;
        if (stream >> x >> y >> z && std::isfinite(x) && std::isfinite(y) && std::isfinite(z)) {
            points.push_back(Point3D(x, y, z));
        }
    }

    std::cout << "File successfully read. " << points.size() << " points detected." << std::endl;
    return points;
}

// FUNCTION: fitPlane finds the plane that best fits the points. The normal is the direction of least spread, and fewer than three points returns the XY plane

inline Plane fitPlane(const std::vector<Point3D>& pointList) {
    if (pointList.size() < 3) {
        return Plane{0.f, 0.f, 1.f, 0.f};
    }

    Point3D centroid = calculateCentroid(pointList);
    Point3D axes[3];
    if (!principalAxes(pointList, centroid, axes)) {
        return Plane{0.f, 0.f, 1.f, -centroid.z};
    }

    Point3D normal = preferPositiveAxis(axes[0]);
    float d = -(normal.x * centroid.x + normal.y * centroid.y + normal.z * centroid.z);
    return Plane{normal.x, normal.y, normal.z, d};
}

// FUNCTION: signedDistanceToPlane returns how far a point is from a plane. Positive means the point is on the side the normal points to

inline float signedDistanceToPlane(const Plane& plane, const Point3D& point) {
    float normalLength = std::sqrt(plane.a * plane.a + plane.b * plane.b + plane.c * plane.c);
    if (normalLength < 1e-8f) {
        return 0.f;
    }
    return (plane.a * point.x + plane.b * point.y + plane.c * point.z + plane.d) / normalLength;
}

// FUNCTION: evaluateFlatness subtracts the lowest point-to-plane distance from the highest. That width is the flatness

inline float evaluateFlatness(const std::vector<Point3D>& pointList, const Plane& plane) {
    if (pointList.empty()) {
        return 0.f;
    }

    float minimum = 1e30f;
    float maximum = -1e30f;
    for (const Point3D& point : pointList) {
        float distance = signedDistanceToPlane(plane, point);
        minimum = std::min(minimum, distance);
        maximum = std::max(maximum, distance);
    }
    return maximum - minimum;
}

// FUNCTION: fitCircle fits a circle in the XY plane, centered on the average of the points, using the average radius. This is the flat roundness helper. The 3D view uses cylindricity instead.
inline Circle fitCircle(const std::vector<Point3D>& pointList) {
    if (pointList.size() < 3) {
        return Circle{0.f, 0.f, 0.f};
    }

    Point3D centroid = calculateCentroid(pointList);
    float radiusSum = 0.f;
    for (const Point3D& point : pointList) {
        float dx = point.x - centroid.x;
        float dy = point.y - centroid.y;
        radiusSum += std::sqrt(dx * dx + dy * dy);
    }
    float averageRadius = radiusSum / static_cast<float>(pointList.size());
    return Circle{centroid.x, centroid.y, averageRadius};
}

// FUNCTION: evaluateRoundness subtracts the smallest radius from the largest, which is the roundness in the XY plane.
inline float evaluateRoundness(const Circle& circle, const std::vector<Point3D>& pointList) {
    if (pointList.empty()) {
        return 0.f;
    }

    float minimum = 1e30f;
    float maximum = -1e30f;
    for (const Point3D& point : pointList) {
        float dx = point.x - circle.xc;
        float dy = point.y - circle.yc;
        float radius = std::sqrt(dx * dx + dy * dy);
        minimum = std::min(minimum, radius);
        maximum = std::max(maximum, radius);
    }
    return maximum - minimum;
}

// FUNCTION: fitCylinder fits an axis through the centroid and records how far each point sits from that axis. Cylindricity is the gap between the smallest and largest of those radii. This assumes the points go around the axis, so the centroid lies on it.
inline bool fitCylinder(const std::vector<Point3D>& pointList, Evaluation& eval, std::vector<float>& radialDeviation) {
    Point3D centroid = calculateCentroid(pointList);
    auto score = [&](Point3D direction) -> float {
        direction = normalizeVector(direction);
        if (length(direction) < 0.5f) {
            return 1e30f;
        }
        float minimum = 1e30f;
        float maximum = -1e30f;
        for (const Point3D& point : pointList) {
            float radius = radialToAxis(centroid, direction, point);
            minimum = std::min(minimum, radius);
            maximum = std::max(maximum, radius);
        }
        return maximum - minimum;
    };

    Point3D axis = chooseAxis(pointList, centroid, score);
    if (length(axis) < 0.5f) {
        return false;
    }

    radialDeviation.assign(pointList.size(), 0.f);
    float radiusSum = 0.f;
    float heightMin = 1e30f;
    float heightMax = -1e30f;
    for (std::size_t i = 0; i < pointList.size(); ++i) {
        float radius = radialToAxis(centroid, axis, pointList[i]);
        float height = heightOnAxis(centroid, axis, pointList[i]);
        radialDeviation[i] = radius;
        radiusSum += radius;
        heightMin = std::min(heightMin, height);
        heightMax = std::max(heightMax, height);
    }

    eval.cylinder.axisPoint = centroid;
    eval.cylinder.axisDirection = axis;
    eval.cylinder.radius = radiusSum / static_cast<float>(pointList.size());
    eval.heightMin = heightMin;
    eval.heightMax = heightMax;
    eval.detailLines.push_back("Axis point: " + formatPoint(centroid));
    eval.detailLines.push_back("Axis direction: " + formatPoint(axis));
    return true;
}

// FUNCTION: fitSquare fits a plane and then the smallest square on that plane that still contains every point. Pass or fail comes from the flatness. The square outline is what gets drawn.

inline bool fitSquare(const std::vector<Point3D>& pointList, Evaluation& eval, std::vector<float>& signedDeviation) {
    Plane plane = fitPlane(pointList);
    Point3D normal(plane.a, plane.b, plane.c);
    if (length(normal) < 0.5f) {
        return false;
    }
    normal = normalizeVector(normal);

    Point3D tangent;
    Point3D bitangent;
    orthonormalBasis(normal, tangent, bitangent);
    Point3D centroid = calculateCentroid(pointList);

    struct PlanarPoint {
        float u;
        float v;
    };
    std::vector<PlanarPoint> projected(pointList.size());
    signedDeviation.assign(pointList.size(), 0.f);
    for (std::size_t i = 0; i < pointList.size(); ++i) {
        Point3D relative = pointList[i] - centroid;
        projected[i].u = dot(relative, tangent);
        projected[i].v = dot(relative, bitangent);
        signedDeviation[i] = dot(relative, normal);
    }

    float bestSide = 1e30f;
    float bestCos = 1.f;
    float bestSin = 0.f;
    float bestMinX = 0.f;
    float bestMaxX = 0.f;
    float bestMinY = 0.f;
    float bestMaxY = 0.f;
    for (int step = 0; step <= 180; ++step) {
        float theta = (kPi * 0.5f) * static_cast<float>(step) / 180.f;
        float cosine = std::cos(theta);
        float sine = std::sin(theta);
        float minX = 1e30f;
        float maxX = -1e30f;
        float minY = 1e30f;
        float maxY = -1e30f;
        for (const PlanarPoint& planar : projected) {
            float x = planar.u * cosine + planar.v * sine;
            float y = -planar.u * sine + planar.v * cosine;
            minX = std::min(minX, x);
            maxX = std::max(maxX, x);
            minY = std::min(minY, y);
            maxY = std::max(maxY, y);
        }
        float side = std::max(maxX - minX, maxY - minY);
        if (side < bestSide) {
            bestSide = side;
            bestCos = cosine;
            bestSin = sine;
            bestMinX = minX;
            bestMaxX = maxX;
            bestMinY = minY;
            bestMaxY = maxY;
        }
    }

    float centerX = 0.5f * (bestMinX + bestMaxX);
    float centerY = 0.5f * (bestMinY + bestMaxY);
    float centerU = bestCos * centerX - bestSin * centerY;
    float centerV = bestSin * centerX + bestCos * centerY;

    eval.square.center = centroid + tangent * centerU + bitangent * centerV;
    eval.square.normal = normal;
    eval.square.axisU = normalizeVector(tangent * bestCos + bitangent * bestSin);
    eval.square.axisV = normalizeVector(tangent * (-bestSin) + bitangent * bestCos);
    eval.square.halfSide = 0.5f * bestSide;
    eval.square.plane = plane;
    eval.detailLines.push_back("Plane normal: " + formatPoint(normal));
    eval.detailLines.push_back("Square side: " + formatNumber(bestSide));
    return true;
}

// FUNCTION: solveLinear4 solves a 4 by 4 linear system. The sphere fit uses it, and it returns false when the points do not define a sphere.

inline bool solveLinear4(double augmented[4][5], double solution[4]) {
    for (int column = 0; column < 4; ++column) {
        int pivot = column;
        for (int row = column + 1; row < 4; ++row) {
            if (std::fabs(augmented[row][column]) > std::fabs(augmented[pivot][column])) {
                pivot = row;
            }
        }
        if (std::fabs(augmented[pivot][column]) < 1e-12) {
            return false;
        }
        if (pivot != column) {
            for (int col = column; col < 5; ++col) {
                std::swap(augmented[pivot][col], augmented[column][col]);
            }
        }
        double divisor = augmented[column][column];
        for (int col = column; col < 5; ++col) {
            augmented[column][col] /= divisor;
        }
        for (int row = 0; row < 4; ++row) {
            if (row == column) {
                continue;
            }
            double factor = augmented[row][column];
            if (factor == 0.0) {
                continue;
            }
            for (int col = column; col < 5; ++col) {
                augmented[row][col] -= factor * augmented[column][col];
            }
        }
    }
    for (int i = 0; i < 4; ++i) {
        solution[i] = augmented[i][4];
    }
    return true;
}

// FUNCTION: fitSphere finds a center and then the distance of every point from that center. Sphericity is the gap between the nearest and farthest point. If the algebra fails, the center falls back to the centroid.
inline bool fitSphere(const std::vector<Point3D>& pointList, Evaluation& eval, std::vector<float>& radialDeviation) {
    Point3D origin = calculateCentroid(pointList);
    double normalMatrix[4][5] = {};
    for (const Point3D& point : pointList) {
        double x = point.x - origin.x;
        double y = point.y - origin.y;
        double z = point.z - origin.z;
        double row[4] = {x, y, z, 1.0};
        double rightHand = -(x * x + y * y + z * z);
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                normalMatrix[i][j] += row[i] * row[j];
            }
            normalMatrix[i][4] += row[i] * rightHand;
        }
    }

    double solution[4] = {};
    bool solved = solveLinear4(normalMatrix, solution);
    Point3D center = origin;
    if (solved) {
        Point3D localCenter(
            static_cast<float>(-0.5 * solution[0]),
            static_cast<float>(-0.5 * solution[1]),
            static_cast<float>(-0.5 * solution[2]));
        double radiusSquared =
            static_cast<double>(localCenter.x) * localCenter.x +
            static_cast<double>(localCenter.y) * localCenter.y +
            static_cast<double>(localCenter.z) * localCenter.z -
            solution[3];
        Point3D worldCenter = origin + localCenter;
        bool centerFinite = std::isfinite(worldCenter.x) && std::isfinite(worldCenter.y) && std::isfinite(worldCenter.z);
        if (centerFinite && radiusSquared > 1e-8 && radiusSquared < 1e12) {
            center = worldCenter;
        }
    }

    radialDeviation.assign(pointList.size(), 0.f);
    float radiusSum = 0.f;
    for (std::size_t i = 0; i < pointList.size(); ++i) {
        float radius = length(pointList[i] - center);
        radialDeviation[i] = radius;
        radiusSum += radius;
    }

    eval.sphere.center = center;
    eval.sphere.radius = radiusSum / static_cast<float>(pointList.size());
    eval.detailLines.push_back("Sphere center: " + formatPoint(center));
    return std::isfinite(eval.sphere.radius);
}

// FUNCTION: measureCone fits radius = radius0 + slope times height along one axis, then stores how far each point sits off the cone, measured perpendicular to the surface rather than straight out from the axis.
inline void measureCone(
    const std::vector<Point3D>& pointList,
    const Point3D& origin,
    const Point3D& direction,
    float& slope,
    float& radius0,
    float& heightMin,
    float& heightMax,
    std::vector<float>& normalDeviation) {

    const int count = static_cast<int>(pointList.size());
    normalDeviation.assign(pointList.size(), 0.f);
    if (count == 0) {
        slope = 0.f;
        radius0 = 0.f;
        heightMin = 0.f;
        heightMax = 0.f;
        return;
    }

    std::vector<float> height(pointList.size());
    std::vector<float> radius(pointList.size());
    double sumHeight = 0.0;
    double sumRadius = 0.0;
    double sumHeight2 = 0.0;
    double sumHeightRadius = 0.0;
    heightMin = 1e30f;
    heightMax = -1e30f;

    for (int i = 0; i < count; ++i) {
        Point3D relative = pointList[static_cast<std::size_t>(i)] - origin;
        height[static_cast<std::size_t>(i)] = dot(relative, direction);
        radius[static_cast<std::size_t>(i)] = length(relative - direction * height[static_cast<std::size_t>(i)]);
        double h = height[static_cast<std::size_t>(i)];
        double r = radius[static_cast<std::size_t>(i)];
        sumHeight += h;
        sumRadius += r;
        sumHeight2 += h * h;
        sumHeightRadius += h * r;
        heightMin = std::min(heightMin, height[static_cast<std::size_t>(i)]);
        heightMax = std::max(heightMax, height[static_cast<std::size_t>(i)]);
    }

    double inverse = 1.0 / static_cast<double>(count);
    double meanHeight = sumHeight * inverse;
    double meanRadius = sumRadius * inverse;
    double varianceHeight = sumHeight2 * inverse - meanHeight * meanHeight;
    double covariance = sumHeightRadius * inverse - meanHeight * meanRadius;
    double fittedSlope = (varianceHeight < 1e-12) ? 0.0 : covariance / varianceHeight;
    double fittedRadius0 = meanRadius - fittedSlope * meanHeight;
    slope = static_cast<float>(fittedSlope);
    radius0 = static_cast<float>(fittedRadius0);

    float cosAlpha = 1.f / std::sqrt(1.f + slope * slope);
    for (int i = 0; i < count; ++i) {
        float predicted = radius0 + slope * height[static_cast<std::size_t>(i)];
        normalDeviation[static_cast<std::size_t>(i)] = (radius[static_cast<std::size_t>(i)] - predicted) * cosAlpha;
    }
}

// FUNCTION: fitCone tries the principal axes and keeps the cone with the smallest form error. The axis passes through the centroid, which is fair when the points go all the way around.
inline bool fitCone(const std::vector<Point3D>& pointList, Evaluation& eval, std::vector<float>& normalDeviation) {
    Point3D centroid = calculateCentroid(pointList);
    auto score = [&](Point3D direction) -> float {
        direction = normalizeVector(direction);
        if (length(direction) < 0.5f) {
            return 1e30f;
        }
        float slope = 0.f;
        float radius0 = 0.f;
        float heightMin = 0.f;
        float heightMax = 0.f;
        std::vector<float> deviation;
        measureCone(pointList, centroid, direction, slope, radius0, heightMin, heightMax, deviation);
        float minimum = 0.f;
        float maximum = 0.f;
        return deviationSpan(deviation, minimum, maximum);
    };

    Point3D axis = chooseAxis(pointList, centroid, score);
    if (length(axis) < 0.5f) {
        return false;
    }

    float slope = 0.f;
    float radius0 = 0.f;
    measureCone(pointList, centroid, axis, slope, radius0, eval.heightMin, eval.heightMax, normalDeviation);

    eval.cone.axisPoint = centroid;
    eval.cone.axisDirection = axis;
    eval.cone.radiusAtAxisPoint = radius0;
    eval.cone.slope = slope;
    eval.cone.semiAngleDeg = std::fabs(std::atan(slope) * (180.f / kPi));
    eval.detailLines.push_back("Axis point: " + formatPoint(centroid));
    eval.detailLines.push_back("Axis direction: " + formatPoint(axis));
    eval.detailLines.push_back("Semi-angle: " + formatNumber(eval.cone.semiAngleDeg) + " deg");

    if (std::fabs(slope) < 1e-4f) {
        eval.detailLines.push_back("Apex: undefined (the fitted slope is nearly zero)");
    } else {
        float apexHeight = -radius0 / slope;
        Point3D apex = centroid + axis * apexHeight;
        eval.detailLines.push_back("Apex: " + formatPoint(apex));
    }
    return true;
}

// FUNCTION: placeToleranceZone lays a band of the given width over the deviations. If the points already fit, the band is centered on them. If they do not, it slides to cover as many as it can, and the points left outside are the red ones.
inline ToleranceZone placeToleranceZone(const std::vector<float>& deviation, float tolerance) {
    ToleranceZone zone{0.f, 0.f, 0.f};
    if (deviation.empty()) {
        return zone;
    }

    float minimum = 0.f;
    float maximum = 0.f;
    float span = deviationSpan(deviation, minimum, maximum);
    if (span <= tolerance) {
        float mid = 0.5f * (minimum + maximum);
        zone.low = mid - 0.5f * tolerance;
        zone.high = mid + 0.5f * tolerance;
        zone.center = mid;
        return zone;
    }

    std::vector<int> order(deviation.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = static_cast<int>(i);
    }
    std::sort(order.begin(), order.end(), [&](int left, int right) {
        return deviation[static_cast<std::size_t>(left)] < deviation[static_cast<std::size_t>(right)];
    });

    int bestStart = 0;
    int bestCount = 1;
    int right = 0;
    const int count = static_cast<int>(order.size());
    for (int left = 0; left < count; ++left) {
        if (right < left) {
            right = left;
        }
        float windowEnd = deviation[static_cast<std::size_t>(order[static_cast<std::size_t>(left)])] + tolerance + 1e-5f;
        while (right < count && deviation[static_cast<std::size_t>(order[static_cast<std::size_t>(right)])] <= windowEnd) {
            ++right;
        }
        int covered = right - left;
        if (covered > bestCount) {
            bestCount = covered;
            bestStart = left;
        }
    }

    zone.low = deviation[static_cast<std::size_t>(order[static_cast<std::size_t>(bestStart)])];
    zone.high = zone.low + tolerance;
    zone.center = 0.5f * (zone.low + zone.high);
    return zone;
}

// FUNCTION: shapeName returns the name shown for the form the user picked.
inline const char* shapeName(ShapeKind kind) {
    switch (kind) {
        case ShapeKind::Cylinder: return "Cylinder";
        case ShapeKind::Square: return "Square plate";
        case ShapeKind::Sphere: return "Sphere";
        case ShapeKind::Cone: return "Cone";
    }
    return "Shape";
}

// FUNCTION: gdtCheckName returns the GD&T control for that form, such as cylindricity or flatness.
inline const char* gdtCheckName(ShapeKind kind) {
    switch (kind) {
        case ShapeKind::Cylinder: return "Cylindricity";
        case ShapeKind::Square: return "Flatness";
        case ShapeKind::Sphere: return "Sphericity";
        case ShapeKind::Cone: return "Cone form";
    }
    return "Form";
}

// FUNCTION: gdtDefinition returns one sentence that says what the form error means for this shape.
inline const char* gdtDefinition(ShapeKind kind) {
    switch (kind) {
        case ShapeKind::Cylinder:
            return "Cylindricity is the gap between the smallest and largest radius about the fitted axis.";
        case ShapeKind::Square:
            return "Flatness is the gap between two parallel planes that enclose the square plate.";
        case ShapeKind::Sphere:
            return "Sphericity is the gap between the smallest and largest concentric spheres.";
        case ShapeKind::Cone:
            return "Cone form is the gap between two coaxial cones, measured perpendicular to the surface.";
    }
    return "";
}

// FUNCTION: minimumPointCount is the fewest points that can define the form. Below that, the fit is refused.
inline int minimumPointCount(ShapeKind kind) {
    switch (kind) {
        case ShapeKind::Cylinder: return 6;
        case ShapeKind::Square: return 3;
        case ShapeKind::Sphere: return 4;
        case ShapeKind::Cone: return 6;
    }
    return 3;
}

// FUNCTION: computeBounds finds the center and the size of the cloud so the camera can frame it.
inline void computeBounds(Evaluation& eval, const std::vector<Point3D>& pointList) {
    eval.boundsMin = pointList.front();
    eval.boundsMax = pointList.front();
    for (const Point3D& point : pointList) {
        eval.boundsMin.x = std::min(eval.boundsMin.x, point.x);
        eval.boundsMin.y = std::min(eval.boundsMin.y, point.y);
        eval.boundsMin.z = std::min(eval.boundsMin.z, point.z);
        eval.boundsMax.x = std::max(eval.boundsMax.x, point.x);
        eval.boundsMax.y = std::max(eval.boundsMax.y, point.y);
        eval.boundsMax.z = std::max(eval.boundsMax.z, point.z);
    }

    eval.cloudCenter = calculateCentroid(pointList);
    eval.modelRadius = 0.f;
    for (const Point3D& point : pointList) {
        eval.modelRadius = std::max(eval.modelRadius, length(point - eval.cloudCenter));
    }
    if (eval.modelRadius < 1e-3f) {
        eval.modelRadius = 1.f;
    }
}

// FUNCTION: applyVerdicts turns the deviations into a pass or fail and a verdict for every point. The surface drawn on screen sits in the middle of the accepted band.
inline void applyVerdicts(Evaluation& eval, const std::vector<Point3D>& pointList, const std::vector<float>& deviation) {
    eval.zone = placeToleranceZone(deviation, eval.tolerance);

    float minimum = 0.f;
    float maximum = 0.f;
    eval.result.formErr = deviationSpan(deviation, minimum, maximum);
    eval.result.isPassed = eval.result.formErr <= eval.tolerance + 1e-4f;
    eval.result.type = gdtCheckName(eval.kind);

    eval.verdicts.clear();
    int insideCount = 0;
    const float epsilon = std::max(1e-4f, 1e-5f * (std::fabs(eval.zone.high) + 1.f));
    for (std::size_t i = 0; i < pointList.size(); ++i) {
        PointVerdict verdict;
        verdict.position = pointList[i];
        verdict.deviation = deviation[i] - eval.zone.center;
        verdict.inside = deviation[i] >= eval.zone.low - epsilon && deviation[i] <= eval.zone.high + epsilon;
        if (verdict.inside) {
            ++insideCount;
        }
        eval.verdicts.push_back(verdict);
    }
    int outsideCount = static_cast<int>(pointList.size()) - insideCount;

    eval.reportLines.clear();
    eval.reportLines.push_back(std::string("Shape: ") + shapeName(eval.kind));
    eval.reportLines.push_back(std::string("GD&T check: ") + eval.result.type);
    eval.reportLines.push_back(eval.definition);
    eval.reportLines.push_back(
        "Points: " + std::to_string(pointList.size()) +
        "    Inside: " + std::to_string(insideCount) +
        "    Outside: " + std::to_string(outsideCount));
    eval.reportLines.push_back(
        std::string(eval.result.type) + ": " + formatNumber(eval.result.formErr) +
        "    Tolerance: " + formatNumber(eval.tolerance));
    eval.reportLines.push_back(std::string("Result: ") + (eval.result.isPassed ? "PASS" : "FAIL"));
    for (const std::string& line : eval.detailLines) {
        eval.reportLines.push_back(line);
    }

    switch (eval.kind) {
        case ShapeKind::Cylinder:
            eval.reportLines.push_back(
                "Drawn radius: " + formatNumber(eval.zone.center) +
                "    Inner zone: " + formatNumber(eval.zone.low) +
                "    Outer zone: " + formatNumber(eval.zone.high));
            break;
        case ShapeKind::Sphere:
            eval.reportLines.push_back(
                "Drawn radius: " + formatNumber(eval.zone.center) +
                "    Inner zone: " + formatNumber(eval.zone.low) +
                "    Outer zone: " + formatNumber(eval.zone.high));
            break;
        case ShapeKind::Square:
            eval.reportLines.push_back(
                "Inner plane shift: " + formatNumber(eval.zone.low) +
                "    Drawn plane shift: " + formatNumber(eval.zone.center) +
                "    Outer plane shift: " + formatNumber(eval.zone.high));
            break;
        case ShapeKind::Cone:
            eval.reportLines.push_back(
                "Inner normal offset: " + formatNumber(eval.zone.low) +
                "    Drawn offset: " + formatNumber(eval.zone.center) +
                "    Outer normal offset: " + formatNumber(eval.zone.high));
            break;
    }
}

// FUNCTION: evaluateShape fits the form the user picked, measures the form error, and marks each point inside or outside the tolerance. This is the call the window uses.
inline Evaluation evaluateShape(ShapeKind kind, const std::vector<Point3D>& pointList, float tolerance) {
    Evaluation eval;
    eval.kind = kind;
    if (!(tolerance > 0.f) || !std::isfinite(tolerance)) {
        tolerance = 0.0001f;
    }
    eval.tolerance = tolerance;
    eval.result.type = gdtCheckName(kind);
    eval.definition = gdtDefinition(kind);

    if (pointList.empty()) {
        eval.result.isPassed = false;
        eval.result.type = "No points";
        eval.reportLines.push_back("No measured points were loaded.");
        return eval;
    }

    computeBounds(eval, pointList);
    if (static_cast<int>(pointList.size()) < minimumPointCount(kind)) {
        eval.fitOk = false;
        eval.result.isPassed = false;
        eval.reportLines.push_back(std::string("Shape: ") + shapeName(kind));
        eval.reportLines.push_back("Not enough points to fit this shape.");
        eval.reportLines.push_back(
            "Need at least " + std::to_string(minimumPointCount(kind)) + " points.");
        for (const Point3D& point : pointList) {
            eval.verdicts.push_back(PointVerdict{point, 0.f, false});
        }
        return eval;
    }

    std::vector<float> deviation;
    bool fitted = false;
    switch (kind) {
        case ShapeKind::Cylinder: fitted = fitCylinder(pointList, eval, deviation); break;
        case ShapeKind::Square: fitted = fitSquare(pointList, eval, deviation); break;
        case ShapeKind::Sphere: fitted = fitSphere(pointList, eval, deviation); break;
        case ShapeKind::Cone: fitted = fitCone(pointList, eval, deviation); break;
    }

    eval.fitOk = fitted && deviation.size() == pointList.size();
    if (!eval.fitOk) {
        eval.result.isPassed = false;
        eval.reportLines.push_back(std::string("Shape: ") + shapeName(kind));
        eval.reportLines.push_back("The shape could not be fitted to these points.");
        for (const Point3D& point : pointList) {
            eval.verdicts.push_back(PointVerdict{point, 0.f, false});
        }
        return eval;
    }

    applyVerdicts(eval, pointList, deviation);
    return eval;
}

}
