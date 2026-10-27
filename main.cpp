#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include "mainLogic.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

// DATA STRACTURES: Camera keeps the orbit angles, the zoom, and the pan

struct Camera {
    float yaw = 38.f;
    float pitch = 26.f;
    float zoom = 6.f;
    float panX = 0.f;
    float panY = 0.f;
};

HDC gDevice = nullptr;
HGLRC gContext = nullptr;
HWND gWindow = nullptr;
GLuint gFontBase = 0;
int gWidth = 1200;
int gHeight = 760;
bool gLeftDown = false;
bool gRightDown = false;
int gLastX = 0;
int gLastY = 0;
bool gClassRegistered = false;

Camera gCamera;
Camera gHomeCamera;
gdt::ShapeKind gShape = gdt::ShapeKind::Cylinder;
float gTolerance = 0.05f;
std::vector<gdt::Point3D> gPoints;
gdt::Evaluation gEval;
std::string gFilePath;
std::string gFileLabel = "No file loaded";
std::string gStatusMessage;
int gSelectedPoint = -1;
int gListScroll = 0;
int gVisibleRows = 1;
int gHotButton = 0;

enum UiCommand {
    UiOpen = 1,
    UiCylinder,
    UiSquare,
    UiSphere,
    UiCone,
    UiTolDown,
    UiTolUp
};

struct UiButton {
    float x;
    float y;
    float w;
    float h;
    int command;
};

std::vector<UiButton> gButtons;
float gListLeft = 0.f;
float gListRight = 0.f;
float gListTop = 0.f;
float gListBottom = 0.f;
const float gRowHeight = 20.f;

// FUNCTION: announceLiveUpdate prints one line after the tolerance or the form changes

void announceLiveUpdate() {
    int inside = 0;
    for (const gdt::PointVerdict& verdict : gEval.verdicts) {
        if (verdict.inside) {
            ++inside;
        }
    }
    int outside = static_cast<int>(gEval.verdicts.size()) - inside;
    std::cout << std::fixed;
    std::cout.precision(4);
    std::cout << gdt::shapeName(gShape) << "  tolerance " << gTolerance
              << "  " << gEval.result.type << " " << gEval.result.formErr
              << "  " << (gEval.result.isPassed ? "PASS" : "FAIL")
              << "  inside " << inside << "  outside " << outside << std::endl;
}

// FUNCTION: recompute fits the current points to the current form again and updates the window title

void recompute() {
    gEval = gdt::evaluateShape(gShape, gPoints, gTolerance);
    if (gWindow) {
        std::ostringstream title;
        title << std::fixed;
        title.precision(4);
        title << "EvaluPoint - " << gdt::shapeName(gShape)
              << " - " << (gEval.result.isPassed ? "PASS" : "FAIL")
              << " - " << gEval.result.type << " " << gEval.result.formErr
              << " - tolerance " << gTolerance;
        SetWindowTextA(gWindow, title.str().c_str());
    }
}

// FUNCTION: resetCamera puts the view back on the orbit it started with

void resetCamera() {
    gCamera = gHomeCamera;
}

// FUNCTION: frameCamera sets the zoom from the size of the point cloud so the shape fills the view

void frameCamera() {
    gCamera.yaw = 38.f;
    gCamera.pitch = 26.f;
    gCamera.panX = 0.f;
    gCamera.panY = 0.f;
    gCamera.zoom = std::max(1.f, gEval.modelRadius * 3.4f);
    gHomeCamera = gCamera;
}

// FUNCTION: bumpTolerance raises or lowers the tolerance by a small step and redraws the zone

void bumpTolerance(float delta) {
    gTolerance = std::max(0.001f, gTolerance + delta);
    recompute();
    announceLiveUpdate();
}

// FUNCTION: switchShape checks the same points against another form. The form buttons and the keys 1 to 4 call this

void switchShape(gdt::ShapeKind kind) {
    gShape = kind;
    recompute();
    if (gSelectedPoint >= static_cast<int>(gEval.verdicts.size())) {
        gSelectedPoint = -1;
    }
    announceLiveUpdate();
}

// FUNCTION: sidePanelWidth is how wide the control column on the right is

float sidePanelWidth() {
    if (gWidth < 980) {
        return std::max(300.f, static_cast<float>(gWidth) * 0.40f);
    }
    return 440.f;
}

// FUNCTION: sidePanelLeft is the screen x where that control column starts

float sidePanelLeft() {
    return static_cast<float>(gWidth) - sidePanelWidth();
}

// FUNCTION: fileNameOnly keeps just the file name, so a long path still fits in the panel

std::string fileNameOnly(const std::string& path) {
    const std::size_t slash = path.find_last_of("\\/");
    if (slash == std::string::npos) {
        return path;
    }
    return path.substr(slash + 1);
}

// FUNCTION: directoryOf returns the folder a path sits in, which is where the file dialog opens

std::string directoryOf(const std::string& path) {
    const std::size_t slash = path.find_last_of("\\/");
    if (slash == std::string::npos) {
        return "samples";
    }
    return path.substr(0, slash);
}

// FUNCTION: fitLabel shortens a label so it stays inside the side panel.
std::string fitLabel(const std::string& text, int maxChars) {
    if (static_cast<int>(text.size()) <= maxChars) {
        return text;
    }
    if (maxChars <= 3) {
        return text.substr(0, static_cast<std::size_t>(maxChars));
    }
    return "..." + text.substr(text.size() - static_cast<std::size_t>(maxChars - 3));
}

// FUNCTION: ensureSelectionVisible scrolls the point list until the highlighted row is on screen.
void ensureSelectionVisible() {
    if (gSelectedPoint < 0) {
        return;
    }
    if (gSelectedPoint < gListScroll) {
        gListScroll = gSelectedPoint;
    }
    if (gVisibleRows > 0 && gSelectedPoint >= gListScroll + gVisibleRows) {
        gListScroll = gSelectedPoint - gVisibleRows + 1;
    }
    if (gListScroll < 0) {
        gListScroll = 0;
    }
}

// FUNCTION: createHudFont builds a Consolas bitmap font so the panel can draw text.
void createHudFont(HDC device) {
    HFONT font = CreateFontA(
        -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FF_DONTCARE | DEFAULT_PITCH, "Consolas");
    HFONT previous = static_cast<HFONT>(SelectObject(device, font));
    gFontBase = glGenLists(96);
    wglUseFontBitmapsA(device, 32, 96, gFontBase);
    SelectObject(device, previous);
    DeleteObject(font);
}

// FUNCTION: releaseGraphics frees the font, the OpenGL context, and the device context. Calling it twice is safe.
void releaseGraphics() {
    if (gFontBase != 0) {
        glDeleteLists(gFontBase, 96);
        gFontBase = 0;
    }
    if (gContext) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(gContext);
        gContext = nullptr;
    }
    if (gDevice && gWindow) {
        ReleaseDC(gWindow, gDevice);
        gDevice = nullptr;
    }
}

// FUNCTION: drawHudText draws one string in window pixels. The projection already has to be flat, in screen space.
void drawHudText(float x, float y, const std::string& text) {
    if (text.empty() || gFontBase == 0) {
        return;
    }
    glRasterPos2f(x, y);
    glListBase(gFontBase - 32);
    glCallLists(static_cast<GLsizei>(text.size()), GL_UNSIGNED_BYTE, text.c_str());
}

