#pragma once
#include <QString>

namespace lifecycle {
QString launchPath();
QString windowsCommand(const QString& executable);
QByteArray desktopEntry(const QString& executable);
bool startupEnabled();
bool setStartup(bool enabled, QString* error);
bool selfTest(QString* error);
}
