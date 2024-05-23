#pragma once
#include "imgui/imgui.h"

class Camera {
public:
    ImVec2 center;  // Center of the camera in world coordinates
    ImVec2 position;
    ImVec2 center_pos; //coordinates of the screen center
    float zoom;     // Zoom level
    int viewportWidth;  // Width of the camera's viewport
    int viewportHeight; // Height of the camera's viewport

    Camera(ImVec2 initPosition, float initZoom, int width, int height)
        : position(initPosition), zoom(initZoom), viewportWidth(width), viewportHeight(height)
    {
        center = ImVec2(initPosition.x + width/2, initPosition.y + height/2);
    }

    void setViewportSize(int width, int height) {
        viewportWidth = width;
        viewportHeight = height;
    }

    void adjustZoom(float zoomChange) {
        float zoomSensitivity = 0.1f;  // Sensitivity of zoom changes
        zoom *= (1.0f + zoomChange * zoomSensitivity);
        zoom = std::max(zoom, 0.1f);   // Prevent zoom from going below a minimum level
        zoom = std::min(zoom, 10.0f);  // Prevent zoom from going above a maximum level
    }

    void setCenter() {
        center = ImVec2(position.x + viewportWidth / 2, position.y + viewportHeight / 2);
    }

    ImVec2 getTransformedPosition(ImVec2 worldPosition) {
        ImVec2 topLeftWorld = ImVec2(center.x - viewportWidth * 0.5f / zoom,
            center.y - viewportHeight * 0.5f / zoom);
        ImVec2 screenPosition;
        screenPosition.x = (worldPosition.x - topLeftWorld.x) * zoom;
        screenPosition.y = (worldPosition.y - topLeftWorld.y) * zoom;
        screenPosition.x += viewportWidth * 0.5f;
        screenPosition.y += viewportHeight * 0.5f;
        return screenPosition;

        //ImVec2 screenPosition;
        //// Calculate the world coordinates of the top-left corner of the viewport
        //ImVec2 topLeftWorld = ImVec2(center.x - viewportWidth * 0.5f / zoom, center.y - viewportHeight * 0.5f / zoom);
        //// Translate the world position relative to the top-left and then scale according to zoom
        //screenPosition.x = (worldPosition.x - topLeftWorld.x) * zoom;
        //screenPosition.y = (worldPosition.y - topLeftWorld.y) * zoom;
        //// Since we've calculated relative to the top-left, add half of the viewport dimensions to center
        //screenPosition.x += viewportWidth * 0.5f;
        //screenPosition.y += viewportHeight * 0.5f;

        //return screenPosition;
        ///*ImVec2 screenPosition;
        //ImVec2 offset = ImVec2(viewportWidth * 0.5f / zoom, viewportHeight * 0.5f / zoom);
        //screenPosition.x = (worldPosition.x - (center.x - offset.x)) * zoom;
        //screenPosition.y = (worldPosition.y - (center.y - offset.y)) * zoom;
        //screenPosition.x += viewportWidth * 0.5f;
        //screenPosition.y += viewportHeight * 0.5f;
        //return screenPosition;*/
    }
};