// FUNCTION: drawFrustum draws a plain tube outline. A cylinder uses the same radius at both ends, and a cone uses a different radius at each end.
void drawFrustum(
    const gdt::Point3D& origin,
    const gdt::Point3D& direction,
    float h0,
    float h1,
    float radius0,
    float radius1,
    int slices,
    int rings,
    bool stippled) {

    gdt::Point3D dir = gdt::normalizeVector(direction);
    gdt::Point3D tangent;
    gdt::Point3D bitangent;
    gdt::orthonormalBasis(dir, tangent, bitangent);
    if (gdt::length(tangent) < 0.5f) {
        return;
    }

    if (stippled) {
        glEnable(GL_LINE_STIPPLE);
        glLineStipple(2, 0x0F0F);
    }

    auto radiusAt = [&](float height) {
        float span = h1 - h0;
        float blend = (std::fabs(span) < 1e-6f) ? 0.f : (height - h0) / span;
        return radius0 + (radius1 - radius0) * blend;
    };
    auto pointAt = [&](float height, float radius, float angle) {
        float clamped = std::max(0.f, radius);
        return origin + dir * height + (tangent * std::cos(angle) + bitangent * std::sin(angle)) * clamped;
    };

    for (int ring = 0; ring <= rings; ++ring) {
        float height = h0 + (h1 - h0) * static_cast<float>(ring) / static_cast<float>(rings);
        float radius = radiusAt(height);
        glBegin(GL_LINE_LOOP);
        for (int slice = 0; slice < slices; ++slice) {
            float angle = gdt::kPi * 2.f * static_cast<float>(slice) / static_cast<float>(slices);
            gdt::Point3D point = pointAt(height, radius, angle);
            glVertex3f(point.x, point.y, point.z);
        }
        glEnd();
    }

    glBegin(GL_LINES);
    const int spokes = 4;
    for (int spoke = 0; spoke < spokes; ++spoke) {
        float angle = gdt::kPi * 2.f * static_cast<float>(spoke) / static_cast<float>(spokes);
        gdt::Point3D start = pointAt(h0, radius0, angle);
        gdt::Point3D end = pointAt(h1, radius1, angle);
        glVertex3f(start.x, start.y, start.z);
        glVertex3f(end.x, end.y, end.z);
    }
    glEnd();

    if (stippled) {
        glDisable(GL_LINE_STIPPLE);
    }
}

// FUNCTION: paddedHeights gives a cylinder or cone a visible length even when every point shares one height.
void paddedHeights(float& h0, float& h1) {
    float span = h1 - h0;
    float pad = std::max(span * 0.04f, gEval.modelRadius * 0.02f);
    h0 -= pad;
    h1 += pad;
    if (h1 - h0 < gEval.modelRadius * 0.08f) {
        float mid = 0.5f * (h0 + h1);
        h0 = mid - gEval.modelRadius * 0.2f;
        h1 = mid + gEval.modelRadius * 0.2f;
    }
}

// FUNCTION: squareCorners returns the four corners of the square plate after it is shifted along its normal.
void squareCorners(float normalShift, gdt::Point3D corners[4]) {
    const gdt::SquareShape& square = gEval.square;
    gdt::Point3D origin = square.center + square.normal * normalShift;
    float half = square.halfSide;
    corners[0] = origin + square.axisU * half + square.axisV * half;
    corners[1] = origin - square.axisU * half + square.axisV * half;
    corners[2] = origin - square.axisU * half - square.axisV * half;
    corners[3] = origin + square.axisU * half - square.axisV * half;
}

// FUNCTION: drawSquareLoop draws one square outline. The nominal plate also gets a light fill so it reads as a face.
void drawSquareLoop(float normalShift, bool stippled, bool filled) {
    gdt::Point3D corners[4];
    squareCorners(normalShift, corners);
    if (filled) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glColor4f(0.20f, 0.55f, 0.70f, 0.28f);
        glBegin(GL_QUADS);
        for (int i = 0; i < 4; ++i) {
            glVertex3f(corners[i].x, corners[i].y, corners[i].z);
        }
        glEnd();
        glDepthMask(GL_TRUE);
    }
    if (stippled) {
        glEnable(GL_LINE_STIPPLE);
        glLineStipple(2, 0x0F0F);
    }
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 4; ++i) {
        glVertex3f(corners[i].x, corners[i].y, corners[i].z);
    }
    glEnd();
    if (stippled) {
        glDisable(GL_LINE_STIPPLE);
    }
}

// FUNCTION: drawSphereWire draws three circles so the sphere reads as a ball.
void drawSphereWire(const gdt::Point3D& center, float radius) {
    if (radius <= 1e-4f) {
        return;
    }
    const int slices = 36;
    for (int circle = 0; circle < 3; ++circle) {
        glBegin(GL_LINE_LOOP);
        for (int slice = 0; slice < slices; ++slice) {
            float angle = gdt::kPi * 2.f * static_cast<float>(slice) / static_cast<float>(slices);
            float c = std::cos(angle);
            float s = std::sin(angle);
            float x = 0.f;
            float y = 0.f;
            float z = 0.f;
            if (circle == 0) {
                x = c;
                y = s;
            } else if (circle == 1) {
                x = c;
                z = s;
            } else {
                y = c;
                z = s;
            }
            glVertex3f(center.x + radius * x, center.y + radius * y, center.z + radius * z);
        }
        glEnd();
    }
}

// FUNCTION: coneRadiusAt returns the cone radius at a height, after shifting the surface perpendicular to itself.
float coneRadiusAt(float height, float normalOffset) {
    float radialScale = std::sqrt(1.f + gEval.cone.slope * gEval.cone.slope);
    return gEval.cone.radiusAtAxisPoint + gEval.cone.slope * height + normalOffset * radialScale;
}

// FUNCTION: drawFeature draws one simple outline of the fitted shape in cyan.
void drawFeature() {
    if (!gEval.fitOk) {
        return;
    }

    float h0 = gEval.heightMin;
    float h1 = gEval.heightMax;
    paddedHeights(h0, h1);
    const int slices = 36;

    glDisable(GL_LINE_STIPPLE);
    glLineWidth(2.f);
    glColor3f(0.35f, 0.86f, 0.95f);
    switch (gEval.kind) {
        case gdt::ShapeKind::Cylinder:
            drawFrustum(
                gEval.cylinder.axisPoint, gEval.cylinder.axisDirection,
                h0, h1, gEval.zone.center, gEval.zone.center, slices, 1, false);
            break;
        case gdt::ShapeKind::Sphere:
            drawSphereWire(gEval.sphere.center, std::max(0.f, gEval.zone.center));
            break;
        case gdt::ShapeKind::Square:
            drawSquareLoop(gEval.zone.center, false, true);
            break;
        case gdt::ShapeKind::Cone:
            drawFrustum(
                gEval.cone.axisPoint, gEval.cone.axisDirection,
                h0, h1, coneRadiusAt(h0, gEval.zone.center), coneRadiusAt(h1, gEval.zone.center),
                slices, 1, false);
            break;
    }
}

// FUNCTION: drawCross draws one 3D plus sign. size is half the length of each arm.
void drawCross(const gdt::Point3D& point, float size) {
    glVertex3f(point.x - size, point.y, point.z);
    glVertex3f(point.x + size, point.y, point.z);
    glVertex3f(point.x, point.y - size, point.z);
    glVertex3f(point.x, point.y + size, point.z);
    glVertex3f(point.x, point.y, point.z - size);
    glVertex3f(point.x, point.y, point.z + size);
}

