#pragma once
#include <QByteArray>
#include <QVariantMap>
QVariantMap verifiedNoiseCache(const QString& directory);
QVariantMap installNoiseArchive(const QString& directory,const QByteArray& archive,const QString& digest,const QString& version);
