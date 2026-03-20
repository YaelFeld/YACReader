#ifndef YACREADER_LIBRARIES_H
#define YACREADER_LIBRARIES_H

#include <QtCore>

class YACReaderLibrary;

/**
 * @brief Enum representing the source type of a library
 */
enum class LibrarySourceType {
    Local,      // Local filesystem
    WebDAV      // WebDAV/Nextcloud server
};

class YACReaderLibraries : public QObject
{
    Q_OBJECT

public:
    YACReaderLibraries();
    YACReaderLibraries(const YACReaderLibraries &source);
    QList<QString> getNames();
    QString getPath(const QString &name);
    QString getPath(int id);
    QString getPath(const QUuid &id);
    QString getDBPath(int id);
    QString getName(int id);
    bool isEmpty();
    bool contains(const QString &name);
    bool contains(int id);
    void remove(const QString &name);
    void rename(const QString &oldName, const QString &newName);
    int getId(const QString &name);
    QUuid getUuid(const QString &name);
    int getIdFromUuid(const QUuid &uuid);
    YACReaderLibraries &operator=(const YACReaderLibraries &source);
    QList<YACReaderLibrary> getLibraries() const;
    QList<YACReaderLibrary> sortedLibraries() const;
    QUuid getLibraryIdFromLegacyId(int legacyId) const;

    // WebDAV library support
    void addWebDAVLibrary(const QString &name, const QString &serverUrl, 
                          const QString &username, const QString &basePath);
    bool isWebDAVLibrary(int id) const;
    bool isWebDAVLibrary(const QString &name) const;

public slots:
    void addLibrary(const QString &name, const QString &path);
    void load();
    bool save();

private:
    QList<YACReaderLibrary> libraries;
};

class YACReaderLibrary
{
public:
    YACReaderLibrary();
    YACReaderLibrary(const QString &name, const QString &path, int legacyId, const QUuid &id);
    
    // WebDAV constructor
    YACReaderLibrary(const QString &name, const QString &serverUrl, 
                     const QString &username, const QString &basePath,
                     int legacyId, const QUuid &id);
    
    QString getName() const;
    QString getPath() const;
    QString getDBPath() const;
    int getLegacyId() const;
    QUuid getId() const;
    
    // Source type
    LibrarySourceType getSourceType() const;
    bool isWebDAV() const;
    bool isLocal() const;
    
    // WebDAV-specific getters
    QString getServerUrl() const;
    QString getUsername() const;
    QString getBasePath() const;
    
    // Setters for WebDAV
    void setWebDAVConfig(const QString &serverUrl, const QString &username, const QString &basePath);
    
    bool operator==(const YACReaderLibrary &other) const;
    bool operator!=(const YACReaderLibrary &other) const;
    friend QDataStream &operator<<(QDataStream &out, const YACReaderLibrary &library);
    friend QDataStream &operator>>(QDataStream &in, YACReaderLibrary &library);
    operator QString() const { return QString("%1 [%2, %3, %4]").arg(name, QString::number(legacyId), id.toString(QUuid::WithoutBraces), path); }

private:
    QString name;
    QString path;           // For local: filesystem path, for WebDAV: server URL
    int legacyId;
    QUuid id;               // stored in `id` file in .yacreaderlibrary
    
    // Source type
    LibrarySourceType sourceType;
    
    // WebDAV-specific fields
    QString serverUrl;      // WebDAV server URL
    QString username;       // WebDAV username
    QString basePath;       // Base path on WebDAV server
};

#endif // YACREADER_LIBRARIES_H
