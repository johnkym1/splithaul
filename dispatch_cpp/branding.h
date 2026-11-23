#pragma once
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <memory>
#include <stdexcept>

class Branding {
    ULONG_PTR token=0;
    IStream* stream=nullptr;
    std::unique_ptr<Gdiplus::Bitmap> original;
    HICON iconSmall=nullptr,iconLarge=nullptr;
    HICON icon(int size) {
        Gdiplus::Bitmap image(size,size,PixelFormat32bppARGB);Gdiplus::Graphics graphics(&image);
        graphics.Clear(Gdiplus::Color(0,0,0,0));graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        draw(graphics,Gdiplus::Rect(0,0,size,size));HICON value=nullptr;image.GetHICON(&value);return value;
    }
    void draw(Gdiplus::Graphics& graphics,const Gdiplus::Rect& box) {
        const float imageWidth=static_cast<float>(original->GetWidth()),imageHeight=static_cast<float>(original->GetHeight());
        const float boxWidth=static_cast<float>(box.Width),boxHeight=static_cast<float>(box.Height);
        const float ratio=(std::min)(boxWidth/imageWidth,boxHeight/imageHeight);
        const float w=imageWidth*ratio,h=imageHeight*ratio;
        graphics.DrawImage(original.get(),Gdiplus::RectF(static_cast<float>(box.X)+(boxWidth-w)/2,static_cast<float>(box.Y)+(boxHeight-h)/2,w,h));
    }
public:
    explicit Branding(HINSTANCE instance) {
        Gdiplus::GdiplusStartupInput input;if(Gdiplus::GdiplusStartup(&token,&input,nullptr)!=Gdiplus::Ok)throw std::runtime_error("Could not initialize logo rendering.");
        auto resource=FindResourceW(instance,MAKEINTRESOURCEW(101),RT_RCDATA);const auto size=SizeofResource(instance,resource);const auto data=LockResource(LoadResource(instance,resource));
        if(!resource||!size||!data)throw std::runtime_error("The app logo resource is missing.");
        HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE,size);if(!memory)throw std::runtime_error("Could not load the logo.");
        auto target=GlobalLock(memory);if(!target){GlobalFree(memory);throw std::runtime_error("Could not load the logo.");}memcpy(target,data,size);GlobalUnlock(memory);
        if(FAILED(CreateStreamOnHGlobal(memory,TRUE,&stream))){GlobalFree(memory);throw std::runtime_error("Could not open the logo resource.");}
        original.reset(Gdiplus::Bitmap::FromStream(stream));if(!original||original->GetLastStatus()!=Gdiplus::Ok)throw std::runtime_error("The app logo could not be decoded.");
        iconSmall=icon(32);iconLarge=icon(64);
    }
    ~Branding(){if(iconSmall)DestroyIcon(iconSmall);if(iconLarge)DestroyIcon(iconLarge);original.reset();if(stream)stream->Release();if(token)Gdiplus::GdiplusShutdown(token);}
    HICON smallIcon() const{return iconSmall;}
    HICON largeIcon() const{return iconLarge;}
    void paint(const DRAWITEMSTRUCT& item,HBRUSH background) {
        FillRect(item.hDC,&item.rcItem,background);Gdiplus::Graphics graphics(item.hDC);graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);draw(graphics,Gdiplus::Rect(item.rcItem.left,item.rcItem.top,item.rcItem.right-item.rcItem.left,item.rcItem.bottom-item.rcItem.top));
    }
};