// FUNCTION: drawPoints draws every point from the file on the fitted shape. Green is inside the zone, red is outside, and the row the user clicked is yellow.
void drawPoints() {
    const float marker = std::max(gEval.modelRadius * 0.045f, 0.02f);
    glPointSize(5.f);
    glBegin(GL_POINTS);
    for (std::size_t index = 0; index < gEval.verdicts.size(); ++index) {
        if (static_cast<int>(index) == gSelectedPoint) {
            continue;
        }
        const gdt::PointVerdict& verdict = gEval.verdicts[index];
        if (!gEval.fitOk) {
            glColor3f(0.95f, 0.78f, 0.30f);
        } else if (verdict.inside) {
            glColor3f(0.30f, 0.95f, 0.42f);
        } else {
            continue;
        }
        glVertex3f(verdict.position.x, verdict.position.y, verdict.position.z);
    }
    glEnd();

    glPointSize(9.f);
    glColor3f(1.00f, 0.30f, 0.24f);
    glBegin(GL_POINTS);
    for (std::size_t index = 0; index < gEval.verdicts.size(); ++index) {
        if (static_cast<int>(index) == gSelectedPoint || !gEval.fitOk) {
            continue;
        }
        const gdt::PointVerdict& verdict = gEval.verdicts[index];
        if (!verdict.inside) {
            glVertex3f(verdict.position.x, verdict.position.y, verdict.position.z);
        }
    }
    glEnd();

    if (gSelectedPoint < 0 || gSelectedPoint >= static_cast<int>(gEval.verdicts.size())) {
        return;
    }

    const gdt::PointVerdict& selected = gEval.verdicts[static_cast<std::size_t>(gSelectedPoint)];
    const float size = marker * 2.8f;
    glDisable(GL_DEPTH_TEST);
    glLineWidth(3.f);
    glColor3f(1.00f, 0.86f, 0.20f);
    glBegin(GL_LINES);
    drawCross(selected.position, size);
    glEnd();

    char label[32];
    std::snprintf(label, sizeof(label), "#%d", gSelectedPoint);
    glRasterPos3f(selected.position.x + size * 0.4f, selected.position.y + size * 0.2f, selected.position.z);
    if (gFontBase != 0) {
        glListBase(gFontBase - 32);
        glCallLists(static_cast<GLsizei>(std::strlen(label)), GL_UNSIGNED_BYTE, label);
    }
    glEnable(GL_DEPTH_TEST);
}

// FUNCTION: loadPointFile replaces the loaded points and draws them again on the form that is already selected.
void loadPointFile(const std::string& path) {
    std::vector<gdt::Point3D> points = gdt::readGivenFile(path);
    if (points.empty()) {
        gStatusMessage = "That file has no x y z points.";
        return;
    }
    gPoints = std::move(points);
    gFilePath = path;
    gFileLabel = fileNameOnly(path);
    gStatusMessage.clear();
    gSelectedPoint = -1;
    gListScroll = 0;
    recompute();
    frameCamera();
    announceLiveUpdate();
}

// FUNCTION: browseForPointFile opens a file dialog so the user can pick a point file, then fits that file to the selected form.
void browseForPointFile() {
    char path[MAX_PATH] = {};
    if (!gFilePath.empty() && gFilePath.size() < MAX_PATH) {
        std::snprintf(path, MAX_PATH, "%s", gFilePath.c_str());
    }

    std::string initialDirectory = gFilePath.empty() ? std::string("samples") : directoryOf(gFilePath);
    OPENFILENAMEA dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = gWindow;
    dialog.lpstrFilter = "Point files (*.txt)\0*.txt\0All files (*.*)\0*.*\0";
    dialog.lpstrFile = path;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrTitle = "Open a point file";
    dialog.lpstrInitialDir = initialDirectory.c_str();
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameA(&dialog)) {
        if (gDevice && gContext) {
            wglMakeCurrent(gDevice, gContext);
        }
        return;
    }
    if (gDevice && gContext) {
        wglMakeCurrent(gDevice, gContext);
    }
    loadPointFile(path);
}

