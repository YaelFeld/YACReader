#include "yacreader_libraries.h"
#include "qnaturalsorting.h"
#include "yacreader_global.h"

using namespace YACReader;

void writeIdToLibraryFolder(const QString &path, const QUuid &id)
{
    QFile file(LibraryPaths::idPath(path));
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << id.toString(QUuid::WithoutBraces);
        file.close();
    }
}

QUuid readFromLibraryFolder(const QString &path)
{
    QFile file(LibraryPaths::idPath(path));
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        QString id = stream.readLine();
        file.close();
        return QUuid(id);
    }

    return QUuid();
}

YACReaderLibraries::YACReaderLibraries()
    : QObject()
{
}

YACReaderLibraries::YACReaderLibraries(const YACReaderLibraries &source)
    : QObject(), libraries(source.libraries)
{
}

QList<QString> YACReaderLibraries::getNames()
{
    QList<QString> names;
    std::transform(libraries.cbegin(), libraries.cend(), std::back_inserter(names), [](const YACReaderLibrary &library) { return library.getName(); });
    return names;
}

QString YACReaderLibraries::getPath(const QString &name)
{
    auto library = std::find_if(libraries.cbegin(), libraries.cend(), [name](const YACReaderLibrary &library) { return library.getName() == name; });
    return library != libraries.cend() ? library->getPath() : "";
}

QString YACReaderLibraries::getPath(int id)
{
    auto library = std::find_if(libraries.cbegin(), libraries.cend(), [id](const YACReaderLibrary &library) { return library.getLegacyId() == id; });
    return library != libraries.cend() ? library->getPath() : "";
}

QString YACReaderLibraries::getPath(const QUuid &id)
{
    auto library = std::find_if(libraries.cbegin(), libraries.cend(), [id](const YACReaderLibrary &library) { return library.getId() == id; });
    return library != libraries.cend() ? library->getPath() : "";
}

QString YACReaderLibraries::getDBPath(int id)
{
    return LibraryPaths::libraryDataPath(getPath(id));
}

QString YACReaderLibraries::getName(int id)
{
    auto library = std::find_if(libraries.cbegin(), libraries.cend(), [id](const YACReaderLibrary &library) { return library.getLegacyId() == id; });
    return library != libraries.cend() ? library->getName() : "";
}

bool YACReaderLibraries::isEmpty()
{
    return libraries.isEmpty();
}

bool YACReaderLibraries::contains(const QString &name)
{
    auto library = std::find_if(libraries.cbegin(), libraries.cend(), [name](const YACReaderLibrary &library) { return library.getName() == name; });
    return library != libraries.cend();
}

bool YACReaderLibraries::contains(int id)
{
    auto library = std::find_if(libraries.cbegin(), libraries.cend(), [id](const YACReaderLibrary &library) { return library.getLegacyId() == id; });
    return library != libraries.cend();
}

void YACReaderLibraries::remove(const QString &name)
{
    auto library = std::find_if(libraries.begin(), libraries.end(), [name](const YACReaderLibrary &library) { return library.getName() == name; });
    libraries.erase(library);
}

void YACReaderLibraries::rename(const QString &oldName, const QString &newName)
{
    auto library = std::find_if(libraries.begin(), libraries.end(), [oldName](const YACReaderLibrary &library) { return library.getName() == oldName; });
    if (library != libraries.end()) {
        auto oldLibrary = *library;
        libraries.erase(library);
        libraries.append(YACReaderLibrary(newName, oldLibrary.getPath(), oldLibrary.getLegacyId(), oldLibrary.getId()));
    }
}

int YACReaderLibraries::getId(const QString &name)
{
    auto library = std::find_if(libraries.cbegin(), libraries.cend(), [name](const YACReaderLibrary &library) { return library.getName() == name; });
    return library != libraries.cend() ? library->getLegacyId() : -1;
}

QUuid YACReaderLibraries::getUuid(const QString &name)
{
    auto library = std::find_if(libraries.cbegin(), libraries.cend(), [name](const YACReaderLibrary &library) { return library.getName() == name; });
    return library != libraries.cend() ? library->getId() : QUuid();
}

