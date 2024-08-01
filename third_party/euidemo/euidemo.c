#include <cosmo.h>
#include <stdio.h>
#include <stdlib.h>
#include <windowsesque.h>

#include "libc/nt/enum/bitblt.h"
#include "libc/nt/enum/color.h"
#include "libc/nt/enum/cs.h"
#include "libc/nt/enum/cw.h"
#include "libc/nt/enum/idc.h"
#include "libc/nt/enum/sw.h"
#include "libc/nt/enum/wm.h"
#include "libc/nt/enum/ws.h"
#include "libc/nt/messagebox.h"
#include "libc/vga/vga.internal.h"

#if __has_include("third_party/euidemo/lib/eui.h")
#include "third_party/euidemo/lib/eui.h"
#include "third_party/euidemo/lib/examples.h"
#else
#include <eui.h>
#include <examples.h>
#endif

// I have not added bothered to get xlib working with the monorepo
#define SupportXLIB __has_include(<X11/Xlib.h>) && __has_include(<X11/Xutil.h>)
#if SupportXLIB
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

// For some reason this gives a black screen if not in the monorepo(also no tty-graph.c but that is easily fixable)
#define SupportMetal defined(__x86_64__) && __has_include("libc/vga/tty-graph.c")
#if SupportMetal
__static_yoink("vga_console");
#include "libc/vga/tty-graph.c"
#endif

// I have a better idea for this toggle but just (un)commenting this works for now
#define SupportUEFI
#ifdef SupportUEFI
__static_yoink("EfiMain");
#endif


typedef struct {
  DWORD biSize;
  LONG  biWidth;
  LONG  biHeight;
  WORD  biPlanes;
  WORD  biBitCount;
  DWORD biCompression;
  DWORD biSizeImage;
  LONG  biXPelsPerMeter;
  LONG  biYPelsPerMeter;
  DWORD biClrUsed;
  DWORD biClrImportant;
} BITMAPINFOHEADER;

typedef struct {
  BYTE rgbBlue;
  BYTE rgbGreen;
  BYTE rgbRed;
  BYTE rgbReserved;
} RGBQUAD;

typedef struct {
  BITMAPINFOHEADER bmiHeader;
  RGBQUAD          bmiColors[1];
} BITMAPINFO;

#define BI_RGB (LONG)0
#define DIB_RGB_COLORS 0

#define MAKEINTRESOURCEW(i) ((LPWSTR)((ULONG_PTR)((WORD)(i))))
#define IDI_APPLICATION MAKEINTRESOURCEW(32512)

extern BOOL AdjustWindowRect(struct NtRect *, DWORD, BOOL);
extern HBITMAP CreateDIBSection(HDC, const BITMAPINFO *, UINT, VOID **, HANDLE, DWORD);
extern HDC GetDC(HWND);
extern BOOL InvalidateRect(HWND, CONST struct NtRect *, BOOL);
extern int ReleaseDC(HWND, HDC);
extern BOOL UpdateWindow(HWND);

typedef void *DPI_AWARENESS_CONTEXT;
#define DPI_AWARENESS_CONTEXT_SYSTEM_AWARE ((DPI_AWARENESS_CONTEXT)-2)

BOOL (WINAPI *pSetProcessDpiAwarenessContext)(DPI_AWARENESS_CONTEXT);

BOOL SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT value) {
  if (!pSetProcessDpiAwarenessContext) {
    pSetProcessDpiAwarenessContext = GetProcAddress(
      LoadLibrary(u"user32.dll"),
      "SetProcessDpiAwarenessContext"
    );
  }
  if (!pSetProcessDpiAwarenessContext) {
    return FALSE;
  }

  return pSetProcessDpiAwarenessContext(value);
}


// My copy of QEMU only has one resolution in the boot manager and the runtime list that cosmo has for bios doesn't appear
#ifdef SupportUEFI
const int width = 1280;
const int height = 800;
#else
const int width = 320;
const int height = 200;
#endif

uint8_t  *framebuf8  = NULL;
uint32_t *framebuf32 = NULL;

HDC hDC = INVALID_HANDLE_VALUE;
HDC cDC = INVALID_HANDLE_VALUE;
HMODULE dib = INVALID_HANDLE_VALUE;
HMODULE oldBmp = INVALID_HANDLE_VALUE;

struct Tty tty;