// FUNCTION: drawQuad fills a rectangle in screen pixels. y is the bottom edge.
void drawQuad(float x, float y, float w, float h) {
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

// FUNCTION: drawOutline strokes the border of a rectangle in screen pixels.
void drawOutline(float x, float y, float w, float h) {
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

// FUNCTION: drawButton draws one clickable control and remembers its rectangle so a later click can hit it.
void drawButton(float x, float y, float w, float h, const char* label, bool selected, bool accent, int command) {
    gButtons.push_back(UiButton{x, y, w, h, command});
    const bool hot = command == gHotButton;
    if (selected) {
        glColor3f(0.14f, 0.40f, 0.50f);
    } else if (accent) {
        glColor3f(hot ? 0.22f : 0.16f, hot ? 0.52f : 0.42f, hot ? 0.68f : 0.58f);
    } else if (hot) {
        glColor3f(0.24f, 0.26f, 0.32f);
    } else {
        glColor3f(0.16f, 0.17f, 0.22f);
    }
    drawQuad(x, y, w, h);
    if (selected || accent) {
        glColor3f(0.45f, 0.88f, 0.96f);
    } else {
        glColor3f(0.38f, 0.40f, 0.48f);
    }
    glLineWidth(1.f);
    drawOutline(x, y, w, h);

    const float textWidth = static_cast<float>(std::strlen(label)) * 8.f;
    glColor3f(0.94f, 0.96f, 0.98f);
    drawHudText(x + std::max(8.f, (w - textWidth) * 0.5f), y + 10.f, label);
}

// FUNCTION: drawHud draws the side panel, with the file button, the form buttons, the tolerance, and every point from the file.
void drawHud() {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, static_cast<double>(gWidth), 0.0, static_cast<double>(gHeight), -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LINE_STIPPLE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    gButtons.clear();
    const float panelLeft = sidePanelLeft();
    const float panelWidth = sidePanelWidth();
    const float margin = 16.f;
    const float innerX = panelLeft + margin;
    const float innerW = panelWidth - margin * 2.f;

    glColor4f(0.09f, 0.10f, 0.13f, 0.96f);
    drawQuad(panelLeft, 0.f, panelWidth, static_cast<float>(gHeight));
    glColor3f(0.28f, 0.32f, 0.40f);
    glBegin(GL_LINES);
    glVertex2f(panelLeft, 0.f);
    glVertex2f(panelLeft, static_cast<float>(gHeight));
    glEnd();

    float cursor = static_cast<float>(gHeight) - 28.f;
    glColor3f(0.94f, 0.96f, 0.98f);
    drawHudText(innerX, cursor, "EvaluPoint");

    cursor -= 40.f;
    drawButton(innerX, cursor, innerW, 34.f, "Open point file", false, true, UiOpen);

    cursor -= 24.f;
    glColor3f(0.78f, 0.82f, 0.88f);
    drawHudText(innerX, cursor, fitLabel(gFileLabel, 46));
    if (!gStatusMessage.empty()) {
        cursor -= 20.f;
        glColor3f(1.00f, 0.55f, 0.40f);
        drawHudText(innerX, cursor, fitLabel(gStatusMessage, 46));
    }

    cursor -= 30.f;
    glColor3f(0.72f, 0.76f, 0.82f);
    drawHudText(innerX, cursor, "Form");

    const float gap = 8.f;
    const float buttonW = (innerW - gap) * 0.5f;
    const float buttonH = 34.f;
    cursor -= buttonH + 8.f;
    drawButton(innerX, cursor, buttonW, buttonH, "Cylinder", gShape == gdt::ShapeKind::Cylinder, false, UiCylinder);
    drawButton(innerX + buttonW + gap, cursor, buttonW, buttonH, "Square", gShape == gdt::ShapeKind::Square, false, UiSquare);
    cursor -= buttonH + gap;
    drawButton(innerX, cursor, buttonW, buttonH, "Sphere", gShape == gdt::ShapeKind::Sphere, false, UiSphere);
    drawButton(innerX + buttonW + gap, cursor, buttonW, buttonH, "Cone", gShape == gdt::ShapeKind::Cone, false, UiCone);

    cursor -= 48.f;
    glColor3f(0.72f, 0.76f, 0.82f);
    drawHudText(innerX, cursor + 10.f, "Tolerance");
    drawButton(innerX + innerW - 92.f, cursor, 42.f, 32.f, "-", false, false, UiTolDown);
    drawButton(innerX + innerW - 42.f, cursor, 42.f, 32.f, "+", false, false, UiTolUp);
    glColor3f(0.94f, 0.96f, 0.98f);
    drawHudText(innerX + 92.f, cursor + 8.f, gdt::formatNumber(gTolerance).c_str());

    int insideCount = 0;
    for (const gdt::PointVerdict& verdict : gEval.verdicts) {
        if (verdict.inside) {
            ++insideCount;
        }
    }
    const int outsideCount = static_cast<int>(gEval.verdicts.size()) - insideCount;

    cursor -= 28.f;
    if (gEval.verdicts.empty()) {
        glColor3f(0.78f, 0.82f, 0.88f);
        drawHudText(innerX, cursor, "Load a point file to check a form.");
    } else if (gEval.result.isPassed) {
        glColor3f(0.35f, 0.95f, 0.45f);
        drawHudText(innerX, cursor, "Result: PASS");
    } else {
        glColor3f(1.00f, 0.42f, 0.34f);
        drawHudText(innerX, cursor, "Result: FAIL");
    }

    cursor -= 22.f;
    glColor3f(0.90f, 0.92f, 0.95f);
    if (!gEval.verdicts.empty()) {
        const std::string summary = std::string(gEval.result.type) + "  " + gdt::formatNumber(gEval.result.formErr);
        drawHudText(innerX, cursor, summary);
        cursor -= 22.f;
        const std::string counts = "Inside " + std::to_string(insideCount) + "     Outside " + std::to_string(outsideCount);
        drawHudText(innerX, cursor, counts);
    }

    cursor -= 26.f;
    glColor3f(0.72f, 0.76f, 0.82f);
    const std::string pointTitle = "Points in file (" + std::to_string(gEval.verdicts.size()) + ")";
    drawHudText(innerX, cursor, pointTitle);
    cursor -= 20.f;
    glColor3f(0.55f, 0.60f, 0.68f);
    drawHudText(innerX, cursor, "  #       X       Y       Z     Dev");

    gListLeft = innerX;
    gListRight = panelLeft + panelWidth - 14.f;
    gListTop = cursor - 6.f;
    gListBottom = 12.f;
    gVisibleRows = std::max(1, static_cast<int>((gListTop - gListBottom) / gRowHeight));
    const int pointCount = static_cast<int>(gEval.verdicts.size());
    const int maxScroll = std::max(0, pointCount - gVisibleRows);
    if (gListScroll > maxScroll) {
        gListScroll = maxScroll;
    }
    if (gListScroll < 0) {
        gListScroll = 0;
    }

    const int lastRow = std::min(pointCount, gListScroll + gVisibleRows);
    for (int index = gListScroll; index < lastRow; ++index) {
        const float rowTop = gListTop - static_cast<float>(index - gListScroll) * gRowHeight;
        const float rowY = rowTop - gRowHeight;
        const gdt::PointVerdict& verdict = gEval.verdicts[static_cast<std::size_t>(index)];
        if (index == gSelectedPoint) {
            glColor3f(0.28f, 0.32f, 0.18f);
            drawQuad(gListLeft - 4.f, rowY, gListRight - gListLeft - 6.f, gRowHeight);
        } else if (!verdict.inside && gEval.fitOk) {
            glColor3f(0.28f, 0.12f, 0.12f);
            drawQuad(gListLeft - 4.f, rowY + 4.f, 4.f, gRowHeight - 8.f);
        }

        char numbers[96];
        std::snprintf(
            numbers, sizeof(numbers), "%3d %7.3f %7.3f %7.3f %7.3f",
            index, verdict.position.x, verdict.position.y, verdict.position.z, verdict.deviation);
        glColor3f(0.90f, 0.92f, 0.94f);
        drawHudText(gListLeft, rowY + 4.f, numbers);
        if (!gEval.fitOk) {
            glColor3f(0.95f, 0.78f, 0.30f);
            drawHudText(gListRight - 36.f, rowY + 4.f, "...");
        } else if (verdict.inside) {
            glColor3f(0.35f, 0.95f, 0.45f);
            drawHudText(gListRight - 28.f, rowY + 4.f, "IN");
        } else {
            glColor3f(1.00f, 0.38f, 0.30f);
            drawHudText(gListRight - 36.f, rowY + 4.f, "OUT");
        }
    }

    if (pointCount > gVisibleRows && gVisibleRows > 0) {
        const float trackTop = gListTop;
        const float trackHeight = gListTop - gListBottom;
        const float thumbHeight = std::max(24.f, trackHeight * static_cast<float>(gVisibleRows) / static_cast<float>(pointCount));
        const float travel = std::max(0.f, trackHeight - thumbHeight);
        const float fraction = maxScroll == 0 ? 0.f : static_cast<float>(gListScroll) / static_cast<float>(maxScroll);
        const float thumbTop = trackTop - travel * fraction;
        glColor3f(0.22f, 0.24f, 0.30f);
        drawQuad(gListRight - 6.f, gListBottom, 4.f, trackHeight);
        glColor3f(0.55f, 0.62f, 0.70f);
        drawQuad(gListRight - 6.f, thumbTop - thumbHeight, 4.f, thumbHeight);
    }

    if (gEval.verdicts.empty()) {
        glColor3f(0.62f, 0.66f, 0.72f);
        drawHudText(innerX, gListTop - 28.f, "Each line of the file is one point: x y z");
    }

    glColor4f(0.05f, 0.06f, 0.09f, 0.88f);
    drawQuad(0.f, 0.f, panelLeft, 36.f);
    glColor3f(0.78f, 0.82f, 0.88f);
    drawHudText(14.f, 12.f, "Drag orbit    Right-drag pan    Wheel zoom    Click a point to highlight it");

    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

// FUNCTION: renderFrame draws one frame: the fitted shape, every point, and the side panel.
void renderFrame() {
    if (gWidth < 1 || gHeight < 1) {
        return;
    }

    const int viewWidth = std::max(1, static_cast<int>(sidePanelLeft()));
    const int viewHeight = std::max(1, gHeight - 36);
    glViewport(0, 36, viewWidth, viewHeight);
    glClearColor(0.07f, 0.08f, 0.11f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_LINE_STIPPLE);

    float aspect = static_cast<float>(viewWidth) / static_cast<float>(viewHeight);
    float nearClip = std::max(0.01f, gCamera.zoom * 0.002f);
    float farClip = gCamera.zoom + gEval.modelRadius * 80.f + 10.f;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(42.0, aspect, nearClip, farClip);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(gCamera.panX, gCamera.panY, -gCamera.zoom);
    glRotatef(gCamera.pitch, 1.f, 0.f, 0.f);
    glRotatef(gCamera.yaw, 0.f, 1.f, 0.f);
    glTranslatef(-gEval.cloudCenter.x, -gEval.cloudCenter.y, -gEval.cloudCenter.z);

    drawFeature();
    drawPoints();
    glViewport(0, 0, gWidth, gHeight);
    drawHud();
    SwapBuffers(gDevice);
}

// FUNCTION: hitButton reports which panel button is under the cursor. Y is measured from the top of the window.
bool hitButton(int x, int y, int& command) {
    const float glX = static_cast<float>(x);
    const float glY = static_cast<float>(gHeight - y);
    for (const UiButton& button : gButtons) {
        if (glX >= button.x && glX <= button.x + button.w && glY >= button.y && glY <= button.y + button.h) {
            command = button.command;
            return true;
        }
    }
    return false;
}

// FUNCTION: hitPointRow returns the point-list row under the cursor, or false when the cursor is outside the list.
bool hitPointRow(int x, int y, int& index) {
    const float glX = static_cast<float>(x);
    const float glY = static_cast<float>(gHeight - y);
    if (glX < gListLeft || glX > gListRight || glY > gListTop || glY < gListBottom) {
        return false;
    }
    const int row = static_cast<int>((gListTop - glY) / gRowHeight);
    index = gListScroll + row;
    return index >= 0 && index < static_cast<int>(gEval.verdicts.size());
}

// FUNCTION: activateCommand runs whatever the clicked button asked for: open a file, change the form, or step the tolerance.
void activateCommand(int command) {
    switch (command) {
        case UiOpen:
            browseForPointFile();
            break;
        case UiCylinder:
            switchShape(gdt::ShapeKind::Cylinder);
            break;
        case UiSquare:
            switchShape(gdt::ShapeKind::Square);
            break;
        case UiSphere:
            switchShape(gdt::ShapeKind::Sphere);
            break;
        case UiCone:
            switchShape(gdt::ShapeKind::Cone);
            break;
        case UiTolDown:
            bumpTolerance(-0.01f);
            break;
        case UiTolUp:
            bumpTolerance(0.01f);
            break;
        default:
            break;
    }
}

// FUNCTION: mouseX reads the cursor x from a Windows mouse message.
int mouseX(LPARAM param) {
    return static_cast<short>(LOWORD(param));
}

// FUNCTION: mouseY reads the cursor y from a Windows mouse message.
int mouseY(LPARAM param) {
    return static_cast<short>(HIWORD(param));
}

// FUNCTION: windowProc handles clicks, dragging, the wheel, and keys for the OpenGL window.
LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CLOSE:
            DestroyWindow(window);
            return 0;
        case WM_DESTROY:
            releaseGraphics();
            PostQuitMessage(0);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_SIZE:
            gWidth = LOWORD(lParam);
            gHeight = HIWORD(lParam);
            return 0;
        case WM_LBUTTONDOWN: {
            const int x = mouseX(lParam);
            const int y = mouseY(lParam);
            int command = 0;
            int row = 0;
            if (hitButton(x, y, command)) {
                activateCommand(command);
                return 0;
            }
            if (hitPointRow(x, y, row)) {
                gSelectedPoint = row;
                ensureSelectionVisible();
                return 0;
            }
            gLeftDown = true;
            gLastX = x;
            gLastY = y;
            SetCapture(window);
            return 0;
        }
        case WM_RBUTTONDOWN:
            gRightDown = true;
            gLastX = mouseX(lParam);
            gLastY = mouseY(lParam);
            SetCapture(window);
            return 0;
        case WM_LBUTTONUP:
            gLeftDown = false;
            if (!gRightDown) {
                ReleaseCapture();
            }
            return 0;
        case WM_RBUTTONUP:
            gRightDown = false;
            if (!gLeftDown) {
                ReleaseCapture();
            }
            return 0;
        case WM_MOUSEMOVE: {
            int x = mouseX(lParam);
            int y = mouseY(lParam);
            int command = 0;
            gHotButton = hitButton(x, y, command) ? command : 0;
            int dx = x - gLastX;
            int dy = y - gLastY;
            gLastX = x;
            gLastY = y;
            if (gLeftDown) {
                gCamera.yaw += static_cast<float>(dx) * 0.45f;
                gCamera.pitch += static_cast<float>(dy) * 0.45f;
                if (gCamera.pitch > 89.f) gCamera.pitch = 89.f;
                if (gCamera.pitch < -89.f) gCamera.pitch = -89.f;
            } else if (gRightDown) {
                float scale = gCamera.zoom * 0.0022f;
                gCamera.panX += static_cast<float>(dx) * scale;
                gCamera.panY -= static_cast<float>(dy) * scale;
            }
            return 0;
        }
        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            POINT wheelPoint;
            wheelPoint.x = mouseX(lParam);
            wheelPoint.y = mouseY(lParam);
            ScreenToClient(window, &wheelPoint);
            if (static_cast<float>(wheelPoint.x) >= sidePanelLeft()) {
                const int notches = delta / WHEEL_DELTA;
                gListScroll -= (notches == 0 ? (delta > 0 ? 1 : -1) : notches) * 3;
                if (gListScroll < 0) {
                    gListScroll = 0;
                }
                return 0;
            }
            if (delta > 0) {
                gCamera.zoom *= 0.90f;
            } else {
                gCamera.zoom *= 1.10f;
            }
            float minimum = std::max(0.05f, gEval.modelRadius * 0.15f);
            float maximum = std::max(20.f, gEval.modelRadius * 40.f);
            if (gCamera.zoom < minimum) gCamera.zoom = minimum;
            if (gCamera.zoom > maximum) gCamera.zoom = maximum;
            return 0;
        }
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                DestroyWindow(window);
            } else if (wParam == 'R') {
                resetCamera();
            } else if (wParam == VK_OEM_PLUS || wParam == VK_ADD) {
                bumpTolerance(0.01f);
            } else if (wParam == VK_OEM_MINUS || wParam == VK_SUBTRACT) {
                bumpTolerance(-0.01f);
            } else if (wParam == '1') {
                switchShape(gdt::ShapeKind::Cylinder);
            } else if (wParam == '2') {
                switchShape(gdt::ShapeKind::Square);
            } else if (wParam == '3') {
                switchShape(gdt::ShapeKind::Sphere);
            } else if (wParam == '4') {
                switchShape(gdt::ShapeKind::Cone);
            } else if (wParam == 'O') {
                browseForPointFile();
            } else if (wParam == VK_UP) {
                if (!gEval.verdicts.empty()) {
                    gSelectedPoint = gSelectedPoint < 0 ? 0 : std::max(0, gSelectedPoint - 1);
                    ensureSelectionVisible();
                }
            } else if (wParam == VK_DOWN) {
                if (!gEval.verdicts.empty()) {
                    const int last = static_cast<int>(gEval.verdicts.size()) - 1;
                    gSelectedPoint = gSelectedPoint < 0 ? 0 : std::min(last, gSelectedPoint + 1);
                    ensureSelectionVisible();
                }
            } else if (wParam == VK_PRIOR) {
                gListScroll = std::max(0, gListScroll - gVisibleRows);
            } else if (wParam == VK_NEXT) {
                gListScroll += gVisibleRows;
            }
            return 0;
        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT && gHotButton != 0) {
                SetCursor(LoadCursor(nullptr, IDC_HAND));
                return TRUE;
            }
            break;
        default:
            break;
    }
    return DefWindowProcA(window, message, wParam, lParam);
}