int YACReaderLibraries::getIdFromUuid(const QUuid &uuid)
{
    auto library = std::find_if(libraries.cbegin(), libraries.cend(), [uuid](const YACReaderLibrary &library) { return library.getId() == uuid; });
    return library != libraries.cend() ? library->getLegacyId() : -1;
}

YACReaderLibraries &YACReaderLibraries::operator=(const YACReaderLibraries &source)
{
    libraries = source.libraries;
    return *this;
}

QList<YACReaderLibrary> YACReaderLibraries::getLibraries() const
{
    return libraries;
}

QList<YACReaderLibrary> YACReaderLibraries::sortedLibraries() const
{
    auto sortedLibraries = libraries;
    std::sort(sortedLibraries.begin(), sortedLibraries.end(), [](const YACReaderLibrary &library1, const YACReaderLibrary &library2) { return naturalSortLessThanCI(library1.getName(), library2.getName()); });
    return sortedLibraries;
}

QUuid YACReaderLibraries::getLibraryIdFromLegacyId(int legacyId) const
{
    auto library = std::find_if(libraries.cbegin(), libraries.cend(), [legacyId](const YACReaderLibrary &library) { return library.getLegacyId() == legacyId; });
    return library != libraries.cend() ? library->getId() : QUuid();
}

void YACReaderLibraries::addLibrary(const QString &name, const QString &path)
{
    int legacyId = 0;
    foreach (YACReaderLibrary l, libraries)
        legacyId = qMax(legacyId, l.getLegacyId());
    legacyId++;

    auto id = readFromLibraryFolder(path);

    if (id.isNull()) {
        id = QUuid::createUuid();
        writeIdToLibraryFolder(path, id);
    }
    libraries.append(YACReaderLibrary(name, path, legacyId, id));
}

void YACReaderLibraries::addWebDAVLibrary(const QString &name, const QString &serverUrl, 
                                          const QString &username, const QString &basePath)
{
    int legacyId = 0;
    foreach (YACReaderLibrary l, libraries)
        legacyId = qMax(legacyId, l.getLegacyId());
    legacyId++;

    QUuid id = QUuid::createUuid();
    
    libraries.append(YACReaderLibrary(name, serverUrl, username, basePath, legacyId, id));
}

bool YACReaderLibraries::isWebDAVLibrary(int id) const
{
    auto library = std::find_if(libraries.cbegin(), libraries.cend(), [id](const YACReaderLibrary &library) { 
        return library.getLegacyId() == id && library.isWebDAV(); 
    });
    return library != libraries.cend();
}

bool YACReaderLibraries::isWebDAVLibrary(const QString &name) const
{
    auto library = std::find_if(libraries.cbegin(), libraries.cend(), [name](const YACReaderLibrary &library) { 
        return library.getName() == name && library.isWebDAV(); 
    });
    return library != libraries.cend();
}

void YACReaderLibraries::load()
{
    QSettings settings(YACReader::getSettingsPath() + "/" + QCoreApplication::applicationName() + ".ini", QSettings::IniFormat);

    if (settings.value(LIBRARIES).isValid()) {
        QByteArray data = settings.value(LIBRARIES).toByteArray();

        // in 9.14 the format of libraries has changed, so we need to check if we can do a migration
        QDataStream legacyIn(&data, QIODevice::ReadOnly);
        QMap<QString, QPair<int, QString>> legacyLibraries;
        legacyIn >> legacyLibraries;

        auto needsMigration = !legacyLibraries.isEmpty();
        if (needsMigration) {
            QList<YACReaderLibrary> migratedLibraries;

            for (auto i = legacyLibraries.cbegin(), end = legacyLibraries.cend(); i != end; ++i) {
                auto name = i.key();
                auto value = i.value();
                auto newId = QUuid::createUuid();

                writeIdToLibraryFolder(value.second, newId);

                auto library = YACReaderLibrary(name, value.second, value.first, newId);
                migratedLibraries.append(library);
            }

            libraries = migratedLibraries;
            save();
        } else {
            QDataStream in(&data, QIODevice::ReadOnly);
            in >> libraries;
        }
    }
}

