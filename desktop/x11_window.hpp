#pragma once
#include <QByteArray>
#include <QString>
namespace x11 {
struct WindowState {quint32 id=0,pid=0;QString start;QByteArray windowClass;bool fullscreen=false,valid=false;};
WindowState inspect(quint32 id);
bool fullscreen(const WindowState& original,bool enabled);
bool compositing();
quint32 foreground();
}
