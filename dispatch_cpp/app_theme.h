#pragma once
#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <objidl.h>
#include <gdiplus.h>
#include <string>
#include <algorithm>
class AppTheme {
    HBRUSH backgroundBrush=nullptr,surfaceBrush=nullptr;
    inline static constexpr wchar_t hoverProperty[]=L"GTDispatchControlHover";
    static Gdiplus::Color tint(COLORREF value){return Gdiplus::Color(255,GetRValue(value),GetGValue(value),GetBValue(value));}
    static void rounded(Gdiplus::GraphicsPath& path,const Gdiplus::RectF& box,float radius){
        const float diameter=(std::min)(radius*2,(std::min)(box.Width,box.Height));
        path.AddArc(box.X,box.Y,diameter,diameter,180,90);path.AddArc(box.GetRight()-diameter,box.Y,diameter,diameter,270,90);path.AddArc(box.GetRight()-diameter,box.GetBottom()-diameter,diameter,diameter,0,90);path.AddArc(box.X,box.GetBottom()-diameter,diameter,diameter,90,90);path.CloseFigure();
    }
    void paintCombo(HWND window)const{
        HDC dc=GetDC(window);if(!dc)return;RECT box;GetClientRect(window,&box);const auto dpi=static_cast<int>(GetDpiForWindow(window));const int edge=(std::max)(1,MulDiv(1,dpi,96));
        const bool enabled=IsWindowEnabled(window)!=FALSE,focused=GetFocus()==window,hover=GetPropW(window,hoverProperty)!=nullptr;
        const auto outline=CreateSolidBrush(focused||hover?accent:border);FrameRect(dc,&box,outline);DeleteObject(outline);auto inner=box;InflateRect(&inner,-edge,-edge);FrameRect(dc,&inner,surfaceBrush);
        const int arrowWidth=MulDiv(28,dpi,96);RECT arrow={box.right-arrowWidth,edge,box.right-edge,box.bottom-edge};FillRect(dc,&arrow,surfaceBrush);
        {
            Gdiplus::Graphics graphics(dc);graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);const float inset=static_cast<float>(MulDiv(3,dpi,96));
            const Gdiplus::RectF tile(static_cast<float>(arrow.left)+inset,static_cast<float>(arrow.top)+inset,static_cast<float>(arrow.right-arrow.left)-inset*2,static_cast<float>(arrow.bottom-arrow.top)-inset*2);
            Gdiplus::GraphicsPath shape;rounded(shape,tile,static_cast<float>(MulDiv(4,dpi,96)));Gdiplus::SolidBrush fill(tint(enabled?(hover?accentHover:accent):button));graphics.FillPath(&fill,&shape);
            Gdiplus::Pen pen(tint(enabled?RGB(255,247,249):muted),static_cast<float>((std::max)(1,MulDiv(2,dpi,96))));pen.SetStartCap(Gdiplus::LineCapRound);pen.SetEndCap(Gdiplus::LineCapRound);
            const float x=tile.X+tile.Width/2,y=tile.Y+tile.Height/2,span=static_cast<float>(MulDiv(3,dpi,96));graphics.DrawLine(&pen,x-span,y-span/2,x,y+span/2);graphics.DrawLine(&pen,x,y+span/2,x+span,y-span/2);
        }
        ReleaseDC(window,dc);
    }
    void paintTextScrollbar(HWND window)const{
        if(!(GetWindowLongW(window,GWL_STYLE)&WS_VSCROLL))return;
        SCROLLBARINFO info={};info.cbSize=sizeof(info);
        if(!GetScrollBarInfo(window,OBJID_VSCROLL,&info)||(info.rgstate[0]&STATE_SYSTEM_INVISIBLE))return;
        RECT frame;GetWindowRect(window,&frame);auto bar=info.rcScrollBar;OffsetRect(&bar,-frame.left,-frame.top);
        if(bar.right<=bar.left||bar.bottom<=bar.top)return;
        const auto dc=GetWindowDC(window);if(!dc)return;
        FillRect(dc,&bar,surfaceBrush);
        const auto oldPen=SelectObject(dc,GetStockObject(DC_PEN)),oldBrush=SelectObject(dc,GetStockObject(DC_BRUSH));
        const int dpi=static_cast<int>(GetDpiForWindow(window)),inset=(std::max)(2,MulDiv(3,dpi,96));
        const bool disabled=(info.rgstate[0]&STATE_SYSTEM_UNAVAILABLE)!=0;
        auto arrow=[&](bool down){
            RECT tile=bar;if(down)tile.top=tile.bottom-info.dxyLineButton;else tile.bottom=tile.top+info.dxyLineButton;
            SetDCPenColor(dc,surface);SetDCBrushColor(dc,button);Rectangle(dc,tile.left,tile.top,tile.right,tile.bottom);
            const int x=(tile.left+tile.right)/2,y=(tile.top+tile.bottom)/2,span=(std::max)(2,MulDiv(3,dpi,96));
            const bool unavailable=disabled||(info.rgstate[down?5:1]&STATE_SYSTEM_UNAVAILABLE)!=0;
            SetDCPenColor(dc,unavailable?muted:foreground);SetDCBrushColor(dc,unavailable?muted:foreground);
            POINT points[3]={{x-span,y+(down?-1:1)*span/2},{x+span,y+(down?-1:1)*span/2},{x,y+(down?1:-1)*span}};Polygon(dc,points,3);
        };
        arrow(false);arrow(true);
        if(!disabled&&info.xyThumbBottom>info.xyThumbTop){
            SetDCPenColor(dc,accent);SetDCBrushColor(dc,accent);
            RoundRect(dc,bar.left+inset,bar.top+info.xyThumbTop,bar.right-inset,bar.top+info.xyThumbBottom,inset*2,inset*2);
        }
        SelectObject(dc,oldBrush);SelectObject(dc,oldPen);ReleaseDC(window,dc);
    }
    static LRESULT CALLBACK controlSkin(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR context){
        auto* theme=reinterpret_cast<AppTheme*>(context);
        if(message==WM_MOUSEMOVE&&!GetPropW(window,hoverProperty)){SetPropW(window,hoverProperty,reinterpret_cast<HANDLE>(1));TRACKMOUSEEVENT tracking={sizeof(tracking),TME_LEAVE,window,0};TrackMouseEvent(&tracking);InvalidateRect(window,nullptr,FALSE);}
        if(message==WM_MOUSELEAVE){RemovePropW(window,hoverProperty);InvalidateRect(window,nullptr,FALSE);}
        if(message==WM_ENABLE&&!wp)RemovePropW(window,hoverProperty);
        if(message==WM_NCDESTROY){RemovePropW(window,hoverProperty);RemoveWindowSubclass(window,controlSkin,id);return DefSubclassProc(window,message,wp,lp);}
        const auto result=DefSubclassProc(window,message,wp,lp);
        if(message==WM_PAINT){wchar_t name[32]={};GetClassNameW(window,name,32);if(_wcsicmp(name,L"ComboBox")==0)theme->paintCombo(window);}
        switch(message){
        case WM_NCPAINT:case WM_PAINT:case WM_SIZE:case WM_SETTEXT:case WM_VSCROLL:case WM_MOUSEWHEEL:
        case WM_KEYDOWN:case WM_NCMOUSEMOVE:case WM_NCLBUTTONDOWN:case WM_LBUTTONUP:case WM_ENABLE:
            {wchar_t name[32]={};GetClassNameW(window,name,32);if(_wcsicmp(name,L"Edit")==0)theme->paintTextScrollbar(window);}break;
        }
        if(message==WM_SETFOCUS||message==WM_KILLFOCUS||message==WM_ENABLE)InvalidateRect(window,nullptr,FALSE);return result;
    }