bool YACReaderLibraries::save()
{
    QSettings settings(YACReader::getSettingsPath() + "/" + QCoreApplication::applicationName() + ".ini", QSettings::IniFormat);

    QByteArray data;
    QDataStream out(&data, QIODevice::WriteOnly);
    out << libraries;
    settings.setValue(LIBRARIES, data);

    return settings.isWritable();
}

YACReaderLibrary::YACReaderLibrary()
    : name(), path(), legacyId(0), id(), sourceType(LibrarySourceType::Local), 
      serverUrl(), username(), basePath()
{
}

YACReaderLibrary::YACReaderLibrary(const QString &name, const QString &path, int legacyId, const QUuid &id)
    : name(name), path(path), legacyId(legacyId), id(id), sourceType(LibrarySourceType::Local),
      serverUrl(), username(), basePath()
{
}

YACReaderLibrary::YACReaderLibrary(const QString &name, const QString &serverUrl, 
                                   const QString &username, const QString &basePath,
                                   int legacyId, const QUuid &id)
    : name(name), path(serverUrl), legacyId(legacyId), id(id), sourceType(LibrarySourceType::WebDAV),
      serverUrl(serverUrl), username(username), basePath(basePath.isEmpty() ? "/" : basePath)
{
}

QString YACReaderLibrary::getName() const
{
    return name;
}

QString YACReaderLibrary::getPath() const
{
    return path;
}

QString YACReaderLibrary::getDBPath() const
{
    return LibraryPaths::libraryDataPath(path);
}

int YACReaderLibrary::getLegacyId() const
{
    return legacyId;
}

QUuid YACReaderLibrary::getId() const
{
    return id;
}

LibrarySourceType YACReaderLibrary::getSourceType() const
{
    return sourceType;
}

bool YACReaderLibrary::isWebDAV() const
{
    return sourceType == LibrarySourceType::WebDAV;
}

bool YACReaderLibrary::isLocal() const
{
    return sourceType == LibrarySourceType::Local;
}

QString YACReaderLibrary::getServerUrl() const
{
    return serverUrl;
}

QString YACReaderLibrary::getUsername() const
{
    return username;
}

QString YACReaderLibrary::getBasePath() const
{
    return basePath.isEmpty() ? "/" : basePath;
}

void YACReaderLibrary::setWebDAVConfig(const QString &url, const QString &user, const QString &bPath)
{
    sourceType = LibrarySourceType::WebDAV;
    serverUrl = url;
    username = user;
    basePath = bPath.isEmpty() ? "/" : bPath;
    path = url;
}

bool YACReaderLibrary::operator==(const YACReaderLibrary &other) const
{
    return id == other.id && name == other.name && path == other.path && 
           legacyId == other.legacyId && sourceType == other.sourceType;
}

bool YACReaderLibrary::operator!=(const YACReaderLibrary &other) const
{
    return !(*this == other);
}

QDataStream &operator<<(QDataStream &out, const YACReaderLibrary &library)
{
    out << library.name << library.path << library.legacyId << library.id;
    // Write source type as int
    out << static_cast<int>(library.sourceType);
    // Write WebDAV-specific fields
    out << library.serverUrl << library.username << library.basePath;
    return out;
}

QDataStream &operator>>(QDataStream &in, YACReaderLibrary &library)
{
    in >> library.name >> library.path >> library.legacyId >> library.id;
    
    // Try to read source type (for backward compatibility, default to Local if not present)
    int sourceTypeInt;
    if (!in.atEnd()) {
        in >> sourceTypeInt;
        library.sourceType = static_cast<LibrarySourceType>(sourceTypeInt);
    } else {
        library.sourceType = LibrarySourceType::Local;
    }
    
    // Read WebDAV-specific fields (if available)
    if (!in.atEnd()) {
        in >> library.serverUrl >> library.username >> library.basePath;
    }
    
    return in;
}
