#pragma once
#include <QByteArray>
#include <QVariantMap>
struct LegacySettings {bool valid=false;QVariantMap options,gameOptions;QByteArray native;};
LegacySettings readLegacySettings(const QString& path,const QVariantMap& defaults);