uint32_t colourmap[256] = {
  0xFF000000,0xFF0000AA,0xFF00AA00,0xFF00AAAA,0xFFAA0000,0xFFAA00AA,0xFFAA5500,0xFFAAAAAA,
  0xFF555555,0xFF5555FF,0xFF55FF55,0xFF55FFFF,0xFFFF5555,0xFFFF55FF,0xFFFFFF55,0xFFFFFFFF,
  
  0xFF000000,0xFF141414,0xFF202020,0xFF2C2C2C,0xFF383838,0xFF454545,0xFF515151,0xFF616161,
  0xFF717171,0xFF828282,0xFF929292,0xFFA2A2A2,0xFFB6B6B6,0xFFCBCBCB,0xFFE3E3E3,0xFFFFFFFF,
  
  0xFF0000FF,0xFF4100FF,0xFF7D00FF,0xFFBE00FF,0xFFFF00FF,0xFFFF00BE,0xFFFF007D,0xFFFF0041,
  0xFFFF0000,0xFFFF4100,0xFFFF7D00,0xFFFFBE00,0xFFFFFF00,0xFFBEFF00,0xFF7DFF00,0xFF41FF00,
  
  0xFF00FF00,0xFF00FF41,0xFF00FF7D,0xFF00FFBE,0xFF00FFFF,0xFF00BEFF,0xFF007DFF,0xFF0041FF,
  0xFF7D7DFF,0xFF9E7DFF,0xFFBE7DFF,0xFFDF7DFF,0xFFFF7DFF,0xFFFF7DDF,0xFFFF7DBE,0xFFFF7D9E,
  
  0xFFFF7D7D,0xFFFF9E7D,0xFFFFBE7D,0xFFFFDF7D,0xFFFFFF7D,0xFFDFFF7D,0xFFBEFF7D,0xFF9EFF7D,
  0xFF7DFF7D,0xFF7DFF9E,0xFF7DFFBE,0xFF7DFFDF,0xFF7DFFFF,0xFF7DDFFF,0xFF7DBEFF,0xFF7D9EFF,
  
  0xFFB6B6FF,0xFFC7B6FF,0xFFDBB6FF,0xFFEBB6FF,0xFFFFB6FF,0xFFFFB6EB,0xFFFFB6DB,0xFFFFB6C7,
  0xFFFFB6B6,0xFFFFC7B6,0xFFFFDBB6,0xFFFFEBB6,0xFFFFFFB6,0xFFEBFFB6,0xFFDBFFB6,0xFFC7FFB6,
  
  0xFFB6FFB6,0xFFB6FFC7,0xFFB6FFDB,0xFFB6FFEB,0xFFB6FFFF,0xFFB6EBFF,0xFFB6DBFF,0xFFB6C7FF,
  0xFF000071,0xFF1C0071,0xFF380071,0xFF550071,0xFF710071,0xFF710055,0xFF710038,0xFF71001C,
  
  0xFF710000,0xFF711C00,0xFF713800,0xFF715500,0xFF717100,0xFF557100,0xFF387100,0xFF1C7100,
  0xFF007100,0xFF00711C,0xFF007138,0xFF007155,0xFF007171,0xFF005571,0xFF003871,0xFF001C71,
  
  0xFF383871,0xFF453871,0xFF553871,0xFF613871,0xFF713871,0xFF713861,0xFF713855,0xFF713845,
  0xFF713838,0xFF714538,0xFF715538,0xFF716138,0xFF717138,0xFF617138,0xFF557138,0xFF457138,
  
  0xFF387138,0xFF387145,0xFF387155,0xFF387161,0xFF387171,0xFF386171,0xFF385571,0xFF384571,
  0xFF515171,0xFF595171,0xFF615171,0xFF695171,0xFF715171,0xFF715169,0xFF715161,0xFF715159,
  
  0xFF715151,0xFF715951,0xFF716151,0xFF716951,0xFF717151,0xFF697151,0xFF617151,0xFF597151,
  0xFF517151,0xFF517159,0xFF517161,0xFF517169,0xFF517171,0xFF516971,0xFF516171,0xFF515971,
  
  0xFF000041,0xFF100041,0xFF200041,0xFF300041,0xFF410041,0xFF410030,0xFF410020,0xFF410010,
  0xFF410000,0xFF411000,0xFF412000,0xFF413000,0xFF414100,0xFF304100,0xFF204100,0xFF104100,
  
  0xFF004100,0xFF004110,0xFF004120,0xFF004130,0xFF004141,0xFF003041,0xFF002041,0xFF001041,
  0xFF202041,0xFF282041,0xFF302041,0xFF382041,0xFF412041,0xFF412038,0xFF412030,0xFF412028,
  
  0xFF412020,0xFF412820,0xFF413020,0xFF413820,0xFF414120,0xFF384120,0xFF304120,0xFF284120,
  0xFF204120,0xFF204128,0xFF204130,0xFF204138,0xFF204141,0xFF203841,0xFF203041,0xFF202841,
  
  0xFF2C2C41,0xFF302C41,0xFF342C41,0xFF3C2C41,0xFF412C41,0xFF412C3C,0xFF412C34,0xFF412C30,
  0xFF412C2C,0xFF41302C,0xFF41342C,0xFF413C2C,0xFF41412C,0xFF3C412C,0xFF34412C,0xFF30412C,
  
  0xFF2C412C,0xFF2C4130,0xFF2C4134,0xFF2C413C,0xFF2C4141,0xFF2C3C41,0xFF2C3441,0xFF2C3041,
  0xFF000000,0xFF000000,0xFF000000,0xFF000000,0xFF000000,0xFF000000,0xFF000000,0xFF000000
};

