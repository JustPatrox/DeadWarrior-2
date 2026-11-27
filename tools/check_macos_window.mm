#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#import <Cocoa/Cocoa.h>
#include <cstdio>
#include <cstdlib>

static void geometry(SDL_Window* window,const char* phase) {
    NSWindow* native=(__bridge NSWindow*)SDL_GetPointerProperty(SDL_GetWindowProperties(window),SDL_PROP_WINDOW_COCOA_WINDOW_POINTER,nullptr);
    std::fprintf(stderr,"%s frame=%s view=%s bounds=%s min=%s max=%s aspect=%s screen=%s visible=%d\n",phase,
        NSStringFromRect(native.frame).UTF8String,NSStringFromRect(native.contentView.frame).UTF8String,
        NSStringFromRect(native.contentView.bounds).UTF8String,NSStringFromSize(native.contentMinSize).UTF8String,
        NSStringFromSize(native.contentMaxSize).UTF8String,NSStringFromSize(native.contentAspectRatio).UTF8String,
        NSStringFromRect(native.screen.frame).UTF8String,int(native.visible));
}
int main(int argc,char** argv) {
    const int control=argc>1 ? std::atoi(argv[1]) : 0;
    SDL_SetMainReady();
    if(!SDL_Init(SDL_INIT_VIDEO))return 2;
    auto window=SDL_CreateWindow("Startup control",1280,720,SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY|(control==5 ? 0 : SDL_WINDOW_HIDDEN));
    if(!window)return 3;
    if(control!=4)SDL_SetWindowMinimumSize(window,640,360);
    geometry(window,"created");
    auto renderer=control==3 ? nullptr : SDL_CreateRenderer(window,nullptr);
    if(control!=1&&control!=3)SDL_SetWindowAspectRatio(window,control==6 ? 16.f/9 : 0,control==6 ? 16.f/9 : 0);
    if(renderer&&control!=2) {
        SDL_SetRenderDrawColor(renderer,0,0,0,255);SDL_RenderClear(renderer);SDL_RenderPresent(renderer);
    }
    geometry(window,"before-show");
    if(!SDL_ShowWindow(window))return 4;
    geometry(window,"shown");
    for(int i=0;i<10;++i) {SDL_PumpEvents();SDL_Delay(10);}
    SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();return 0;
}
