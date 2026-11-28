#pragma once
#include <algorithm>

// Logical (96-DPI) coordinates shared by the page outlines and live controls.
struct DispatchLayout {
    static constexpr int minimumHeight=1016;
    static constexpr int fieldTop(int row) {
        return 156+row*36+(row>=3?12:0)+(row>=5?12:0);
    }
    int routeBottom, guidanceTop, guidanceHeight, actionsTop, actionsHeight, statusTop, statusHeight;
    DispatchLayout(bool live, bool multiple, int height,bool multiDrop=false) {
        const int rows=multiple?(live?(multiDrop?9:8):7):5;
        routeBottom=fieldTop(rows-1)+52;
        guidanceTop=routeBottom+12;guidanceHeight=live?194:84;
        actionsTop=guidanceTop+guidanceHeight+12;actionsHeight=live?90:140;
        statusTop=actionsTop+actionsHeight+12;
        statusHeight=(std::max)(140,height-16-statusTop);
    }
};