void destroy_framebuffers(HWND hWnd) {
  eui_quit();
  // This crashes under qemu uefi for some reason
#ifndef SupportUEFI
    free(framebuf8);
#endif

  if (IsWindows()) {
    SelectObject(cDC, oldBmp);
    DeleteObject(dib);
    DeleteDC(cDC);
    ReleaseDC(hWnd, hDC);
  // This also fails under uefi but also if allocation fails then _vga_reinit can directly map to system memory
  } else if (!IsMetal()) {
    free(framebuf32);
  }
}

bool create_framebuffers(HWND hWnd) {
  framebuf8  = malloc((width*height) *sizeof *framebuf8);
  if (!framebuf8) {
    return false;
  }

  if (IsMetal()) {
    _vga_reinit(&tty, 0, 0, 0);
    if (_TtyCanvasType(&tty) != PC_VIDEO_BGRX8888 || tty.xp != width || tty.yp != height) {
      destroy_framebuffers(hWnd);
      printf("Not in desired outptut mode, reboot and set to %ix%ix32.\n", width, height);
      printf("Current mode is %ix%i", tty.xp, tty.yp);
      switch (_TtyCanvasType(&tty)) {
        case PC_VIDEO_BGRX8888:
          printf("x32");
          break;
        case PC_VIDEO_TEXT:
          printf(", text only");
          break;
        default:
          printf(", unknown colour mode");
          break;

      }
      puts(".");
      return false;
    }
    framebuf32 = (uint32_t *)tty.canvas;
  }
  else if (IsWindows()) {
    hDC = GetDC(hWnd);
    if (!hDC) {
      destroy_framebuffers(hWnd);
      return false;
    }

    cDC = CreateCompatibleDC(hDC);
    if (!cDC) {
      destroy_framebuffers(hWnd);
      return false;
    }

    BITMAPINFO info;
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    info.bmiHeader.biSizeImage = 0;

    dib = CreateDIBSection(hDC, &info, DIB_RGB_COLORS, (VOID **)&framebuf32, 0, 0);
    if (!dib) {
      destroy_framebuffers(hWnd);
      return false;
    }

    oldBmp = SelectObject(cDC, dib);
    if (!oldBmp) {
      destroy_framebuffers(hWnd);
      return false;
    }
  } else {
    framebuf32 = malloc((width*height) *sizeof *framebuf32);
    if (!framebuf32) {
      destroy_framebuffers(hWnd);
      return false;
    }
  }

  eui_init(width, height, 8, width, framebuf8);
  return true;
}

void show_gui(void) {
  if (eui_context_begin()) {
    example_order();
    eui_context_end();
  }

  for (size_t i = 0; i < (width*height); i++) {
    if (framebuf8[i] >= 64) {
      framebuf32[i] = 0xFFFFFFFF;
    } else {
      framebuf32[i] = colourmap[framebuf8[i]];
    }
  }
}


#if SupportMetal
void show_gui_vga(void) {
  if (!create_framebuffers(NULL)) {
    return;
  }
  while(true) {
    show_gui();
    DIRTY(&tty, 0, 0, tty.yp, tty.xp);
    tty.update(&tty);
  }
}
#endif


