#include "lifecycle.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

namespace lifecycle {
namespace {
bool validPath(const QString& path) {
    if (!QDir::isAbsolutePath(path) || path.size()>32760) return false;
    for (const auto c:path) if (c.unicode()<32 || c=='"') return false;
    return true;
}
QString autostartPath() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)+"/autostart/io.framoryx.Framoryx.desktop";
}
}
QString launchPath() {
    // An AppImage mount is temporary; register its persistent outer file instead.
    const QString image=qEnvironmentVariable("APPIMAGE");
    return !image.isEmpty() && validPath(image) && QFileInfo(image).isExecutable()
        ? QFileInfo(image).absoluteFilePath() : QCoreApplication::applicationFilePath();
}
QString windowsCommand(const QString& executable) {
    if (!validPath(executable)) return {};
    return '"'+QDir::toNativeSeparators(executable)+"\" --startup --background";
}
QByteArray desktopEntry(const QString& executable,bool startup) {
    if (!validPath(executable)) return {};
    QString escaped;
    // Exec quoting, then desktop-entry string escaping. Percent is a field code.
    for (const auto c:executable) {
        if (c=='\\') escaped+="\\\\\\\\";
        else if (c=='"' || c=='`' || c=='$') {escaped+="\\\\";escaped+=c;}
        else if (c=='%') escaped+="%%";
        else escaped+=c;
    }
    return ("[Desktop Entry]\nType=Application\nName=Framoryx\nComment=Your game, your way\nExec=\""+escaped+
        "\""+(startup?" --startup --background":"")+"\nIcon=io.framoryx.Framoryx\nTerminal=false\nX-GNOME-Autostart-enabled=true\nX-Framoryx-Managed=true\n").toUtf8();
}
bool startupEnabled() {
#ifdef Q_OS_WIN
    QSettings run("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",QSettings::NativeFormat);
    return run.value("Framoryx").toString()==windowsCommand(launchPath());
#else
    QFile file(autostartPath());
    return file.open(QIODevice::ReadOnly) && file.size()<65536 && file.readAll()==desktopEntry(launchPath());
#endif
}
bool setStartup(bool enabled,QString* error) {
    if (!validPath(launchPath())) {*error="This application path cannot be registered for startup.";return false;}
#ifdef Q_OS_WIN
    QSettings run("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",QSettings::NativeFormat);
    if (enabled) run.setValue("Framoryx",windowsCommand(launchPath()));
    else run.remove("Framoryx");
    run.sync();
    if (run.status()!=QSettings::NoError) {*error="Windows could not update your sign-in preference.";return false;}
#else
    const auto path=autostartPath();
    if (enabled) {
        if (!QDir().mkpath(QFileInfo(path).absolutePath())) {*error="The autostart folder could not be created.";return false;}
        QSaveFile file(path);const auto bytes=desktopEntry(launchPath());
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size() || !file.commit()) {*error="The sign-in entry could not be saved.";return false;}
    } else if (QFileInfo::exists(path) && !QFile::remove(path)) {*error="The sign-in entry could not be removed.";return false;}
#endif
    if (startupEnabled()!=enabled) {*error="Your desktop did not retain the startup preference.";return false;}
    return true;
}
bool selfTest(QString* error) {
    const auto entry=desktopEntry("/opt/A $B/100% `test`/Framoryx");
    if (!entry.contains("100%%") || !entry.contains("\\\\$") || !entry.contains("\\\\`") || !entry.contains("--startup --background") || !desktopEntry("relative/path").isEmpty() || !desktopEntry("/tmp/bad\nentry").isEmpty() || !windowsCommand("relative.exe").isEmpty()) {
        *error="Startup command escaping or path validation failed";return false;
    }
    return true;
}
}