// FUNCTION: splashProc paints the small EvaluPoint window shown while the viewer starts
LRESULT CALLBACK splashProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT paint;
        HDC device = BeginPaint(window, &paint);
        RECT client;
        GetClientRect(window, &client);

        HBRUSH background = CreateSolidBrush(RGB(16, 18, 22));
        FillRect(device, &client, background);
        DeleteObject(background);

        SetBkMode(device, TRANSPARENT);
        HFONT titleFont = CreateFontA(
            -32, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_SWISS, "Segoe UI");
        HFONT previousFont = static_cast<HFONT>(SelectObject(device, titleFont));
        SetTextColor(device, RGB(244, 246, 248));
        RECT titleRect = client;
        titleRect.bottom = (client.bottom + client.top) / 2 + 4;
        DrawTextA(device, "EvaluPoint 1", -1, &titleRect, DT_CENTER | DT_BOTTOM | DT_SINGLELINE);

        HFONT noteFont = CreateFontA(
            -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_SWISS, "Segoe UI");
        SelectObject(device, noteFont);
        SetTextColor(device, RGB(168, 176, 188));
        RECT noteRect = client;
        noteRect.top = (client.bottom + client.top) / 2 + 14;
        DrawTextA(device, "Initializing...", -1, &noteRect, DT_CENTER | DT_TOP | DT_SINGLELINE);

        SelectObject(device, previousFont);
        DeleteObject(titleFont);
        DeleteObject(noteFont);
        EndPaint(window, &paint);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    default:
        break;
    }
    return DefWindowProcA(window, message, wParam, lParam);
}