#if SupportXLIB
int show_gui_xlib(void) {
  Display *dpy;
  XVisualInfo vinfo;
  int depth;
  XVisualInfo *visual_list;
  XVisualInfo visual_template;
  int nxvisuals;
  int i;
  XSetWindowAttributes attrs;
  Window parent;
  Visual *visual;

  Window win;
  XImage *ximage;
  XEvent event;

  dpy = XOpenDisplay(NULL);

  nxvisuals = 0;
  visual_template.screen = DefaultScreen(dpy);
  visual_list = XGetVisualInfo (dpy, VisualScreenMask, &visual_template, &nxvisuals);

  if (!XMatchVisualInfo(dpy, XDefaultScreen(dpy), 24, TrueColor, &vinfo)) {
    fprintf(stderr, "no such visual\n");
    return 1;
  }

  parent = XDefaultRootWindow(dpy);

  XSync(dpy, True);

  printf("creating RGBA child\n");

  visual = vinfo.visual;
  depth = vinfo.depth;

  attrs.colormap = XCreateColormap(dpy, XDefaultRootWindow(dpy), visual, AllocNone);
  attrs.background_pixel = 0;
  attrs.border_pixel = 0;

  if (!create_framebuffers(NULL)) {
    return -1;
  }

  win = XCreateWindow(dpy, parent, 100, 100, width, height, 0, depth, InputOutput,
            visual, CWBackPixel | CWColormap | CWBorderPixel, &attrs);

  ximage = XCreateImage(dpy, vinfo.visual, depth, ZPixmap, 0, (char *)framebuf32, width, height, 8, width*4);

  if (ximage == 0) {
    printf("ximage is null!\n");
  }

  XSync(dpy, True);

  XSelectInput(dpy, win, ExposureMask | KeyPressMask);

  XGCValues gcv;
  unsigned long gcm;
  GC NormalGC;

  gcm = GCGraphicsExposures;
  gcv.graphics_exposures = 0;
  NormalGC = XCreateGC(dpy, parent, gcm, &gcv);

  XMapWindow(dpy, win);

  while(!XNextEvent(dpy, &event)) {
    show_gui();

    switch(event.type) {
      case Expose:
        printf("I have been exposed!\n");
        XPutImage(dpy, win, NormalGC, ximage, 0, 0, 0, 0, width, height);
        break;
      }
  }

  destroy_framebuffers(NULL);

  printf("No error\n");

  return 0;
}
#endif


LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch(msg) {
    case kNtWmPaint:
      show_gui();
      BitBlt(hDC, 0, 0, width, height, cDC, 0, 0, kNtSrccopy);
      return 0;

    case kNtWmDestroy:
      PostQuitMessage(0);
      return 0;
  }

  return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI show_gui_gdi(void) {
  struct NtStartupInfo info;
  GetStartupInfo(&info);

  HINSTANCE hInstance = (HINSTANCE)GetModuleHandle(NULL);
  int nCmdShow = info.dwFlags & STARTF_USESHOWWINDOW ? info.wShowWindow : kNtSwNormal;

  int result = 1;

  struct NtWndClass wnd     = {0};
  wnd.style         = kNtCsVredraw | kNtCsHredraw;
  wnd.lpfnWndProc   = (NtWndProc)WndProc;
  wnd.hInstance     = hInstance;
  wnd.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
  wnd.hCursor       = LoadCursor(NULL, kNtIdcArrow);
  wnd.hbrBackground = (HBRUSH)(kNtColorWindow + 1);
  wnd.lpszClassName = u"Test window";
  if (!wnd.hIcon || !wnd.hCursor || !wnd.lpszClassName) {
    goto end;
  }

  ATOM class = RegisterClass(&wnd);
  if (!class) {
    goto end;
  }
  void *rClass = MAKEINTRESOURCEW(class);

  DWORD windowFlags = kNtWsOverlapped | kNtWsCaption | kNtWsSysmenu | kNtWsMaximizebox;
  struct NtRect rect = {0, 0, width, height};
  AdjustWindowRect(&rect, windowFlags, false);

  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);

  HWND hWnd = CreateWindowEx(0, rClass, wnd.lpszClassName, windowFlags,
                             kNtCwUsedefault, kNtCwUsedefault,
                             rect.right - rect.left, rect.bottom - rect.top,
                             NULL, NULL, hInstance, NULL);
  if(!hWnd) {
    goto end;
  }
  
  if (!create_framebuffers(hWnd)) {
    goto cleanup_class;
  }

  ShowWindow(hWnd, nCmdShow);
  if (!UpdateWindow(hWnd)) {
    goto free_framebuffers;
  }

  struct NtMsg msg;
  while (GetMessage(&msg, NULL, 0, 0)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
  }
  result = msg.wParam;

free_framebuffers:
  destroy_framebuffers(hWnd);
cleanup_class:
  // UnregisterClassW(rClass, hInstance);
end:
  return result;
}


int main(void) {
  if (IsMetal()) {
#if SupportMetal
    show_gui_vga();
#else
    puts("Metal is not supported currently");
#endif
    // loop so machine doesn't reset
    for (;;) ;
  } else if (IsWindows()) {
    return show_gui_gdi();
  } else {
#if SupportXLIB
    return show_gui_xlib();
#else
    puts("Current platform is not supported currently");
#endif
  }
}
