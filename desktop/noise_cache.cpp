#include "noise_cache.hpp"
#include "vendor/miniz/miniz.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
namespace {
constexpr auto archive110="811390b6eb6e28dde023c70590c74d26e74ebb2e595bcf4b95af2341db160e99";
constexpr auto plugin110="ba2b420c76cd2f1e7684430c96fef8c533041d5f67b94060f8eeda6af1701f97";
bool hex(const QString& value){return QRegularExpression("^[0-9a-f]{64}$").match(value).hasMatch();}
bool write(const QString& path,const QByteArray& data){QSaveFile file(path);if(!file.open(QIODevice::WriteOnly))return false;file.setPermissions(QFile::ReadOwner|QFile::WriteOwner);return file.write(data)==data.size()&&file.commit();}
}
QVariantMap verifiedNoiseCache(const QString& directory){
    if(QFileInfo(directory).isSymLink())return {};
    const QString path=directory+"/librnnoise_ladspa.so";const QFileInfo info(path);
    if(!info.isFile()||info.isSymLink()||info.size()<64||info.size()>32*1024*1024)return {};
    QFile file(path);if(!file.open(QIODevice::ReadOnly))return {};
    const auto actual=QString::fromLatin1(QCryptographicHash::hash(file.readAll(),QCryptographicHash::Sha256).toHex());
    QVariantMap record;QFile metadata(directory+"/verified.json");
    if(!QFileInfo(metadata).isSymLink()&&metadata.size()<16384&&metadata.open(QIODevice::ReadOnly))record=QJsonDocument::fromJson(metadata.read(16384)).object().toVariantMap();
    if(actual==plugin110){record["version"]="v1.10";record["sha256"]=actual;record["archiveSha256"]=archive110;}
    if(record["sha256"]!=actual||!hex(record["archiveSha256"].toString())||!QRegularExpression("^v[0-9]+\\.[0-9]+(?:\\.[0-9]+)?$").match(record["version"].toString()).hasMatch())return {};
    record["ok"]=true;return record;
}
QVariantMap installNoiseArchive(const QString& directory,const QByteArray& archive,const QString& digest,const QString& version){
    if(!hex(digest)||archive.size()>64*1024*1024||QCryptographicHash::hash(archive,QCryptographicHash::Sha256).toHex()!=digest.toLatin1())return {};
    if(!QRegularExpression("^v[0-9]+\\.[0-9]+(?:\\.[0-9]+)?$").match(version).hasMatch())return {};
    mz_zip_archive zip{};if(!mz_zip_reader_init_mem(&zip,archive.constData(),size_t(archive.size()),0))return {};
    QByteArray binary;
    for(mz_uint i=0;i<mz_zip_reader_get_num_files(&zip);++i){mz_zip_archive_file_stat stat{};if(!mz_zip_reader_file_stat(&zip,i,&stat))continue;const auto name=QString::fromUtf8(stat.m_filename);if(!name.endsWith("/ladspa/librnnoise_ladspa.so")||stat.m_uncomp_size>32*1024*1024||stat.m_uncomp_size<64)continue;binary.resize(qsizetype(stat.m_uncomp_size));if(!mz_zip_reader_extract_to_mem(&zip,i,binary.data(),size_t(binary.size()),0))binary.clear();break;}
    mz_zip_reader_end(&zip);
    if(binary.size()<64||binary.left(4)!=QByteArray("\x7f" "ELF",4)||quint8(binary[4])!=2||quint8(binary[5])!=1||quint8(binary[6])!=1||quint8(binary[16])!=3||quint8(binary[17])!=0||quint8(binary[18])!=62||quint8(binary[19])!=0)return {};
    const auto hash=QString::fromLatin1(QCryptographicHash::hash(binary,QCryptographicHash::Sha256).toHex());if(version=="v1.10"&&hash!=plugin110)return {};
    if(QFileInfo(directory).isSymLink()||!QDir().mkpath(directory))return {};QFile::setPermissions(directory,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
    QVariantMap record{{"ok",true},{"version",version},{"sha256",hash},{"archiveSha256",digest},{"checkedAt",QDateTime::currentSecsSinceEpoch()}};
    if(!write(directory+"/librnnoise_ladspa.so",binary)||!write(directory+"/verified.json",QJsonDocument(QJsonObject::fromVariantMap(record)).toJson()))return {};
    return record;
}