// FUNCTION: openStartupSplash shows a small EvaluPoint 1 window in the center of the screen
HWND openStartupSplash(DWORD& openedAt) {
    HINSTANCE instance = GetModuleHandleA(nullptr);
    const char* className = "EvaluPointSplash";
    WNDCLASSA windowClass = {};
    windowClass.lpfnWndProc = splashProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    windowClass.lpszClassName = className;
    if (!RegisterClassA(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return nullptr;
    }

    const int clientWidth = 380;
    const int clientHeight = 150;
    RECT bounds = {0, 0, clientWidth, clientHeight};
    const DWORD style = WS_POPUP | WS_CAPTION | WS_BORDER;
    AdjustWindowRect(&bounds, style, FALSE);
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    const int x = std::max(0, (GetSystemMetrics(SM_CXSCREEN) - width) / 2);
    const int y = std::max(0, (GetSystemMetrics(SM_CYSCREEN) - height) / 2);

    HWND splash = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        className,
        "EvaluPoint 1",
        style,
        x, y, width, height,
        nullptr, nullptr, instance, nullptr);
    if (!splash) {
        return nullptr;
    }

    openedAt = GetTickCount();
    ShowWindow(splash, SW_SHOW);
    UpdateWindow(splash);
    return splash;
}

// FUNCTION: closeStartupSplash keeps the startup window up briefly, then removes it
void closeStartupSplash(HWND splash, DWORD openedAt) {
    if (!splash) {
        return;
    }

    const DWORD holdMs = 1200;
    MSG message;
    while (GetTickCount() - openedAt < holdMs) {
        while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }
        Sleep(16);
    }

    DestroyWindow(splash);
}

// FUNCTION: runViewer opens the 3D window and keeps drawing until the user closes it.
bool runViewer(gdt::ShapeKind kind, const std::vector<gdt::Point3D>& points, float tolerance) {
    DWORD splashOpenedAt = 0;
    HWND splash = openStartupSplash(splashOpenedAt);
    gShape = kind;
    gPoints = points;
    gTolerance = tolerance;
    gLeftDown = false;
    gRightDown = false;
    recompute();
    frameCamera();

    HINSTANCE instance = GetModuleHandleA(nullptr);
    if (!gClassRegistered) {
        WNDCLASSA windowClass = {};
        windowClass.style = CS_OWNDC;
        windowClass.lpfnWndProc = windowProc;
        windowClass.hInstance = instance;
        windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
        windowClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        windowClass.lpszClassName = "GDTEvaluatorGL";
        if (!RegisterClassA(&windowClass)) {
            std::cerr << "Could not register the OpenGL window class." << std::endl;
            if (splash) {
                DestroyWindow(splash);
            }
            return false;
        }
        gClassRegistered = true;
    }

    gWindow = CreateWindowA(
        "GDTEvaluatorGL",
        "EvaluPoint",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1400, 860,
        nullptr, nullptr, instance, nullptr);
    if (!gWindow) {
        std::cerr << "Could not create the OpenGL window." << std::endl;
        if (splash) {
            DestroyWindow(splash);
        }
        return false;
    }

    gDevice = GetDC(gWindow);
    PIXELFORMATDESCRIPTOR descriptor = {};
    descriptor.nSize = sizeof(descriptor);
    descriptor.nVersion = 1;
    descriptor.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    descriptor.iPixelType = PFD_TYPE_RGBA;
    descriptor.cColorBits = 32;
    descriptor.cDepthBits = 24;
    descriptor.iLayerType = PFD_MAIN_PLANE;
    int format = ChoosePixelFormat(gDevice, &descriptor);
    if (format == 0 || !SetPixelFormat(gDevice, format, &descriptor)) {
        std::cerr << "Could not choose an OpenGL pixel format." << std::endl;
        ReleaseDC(gWindow, gDevice);
        gDevice = nullptr;
        DestroyWindow(gWindow);
        if (splash) {
            DestroyWindow(splash);
        }
        return false;
    }

    gContext = wglCreateContext(gDevice);
    if (!gContext || !wglMakeCurrent(gDevice, gContext)) {
        std::cerr << "Could not create the OpenGL context." << std::endl;
        releaseGraphics();
        DestroyWindow(gWindow);
        if (splash) {
            DestroyWindow(splash);
        }
        return false;
    }

    createHudFont(gDevice);
    recompute();
    closeStartupSplash(splash, splashOpenedAt);
    ShowWindow(gWindow, SW_SHOW);
    UpdateWindow(gWindow);
    SetForegroundWindow(gWindow);

    std::cout << "3D view is open. Close the window to return to the menu." << std::endl;

    MSG message;
    bool running = true;
    while (running) {
        while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                running = false;
                break;
            }
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }
        if (!running || !gContext) {
            break;
        }
        renderFrame();
        Sleep(16);
    }

    gWindow = nullptr;
    return true;
}

// FUNCTION: printReport writes the form error and an inside or outside line for each point. The report-only path uses this.
void printReport(const gdt::Evaluation& eval) {
    std::cout << std::endl;
    for (const std::string& line : eval.reportLines) {
        std::cout << line << std::endl;
    }
    if (eval.verdicts.empty()) {
        std::cout << std::endl;
        return;
    }

    std::cout << std::endl;
    std::cout << "Deviation is the signed distance from the drawn nominal surface." << std::endl;
    const int count = static_cast<int>(eval.verdicts.size());
    const bool listAll = count <= 500;
    if (!listAll) {
        std::cout << "Listing points that fall outside the zone (the cloud has " << count << " points)." << std::endl;
    }

    std::cout << std::fixed << std::setprecision(4);
    std::cout << std::setw(7) << "Index"
              << std::setw(12) << "X"
              << std::setw(12) << "Y"
              << std::setw(12) << "Z"
              << std::setw(14) << "Deviation"
              << "   Verdict" << std::endl;

    for (int i = 0; i < count; ++i) {
        const gdt::PointVerdict& verdict = eval.verdicts[static_cast<std::size_t>(i)];
        if (!listAll && verdict.inside) {
            continue;
        }
        std::cout << std::setw(7) << i
                  << std::setw(12) << verdict.position.x
                  << std::setw(12) << verdict.position.y
                  << std::setw(12) << verdict.position.z
                  << std::setw(14) << verdict.deviation
                  << "   " << (verdict.inside ? "INSIDE" : "OUTSIDE")
                  << std::endl;
    }
    std::cout << std::endl;
}

// FUNCTION: trimCopy strips spaces and a matching pair of quotes from the ends of a string.
std::string trimCopy(std::string text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
        text.erase(text.begin());
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
        text.pop_back();
    }
    if (text.size() >= 2 &&
        ((text.front() == '"' && text.back() == '"') || (text.front() == '\'' && text.back() == '\''))) {
        text = text.substr(1, text.size() - 2);
    }
    return text;
}

// FUNCTION: lowerCopy returns the same text in lowercase, so shape names can be matched loosely.
std::string lowerCopy(std::string text) {
    for (char& character : text) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return text;
}

// FUNCTION: parseShape accepts a shape name or the numbers 1 to 4 from the command line.
bool parseShape(const std::string& text, gdt::ShapeKind& kind) {
    std::string key = lowerCopy(trimCopy(text));
    if (key == "1" || key == "cylinder" || key == "c") {
        kind = gdt::ShapeKind::Cylinder;
        return true;
    }
    if (key == "2" || key == "square" || key == "s") {
        kind = gdt::ShapeKind::Square;
        return true;
    }
    if (key == "3" || key == "sphere") {
        kind = gdt::ShapeKind::Sphere;
        return true;
    }
    if (key == "4" || key == "cone") {
        kind = gdt::ShapeKind::Cone;
        return true;
    }
    return false;
}

