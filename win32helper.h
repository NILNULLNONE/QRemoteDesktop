#ifndef WIN32HELPER_H
#define WIN32HELPER_H
#include<QtCore>
#include<QImage>

extern void GetDesktopCaptureInfo(quint16* width, quint16* height, QImage::Format* format, bool* rowAlign);
extern void CaptureDesktop(quint8* bytes);

#endif // WIN32HELPER_H
