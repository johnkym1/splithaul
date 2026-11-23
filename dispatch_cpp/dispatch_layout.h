#pragma once
#include <algorithm>

// Logical (96-DPI) coordinates shared by the page outlines and live controls.
struct DispatchLayout {
    int routeBottom, guidanceTop, guidanceHeight, actionsTop, actionsHeight, statusTop, statusHeight;
    DispatchLayout(bool live, bool multiple, int height) {
        const int rows=multiple?(live?8:7):5;
        routeBottom=160+rows*36;
        guidanceTop=routeBottom+12;guidanceHeight=live?170:84;
        actionsTop=guidanceTop+guidanceHeight+12;actionsHeight=live?90:140;
        statusTop=actionsTop+actionsHeight+12;
        statusHeight=(std::max)(140,height-16-statusTop);
    }
};