// FUNCTION: parseTolerance reads a positive tolerance number from text.
bool parseTolerance(const std::string& text, float& tolerance) {
    std::string trimmed = trimCopy(text);
    if (trimmed.empty()) {
        return false;
    }
    char* end = nullptr;
    float value = std::strtof(trimmed.c_str(), &end);
    if (end == trimmed.c_str() || !std::isfinite(value) || value <= 0.f) {
        return false;
    }
    tolerance = value;
    return true;
}

// FUNCTION: defaultFileFor returns the sample point file that matches a form.
std::string defaultFileFor(gdt::ShapeKind kind) {
    switch (kind) {
        case gdt::ShapeKind::Cylinder: return "samples/cylinder.txt";
        case gdt::ShapeKind::Square: return "samples/square.txt";
        case gdt::ShapeKind::Sphere: return "samples/sphere.txt";
        case gdt::ShapeKind::Cone: return "samples/cone.txt";
    }
    return "samples/cylinder.txt";
}

// FUNCTION: printUsage prints how to start the program from the command line.
void printUsage() {
    std::cout << "Usage:\n"
              << "  gdtevaluator\n"
              << "  gdtevaluator <cylinder|square|sphere|cone> <points.txt> <tolerance>\n"
              << "  gdtevaluator --report-only <shape> <points.txt> <tolerance>\n"
              << "  gdtevaluator --self-test\n";
}

// FUNCTION: evaluateFile loads a point file. With the window open, the points show in the list and on the shape. Without it, the result is printed

bool evaluateFile(gdt::ShapeKind kind, const std::string& path, float tolerance, bool openView) {
    std::vector<gdt::Point3D> points = gdt::readGivenFile(path);
    if (points.empty()) {
        std::cout << "Nothing to evaluate. Check the file path and the x y z layout." << std::endl;
        return false;
    }
    if (!openView) {
        printReport(gdt::evaluateShape(kind, points, tolerance));
        return true;
    }
    gFilePath = path;
    gFileLabel = fileNameOnly(path);
    gStatusMessage.clear();
    gSelectedPoint = -1;
    gListScroll = 0;
    return runViewer(kind, points, tolerance);
}

// FUNCTION: interactiveLoop opens the view right away, with the sample cylinder loaded so the file button and the form buttons are ready to use

int interactiveLoop() {
    const std::string path = defaultFileFor(gdt::ShapeKind::Cylinder);
    std::vector<gdt::Point3D> points = gdt::readGivenFile(path);
    std::cout << "The view is open. Use Open point file and the form buttons." << std::endl;
    if (points.empty()) {
        gFileLabel = "No file loaded";
        gStatusMessage = "Open a point file. Each line should be x y z.";
        return runViewer(gdt::ShapeKind::Cylinder, {}, 0.05f) ? 0 : 1;
    }
    gFilePath = path;
    gFileLabel = fileNameOnly(path);
    gStatusMessage.clear();
    return runViewer(gdt::ShapeKind::Cylinder, points, 0.05f) ? 0 : 1;
}

// FUNCTION: expect prints OK or FAIL for one self-test check

bool expect(bool condition, const std::string& name, const std::string& detail) {
    if (condition) {
        std::cout << "  OK   " << name << std::endl;
        return true;
    }
    std::cout << "  FAIL " << name << " " << detail << std::endl;
    return false;
}

// FUNCTION: countOutside counts how many points the check marked outside the zone

int countOutside(const gdt::Evaluation& eval) {
    int outside = 0;
    for (const gdt::PointVerdict& verdict : eval.verdicts) {
        if (!verdict.inside) {
            ++outside;
        }
    }
    return outside;
}

// FUNCTION: runSelfTest checks that a perfect cylinder, square, sphere, and cone pass, and that one outlier on a cylinder falls outside the zone

