#include <windows.h>
#include "dciddi.h"

#define DOOM_IMPLEMENT_PRINT 1
#define DOOM_IMPLEMENT_MALLOC 1
#define DOOM_IMPLEMENT_FILE_IO 1
#define DOOM_IMPLEMENT_GETTIME 1
#define DOOM_IMPLEMENT_EXIT 1
#define DOOM_IMPLEMENT_GETENV 1
#define WIN32
#define DOOM_IMPLEMENTATION
#include "PureDOOM.h"

HMODULE dciManagerLibHandle = NULL;

HDC dciDC = NULL;

int toPaint;

HDC (WINAPI *fnDCIOpenProvider)(void) = NULL;
void (WINAPI *fnDCICloseProvider)(HDC hdc) = NULL;
int (WINAPI *fnDCICreatePrimary)(HDC hdc, LPDCISURFACEINFO *lplpSurface) = NULL;
void (WINAPI *fnDCIDestroy)(LPDCISURFACEINFO pdci) = NULL;

int (WINAPI *fnDCIBeginAccess)(LPDCISURFACEINFO pdci, int x, int y, int dx, int dy) = NULL;
void (WINAPI *fnDCIEndAccess)(LPDCISURFACEINFO pdci) = NULL;

int exited = 0;

LRESULT CALLBACK wndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
        switch (uMsg)
        {
                case WM_ERASEBKGND:
                        toPaint++;
                        return 1;
                case WM_DESTROY:
                        exited = 1;
                        PostQuitMessage(0);
                        break;
                case WM_PAINT:
                        return 0;
                case WM_SETCURSOR:
                        SetCursor(NULL);
                        return 0;
        }
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

HWND createWindowForPalette()
{
        WNDCLASSA wclass;
        HWND window;
        wclass.style = 0;
        wclass.lpfnWndProc = wndProc;
        wclass.cbClsExtra = 0;
        wclass.cbWndExtra = 0;
        wclass.hInstance = GetModuleHandle(NULL);
        wclass.hIcon = NULL;
        wclass.hCursor = LoadCursor(NULL, IDC_ARROW);
        wclass.lpszMenuName = NULL;
        wclass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        wclass.lpszClassName = "DCI";
        RegisterClassA(&wclass);
        window = CreateWindowExA(0, "DCI", "DCI", WS_VISIBLE | WS_POPUP | 8, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), NULL, NULL, wclass.hInstance, NULL);
        return window;
}

int main(int argc, char** argv)
{
        DWORD width, height;
        int bpp = 1;
        LPDCISURFACEINFO info = NULL;
        dciManagerLibHandle = LoadLibraryA("DCIMAN32.DLL");
        if (dciManagerLibHandle)
        {
            fnDCIOpenProvider = (void*)GetProcAddress(dciManagerLibHandle, "DCIOpenProvider");
            fnDCICloseProvider = (void*)GetProcAddress(dciManagerLibHandle, "DCICloseProvider");
            fnDCICreatePrimary = (void*)GetProcAddress(dciManagerLibHandle, "DCICreatePrimary");
            fnDCIDestroy = (void*)GetProcAddress(dciManagerLibHandle, "DCIDestroy");
            fnDCIBeginAccess = (void*)GetProcAddress(dciManagerLibHandle, "DCIBeginAccess");
            fnDCIEndAccess = (void*)GetProcAddress(dciManagerLibHandle, "DCIEndAccess");
        }
        HDC dcidc = fnDCIOpenProvider();
        doom_init(argc, argv, 0);
        if (fnDCICreatePrimary(dcidc,&info) < DCI_OK) {
            printf("Failed to create DCI primary surface!\n");
            return -1;
        }
        if (info->dwBitCount == 32) bpp = 4;
        if (info->dwBitCount == 24) bpp = 3;
        if (info->dwBitCount == 16 || info->dwBitCount == 15) bpp = 2;
        HWND hwnd = createWindowForPalette();
        HDC dispcalt = GetDC(hwnd);
                        width = info->dwWidth;
                        height = info->dwHeight;
        if (bpp == 1)
            SetSystemPaletteUse(GetDC(hwnd), SYSPAL_NOSTATIC);

        if (bpp == 1) {
                doom_update();
                (void)doom_get_framebuffer(0);
                LOGPALETTE* pal = calloc(1, sizeof(LOGPALETTE) + 256 * sizeof(PALETTEENTRY));
                pal->palVersion = 0x300;
                pal->palNumEntries = 256;
                for (int i = 0; i < 256; i++)
                {
                        pal->palPalEntry[i].peRed = screen_palette[i * 3 + 0];
                        pal->palPalEntry[i].peGreen = screen_palette[i * 3 + 1];
                        pal->palPalEntry[i].peBlue = screen_palette[i * 3 + 2];
                        pal->palPalEntry[i].peFlags = PC_NOCOLLAPSE;
                }
                pal->palPalEntry[0].peFlags = 0;
                pal->palPalEntry[255].peFlags = 0;
                HPALETTE createdPal = CreatePalette(pal);
                if (createdPal)
                {
                        HPALETTE prevPal = SelectPalette(dispcalt, createdPal, 0);
                        if (prevPal)
                        {
                                RealizePalette(dispcalt);
                        }
                }
        }
                        ShowCursor(FALSE);
        int bRet;
        MSG msg;
        if (fnDCIBeginAccess(info, 0, 0, info->dwWidth, info->dwHeight) >= DCI_OK) {
            for (int y = 0; y < info->dwHeight; y++)
            {
                unsigned char* ptr = (unsigned char*)info->dwOffSurface;
                memset(&ptr[y * info->lStride], 255, info->dwWidth * bpp);
            }
            fnDCIEndAccess(info);
            ShowCursor(TRUE);
            Sleep(1000);
            fnDCIBeginAccess(info, 0, 0, info->dwWidth, info->dwHeight);
            for (int y = 0; y < info->dwHeight; y++)
            {
                unsigned char* ptr = (unsigned char*)info->dwOffSurface;
                memset(&ptr[y * info->lStride], 0, info->dwWidth * bpp);
            }
            fnDCIEndAccess(info);
            ShowCursor(FALSE);
        } else {
            printf("DCIBeginAccess failed!");
            return -1;
        }
        while ((bRet = GetMessage(&msg, hwnd, 0, 0)) != 0)
        {
                doom_update();
                const unsigned char* fb = doom_get_framebuffer(1);
                if (exited) { exit(-1); }
                if (fnDCIBeginAccess(info, 0, 0, width, height) >= DCI_OK) {
                    unsigned char* ptr = (unsigned char*)info->dwOffSurface;
                    for (int y = 0; y < 200; y++)
                    {
                        memcpy(&ptr[y * info->lStride], &fb[y * 320], 320);
                    }
                    fnDCIEndAccess(info);
                }
        }
        return 0;
}