#include<QtCore>
#include<QImage>
#include <Windows.h>

void GetDesktopCaptureInfo(quint16* width, quint16* height, QImage::Format* format, bool* rowAlign)
{
    HDC hDC = GetDC(NULL);
    BITMAP bAllDesktops;
    HGDIOBJ hTempBitmap = GetCurrentObject(hDC, OBJ_BITMAP);
    ZeroMemory(&bAllDesktops, sizeof(BITMAP));
    GetObjectW(hTempBitmap, sizeof(BITMAP), &bAllDesktops);
    *width = static_cast<quint64>(bAllDesktops.bmWidth);
    *height = static_cast<quint64>(bAllDesktops.bmHeight);
    *format = QImage::Format_RGB888;
    *rowAlign = true;
    DeleteObject(hTempBitmap);
    ReleaseDC(NULL, hDC);
}

void CaptureDesktop(quint8* bytes)
{
    BITMAPINFOHEADER biHeader;
    BITMAPINFO bInfo;
    HGDIOBJ hTempBitmap;
    HBITMAP hBitmap;
    BITMAP bAllDesktops;
    HDC hDC, hMemDC;
    LONG lWidth, lHeight;
    BYTE *bBits = NULL;
    DWORD cbBits;
    INT x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    INT y = GetSystemMetrics(SM_YVIRTUALSCREEN);

    ZeroMemory(&biHeader, sizeof(BITMAPINFOHEADER));
    ZeroMemory(&bInfo, sizeof(BITMAPINFO));
    ZeroMemory(&bAllDesktops, sizeof(BITMAP));

    hDC = GetDC(NULL);
    hTempBitmap = GetCurrentObject(hDC, OBJ_BITMAP);
    GetObjectW(hTempBitmap, sizeof(BITMAP), &bAllDesktops);

    lWidth = bAllDesktops.bmWidth;
    lHeight = bAllDesktops.bmHeight;

    DeleteObject(hTempBitmap);

    biHeader.biSize = sizeof(BITMAPINFOHEADER);
    biHeader.biBitCount = 24;
    biHeader.biCompression = BI_RGB;
    biHeader.biPlanes = 1;
    biHeader.biWidth = lWidth;
    biHeader.biHeight = lHeight;

    bInfo.bmiHeader = biHeader;

    cbBits = (((24 * lWidth + 31)&~31) / 8) * lHeight;

    hMemDC = CreateCompatibleDC(hDC);
    hBitmap = CreateDIBSection(hDC, &bInfo, DIB_RGB_COLORS, (VOID **)&bBits, NULL, 0);
    SelectObject(hMemDC, hBitmap);
    BitBlt(hMemDC, 0, 0, lWidth, lHeight, hDC, x, y, SRCCOPY);

    memcpy(bytes, bBits, cbBits);


    DeleteDC(hMemDC);
    ReleaseDC(NULL, hDC);
    DeleteObject(hBitmap);
}

void StretchCaptureDesktop(quint8* dst, quint64 dstSize, quint16 width, quint16 height)
{
    BITMAPINFOHEADER biHeader;
    BITMAPINFO bInfo;
    HGDIOBJ hTempBitmap;
    HBITMAP hBitmap;
    BITMAP bAllDesktops;
    HDC hDC, hMemDC;
    BYTE *bBits = NULL;
    INT x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    INT y = GetSystemMetrics(SM_YVIRTUALSCREEN);

    ZeroMemory(&biHeader, sizeof(BITMAPINFOHEADER));
    ZeroMemory(&bInfo, sizeof(BITMAPINFO));
    ZeroMemory(&bAllDesktops, sizeof(BITMAP));

    hDC = GetDC(NULL);
    hTempBitmap = GetCurrentObject(hDC, OBJ_BITMAP);
    GetObjectW(hTempBitmap, sizeof(BITMAP), &bAllDesktops);

    DeleteObject(hTempBitmap);

    biHeader.biSize = sizeof(BITMAPINFOHEADER);
    biHeader.biBitCount = 24;
    biHeader.biCompression = BI_RGB;
    biHeader.biPlanes = 1;
    biHeader.biWidth = width;
    biHeader.biHeight = height;

    bInfo.bmiHeader = biHeader;

    hMemDC = CreateCompatibleDC(hDC);
    hBitmap = CreateDIBSection(hDC, &bInfo, DIB_RGB_COLORS, (VOID **)&bBits, NULL, 0);
    SelectObject(hMemDC, hBitmap);
    SetStretchBltMode(hMemDC, HALFTONE);
    StretchBlt(hMemDC, 0, 0, width, height, hDC, x, y, bAllDesktops.bmWidth, bAllDesktops.bmHeight, SRCCOPY);

    memcpy(dst, bBits, dstSize);


    DeleteDC(hMemDC);
    ReleaseDC(NULL, hDC);
    DeleteObject(hBitmap);
}


//https://stackoverflow.com/questions/3291167/how-can-i-take-a-screenshot-in-a-windows-application