public:
    bool dark=false;
    COLORREF background=0,surface=0,foreground=0,muted=0,border=0,selection=0,accent=0,accentHover=0,button=0,buttonHover=0,verified=0,unverified=0;
    ~AppTheme(){if(backgroundBrush)DeleteObject(backgroundBrush);if(surfaceBrush)DeleteObject(surfaceBrush);}
    HBRUSH backgroundHandle()const{return backgroundBrush;}
    HBRUSH surfaceHandle()const{return surfaceBrush;}
    void apply(HWND window,bool value){
        dark=value;background=dark?RGB(23,19,22):RGB(251,247,248);surface=dark?RGB(43,28,34):RGB(255,252,253);
        foreground=dark?RGB(245,236,240):RGB(42,26,33);muted=dark?RGB(176,148,159):RGB(126,102,112);border=dark?RGB(128,65,83):RGB(184,126,145);selection=dark?RGB(108,30,49):RGB(248,217,225);
        accent=dark?RGB(180,43,66):RGB(157,29,49);accentHover=dark?RGB(207,57,81):RGB(181,39,61);button=dark?RGB(76,29,43):RGB(251,234,240);buttonHover=dark?RGB(103,34,52):RGB(247,215,225);verified=dark?RGB(106,215,166):RGB(22,112,71);unverified=dark?RGB(245,130,151):RGB(158,28,53);
        if(backgroundBrush)DeleteObject(backgroundBrush);if(surfaceBrush)DeleteObject(surfaceBrush);backgroundBrush=CreateSolidBrush(background);surfaceBrush=CreateSolidBrush(surface);
        const BOOL enabled=dark?TRUE:FALSE;DwmSetWindowAttribute(window,DWMWA_USE_IMMERSIVE_DARK_MODE,&enabled,sizeof(enabled));
        for(HWND child=GetWindow(window,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)){
            wchar_t name[32]={};GetClassNameW(child,name,32);
            const bool isButton=_wcsicmp(name,L"Button")==0,isCombo=_wcsicmp(name,L"ComboBox")==0;
            if(isButton){const auto style=GetWindowLongW(child,GWL_STYLE);SetWindowLongW(child,GWL_STYLE,(style&~BS_TYPEMASK)|BS_OWNERDRAW);}
            if(isButton||isCombo){SetWindowSubclass(child,controlSkin,0x4754,reinterpret_cast<DWORD_PTR>(this));SetWindowTheme(child,L"",L"");}
            if(_wcsicmp(name,L"Edit")==0){SetWindowTheme(child,L"",L"");if(GetWindowLongW(child,GWL_STYLE)&ES_READONLY)SetWindowSubclass(child,controlSkin,0x4754,reinterpret_cast<DWORD_PTR>(this));}
        }
        RedrawWindow(window,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_FRAME);
    }
    LRESULT color(HDC dc,bool input)const{SetTextColor(dc,foreground);SetBkColor(dc,input?surface:background);SetBkMode(dc,OPAQUE);return reinterpret_cast<LRESULT>(input?surfaceBrush:backgroundBrush);}
    bool draw(const DRAWITEMSTRUCT& item)const{
        if(item.CtlType!=ODT_BUTTON&&item.CtlType!=ODT_COMBOBOX)return false;
        const bool disabled=(item.itemState&ODS_DISABLED)!=0,pressed=(item.itemState&ODS_SELECTED)!=0,hover=GetPropW(item.hwndItem,hoverProperty)!=nullptr;const int dpi=static_cast<int>(GetDpiForWindow(item.hwndItem));
        SetBkMode(item.hDC,TRANSPARENT);SelectObject(item.hDC,reinterpret_cast<HFONT>(SendMessageW(item.hwndItem,WM_GETFONT,0,0)));std::wstring caption;auto box=item.rcItem;
        if(item.CtlType==ODT_COMBOBOX){
            const auto fill=CreateSolidBrush(pressed?selection:surface);FillRect(item.hDC,&box,fill);DeleteObject(fill);SetTextColor(item.hDC,disabled?muted:foreground);
            if(item.itemID!=static_cast<UINT>(-1)){const auto length=SendMessageW(item.hwndItem,CB_GETLBTEXTLEN,item.itemID,0);if(length!=CB_ERR){caption.resize(static_cast<size_t>(length)+1);SendMessageW(item.hwndItem,CB_GETLBTEXT,item.itemID,reinterpret_cast<LPARAM>(caption.data()));caption.resize(static_cast<size_t>(length));}}
            box.left+=MulDiv(8,dpi,96);box.right-=MulDiv((item.itemState&ODS_COMBOBOXEDIT)?16:4,dpi,96);DrawTextW(item.hDC,caption.c_str(),-1,&box,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
        }else{
            FillRect(item.hDC,&box,backgroundBrush);const bool primary=item.CtlID==500||item.CtlID==502||item.CtlID==610||item.CtlID==200;
            const auto face=disabled?surface:pressed?(dark?RGB(119,30,49):RGB(132,25,45)):primary?(hover?accentHover:accent):(hover?buttonHover:button);
            {
                Gdiplus::Graphics graphics(item.hDC);graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);const float penWidth=static_cast<float>((std::max)(1,MulDiv((item.itemState&ODS_FOCUS)?2:1,dpi,96))),inset=penWidth/2;
                Gdiplus::GraphicsPath shape;rounded(shape,Gdiplus::RectF(static_cast<float>(box.left)+inset,static_cast<float>(box.top)+inset,static_cast<float>(box.right-box.left)-penWidth,static_cast<float>(box.bottom-box.top)-penWidth),static_cast<float>(MulDiv(8,dpi,96)));
                Gdiplus::SolidBrush fill(tint(face));Gdiplus::Pen outline(tint((item.itemState&ODS_FOCUS)||hover?accentHover:border),penWidth);graphics.FillPath(&fill,&shape);graphics.DrawPath(&outline,&shape);
            }
            const auto length=GetWindowTextLengthW(item.hwndItem);caption.resize(static_cast<size_t>(length)+1);GetWindowTextW(item.hwndItem,caption.data(),length+1);SetTextColor(item.hDC,disabled?muted:primary||pressed?RGB(255,247,249):foreground);
            box.left+=MulDiv(8,dpi,96);box.right-=MulDiv(8,dpi,96);if(pressed)OffsetRect(&box,0,1);DrawTextW(item.hDC,caption.c_str(),-1,&box,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
        }
        return true;
    }
};