bool runSelfTest() {
    bool allPassed = true;
    std::cout << "Geometry self-test" << std::endl;

    double diagonal[3][3] = {
        {4.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
        {0.0, 0.0, 9.0}
    };
    gdt::SymmetricEigen3 eigen = gdt::eigenSymmetric3(diagonal);
    allPassed &= expect(
        std::fabs(eigen.value[0] - 1.0) < 1e-6 &&
        std::fabs(eigen.value[1] - 4.0) < 1e-6 &&
        std::fabs(eigen.value[2] - 9.0) < 1e-6,
        "Jacobi eigenvalues",
        "got " + std::to_string(eigen.value[0]) + ", " + std::to_string(eigen.value[1]) + ", " + std::to_string(eigen.value[2]));

    double cosine = std::cos(0.4);
    double sine = std::sin(0.4);
    double rotated[3][3] = {};
    double basis[3][3] = {
        {cosine, 0.0, -sine},
        {0.0, 1.0, 0.0},
        {sine, 0.0, cosine}
    };
    double lambdas[3] = {1.0, 4.0, 9.0};
    for (int col = 0; col < 3; ++col) {
        for (int row = 0; row < 3; ++row) {
            for (int k = 0; k < 3; ++k) {
                rotated[row][k] += lambdas[col] * basis[row][col] * basis[k][col];
            }
        }
    }
    gdt::SymmetricEigen3 rotatedEigen = gdt::eigenSymmetric3(rotated);
    double eigenError = 0.0;
    double expectedLambda[3] = {1.0, 4.0, 9.0};
    for (int i = 0; i < 3; ++i) {
        eigenError = std::max(eigenError, std::fabs(rotatedEigen.value[i] - expectedLambda[i]));
        double prediction[3] = {0.0, 0.0, 0.0};
        double components[3] = {rotatedEigen.axis[i].x, rotatedEigen.axis[i].y, rotatedEigen.axis[i].z};
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 3; ++col) {
                prediction[row] += rotated[row][col] * components[col];
            }
        }
        for (int row = 0; row < 3; ++row) {
            eigenError = std::max(eigenError, std::fabs(prediction[row] - expectedLambda[i] * components[row]));
        }
    }
    allPassed &= expect(eigenError < 1e-5, "Jacobi rotated matrix", "error " + std::to_string(eigenError));

    std::vector<gdt::Point3D> planePoints;
    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 5; ++j) {
            planePoints.push_back(gdt::Point3D(-1.f + 0.5f * i, -1.f + 0.5f * j, 2.f));
        }
    }
    gdt::Plane plane = gdt::fitPlane(planePoints);
    allPassed &= expect(
        std::fabs(plane.c) > 0.99f && gdt::evaluateFlatness(planePoints, plane) < 1e-4f,
        "Plane flatness",
        "normal z " + std::to_string(plane.c) + " flatness " + std::to_string(gdt::evaluateFlatness(planePoints, plane)));

    std::vector<gdt::Point3D> circlePoints;
    for (int i = 0; i < 8; ++i) {
        float angle = gdt::kPi * 2.f * static_cast<float>(i) / 8.f;
        circlePoints.push_back(gdt::Point3D(std::cos(angle), std::sin(angle), 4.f));
    }
    gdt::Circle circle = gdt::fitCircle(circlePoints);
    allPassed &= expect(
        std::fabs(circle.radius - 1.f) < 1e-4f && gdt::evaluateRoundness(circle, circlePoints) < 1e-4f,
        "Circle roundness",
        "radius " + std::to_string(circle.radius));

    gdt::Point3D normal = gdt::normalizeVector(gdt::Point3D(1.f, 1.f, 1.f));
    gdt::Point3D tangent;
    gdt::Point3D bitangent;
    gdt::orthonormalBasis(normal, tangent, bitangent);
    std::vector<gdt::Point3D> squarePoints;
    for (int i = -2; i <= 2; ++i) {
        for (int j = -2; j <= 2; ++j) {
            squarePoints.push_back(normal * 3.f + tangent * (0.4f * i) + bitangent * (0.4f * j));
        }
    }
    gdt::Evaluation squareEval = gdt::evaluateShape(gdt::ShapeKind::Square, squarePoints, 0.01f);
    allPassed &= expect(
        squareEval.fitOk && squareEval.result.isPassed && squareEval.result.formErr < 1e-3f &&
        std::fabs(squareEval.square.halfSide - 0.8f) < 0.05f,
        "Tilted square flatness",
        "form " + std::to_string(squareEval.result.formErr) + " halfSide " + std::to_string(squareEval.square.halfSide));

    auto makeCylinder = [](const gdt::Point3D& origin, const gdt::Point3D& direction, float radius) {
        std::vector<gdt::Point3D> points;
        gdt::Point3D tangentAxis;
        gdt::Point3D bitangentAxis;
        gdt::orthonormalBasis(direction, tangentAxis, bitangentAxis);
        for (int i = 0; i < 16; ++i) {
            float angle = gdt::kPi * 2.f * static_cast<float>(i) / 16.f;
            for (int k = 0; k < 8; ++k) {
                float height = -1.f + 2.f * static_cast<float>(k) / 7.f;
                points.push_back(
                    origin + direction * height +
                    (tangentAxis * std::cos(angle) + bitangentAxis * std::sin(angle)) * radius);
            }
        }
        return points;
    };

    gdt::Point3D cylinderDirection = gdt::normalizeVector(gdt::Point3D(0.2f, 0.5f, 0.8f));
    std::vector<gdt::Point3D> cylinderPoints = makeCylinder(gdt::Point3D(1.f, -2.f, 0.5f), cylinderDirection, 1.5f);
    gdt::Evaluation cylinderEval = gdt::evaluateShape(gdt::ShapeKind::Cylinder, cylinderPoints, 0.01f);
    float axisAgreement = std::fabs(gdt::dot(cylinderEval.cylinder.axisDirection, cylinderDirection));
    allPassed &= expect(
        cylinderEval.fitOk && cylinderEval.result.isPassed && cylinderEval.result.formErr < 1e-3f &&
        std::fabs(cylinderEval.cylinder.radius - 1.5f) < 1e-3f && axisAgreement > 0.999f,
        "Tilted cylinder cylindricity",
        "form " + std::to_string(cylinderEval.result.formErr) +
        " radius " + std::to_string(cylinderEval.cylinder.radius) +
        " axis dot " + std::to_string(axisAgreement));

    gdt::Point3D outlierDirection;
    gdt::Point3D outlierBitangent;
    gdt::orthonormalBasis(cylinderDirection, outlierDirection, outlierBitangent);
    cylinderPoints.push_back(gdt::Point3D(1.f, -2.f, 0.5f) + outlierDirection * 1.75f);
    gdt::Evaluation outlierEval = gdt::evaluateShape(gdt::ShapeKind::Cylinder, cylinderPoints, 0.05f);
    allPassed &= expect(
        outlierEval.fitOk && !outlierEval.result.isPassed && outlierEval.result.formErr > 0.2f &&
        countOutside(outlierEval) >= 1 && countOutside(outlierEval) < 8,
        "Cylinder outlier is outside the zone",
        "form " + std::to_string(outlierEval.result.formErr) + " outside " + std::to_string(countOutside(outlierEval)));

    std::vector<gdt::Point3D> spherePoints;
    gdt::Point3D sphereCenter(1.5f, -2.f, 0.75f);
    const float sphereRadius = 2.25f;
    for (int stack = 0; stack <= 8; ++stack) {
        float phi = gdt::kPi * static_cast<float>(stack) / 8.f;
        float ring = std::sin(phi);
        float y = std::cos(phi);
        int slices = (stack == 0 || stack == 8) ? 1 : 12;
        for (int slice = 0; slice < slices; ++slice) {
            float theta = gdt::kPi * 2.f * static_cast<float>(slice) / static_cast<float>(slices);
            spherePoints.push_back(sphereCenter + gdt::Point3D(
                sphereRadius * ring * std::cos(theta),
                sphereRadius * y,
                sphereRadius * ring * std::sin(theta)));
        }
    }
    gdt::Evaluation sphereEval = gdt::evaluateShape(gdt::ShapeKind::Sphere, spherePoints, 0.01f);
    float centerError = gdt::length(sphereEval.sphere.center - sphereCenter);
    allPassed &= expect(
        sphereEval.fitOk && sphereEval.result.isPassed && sphereEval.result.formErr < 1e-3f &&
        centerError < 1e-3f && std::fabs(sphereEval.zone.center - sphereRadius) < 1e-3f,
        "Sphere sphericity",
        "form " + std::to_string(sphereEval.result.formErr) +
        " center error " + std::to_string(centerError) +
        " radius " + std::to_string(sphereEval.zone.center));

    gdt::Point3D coneDirection = gdt::normalizeVector(gdt::Point3D(0.1f, -0.2f, 1.f));
    gdt::Point3D coneTangent;
    gdt::Point3D coneBitangent;
    gdt::orthonormalBasis(coneDirection, coneTangent, coneBitangent);
    std::vector<gdt::Point3D> conePoints;
    const float coneSlope = 0.35f;
    gdt::Point3D apex(0.4f, 0.2f, -0.5f);
    for (int i = 0; i < 18; ++i) {
        float angle = gdt::kPi * 2.f * static_cast<float>(i) / 18.f;
        for (int k = 0; k < 8; ++k) {
            float height = 0.6f + 0.25f * static_cast<float>(k);
            float radius = coneSlope * height;
            conePoints.push_back(
                apex + coneDirection * height +
                (coneTangent * std::cos(angle) + coneBitangent * std::sin(angle)) * radius);
        }
    }
    gdt::Evaluation coneEval = gdt::evaluateShape(gdt::ShapeKind::Cone, conePoints, 0.01f);
    float coneAxis = std::fabs(gdt::dot(coneEval.cone.axisDirection, coneDirection));
    allPassed &= expect(
        coneEval.fitOk && coneEval.result.isPassed && coneEval.result.formErr < 1e-3f &&
        std::fabs(std::fabs(coneEval.cone.slope) - coneSlope) < 1e-3f && coneAxis > 0.999f,
        "Tilted cone form",
        "form " + std::to_string(coneEval.result.formErr) +
        " slope " + std::to_string(coneEval.cone.slope) +
        " axis dot " + std::to_string(coneAxis));

    std::cout << (allPassed ? "Self-test passed." : "Self-test failed.") << std::endl;
    return allPassed;
}

}

// FUNCTION: main starts a self-test, a text report, a file given on the command line, or the window with the sample cylinder

int main(int argc, char** argv) {
    if (argc >= 2 && std::string(argv[1]) == "--self-test") {
        return runSelfTest() ? 0 : 1;
    }
    if (argc >= 2 && std::string(argv[1]) == "--report-only") {
        if (argc < 5) {
            printUsage();
            return 1;
        }
        gdt::ShapeKind kind;
        float tolerance = 0.f;
        if (!parseShape(argv[2], kind) || !parseTolerance(argv[4], tolerance)) {
            printUsage();
            return 1;
        }
        return evaluateFile(kind, argv[3], tolerance, false) ? 0 : 1;
    }
    if (argc >= 4) {
        gdt::ShapeKind kind;
        float tolerance = 0.f;
        if (!parseShape(argv[1], kind) || !parseTolerance(argv[3], tolerance)) {
            printUsage();
            return 1;
        }
        return evaluateFile(kind, argv[2], tolerance, true) ? 0 : 1;
    }
    if (argc > 1) {
        printUsage();
        return 1;
    }
    return interactiveLoop();
}
