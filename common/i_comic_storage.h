#ifndef I_COMIC_STORAGE_H
#define I_COMIC_STORAGE_H

#include <QObject>
#include <QList>
#include <QByteArray>
#include <QMap>
#include <QDateTime>

/**
 * @brief Metadata about a comic file
 */
struct ComicFileInfo {
    QString name;           // Comic filename
    QString path;           // Full path
    qint64 size;            // File size in bytes
    QDateTime lastModified; // Last modified date
    QString hash;           // File hash for change detection
    bool isRemote;          // true if stored remotely
};

/**
 * @brief Folder information for library organization
 */
struct FolderInfo {
    QString name;
    QString path;
    QString parentId;
    int comicCount;
    int subfolderCount;
    bool isRoot;
};

/**
 * @brief Interface for comic storage backends
 * 
 * This abstract class defines the interface that all storage backends
 * must implement (local filesystem, WebDAV, cloud services, etc.)
 */
class IComicStorage : public QObject
{
    Q_OBJECT

public:
    explicit IComicStorage(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~IComicStorage() = default;

    // Connection/Initialization
    virtual bool initialize() = 0;
    virtual bool isConnected() const = 0;
    virtual QString storageType() const = 0;  // "local" or "webdav"
    virtual QString storageName() const = 0;  // User-friendly name

    // Directory browsing
    virtual QList<FolderInfo> listFolders(const QString &path = "/") = 0;
    virtual QList<ComicFileInfo> listComics(const QString &path = "/") = 0;
    virtual QList<ComicFileInfo> listComicsRecursive(const QString &path = "/") = 0;

    // File operations
    virtual QByteArray getComicData(const QString &path) = 0;
    virtual QByteArray getComicPageData(const QString &comicPath, int pageIndex) = 0;
    virtual bool saveComicMetadata(const QString &comicPath, const QMap<QString, QVariant> &metadata) = 0;
    virtual QMap<QString, QVariant> loadComicMetadata(const QString &comicPath) = 0;

    // Cover management
    virtual QByteArray getCoverImage(const QString &comicPath) = 0;
    virtual bool saveCoverImage(const QString &comicPath, const QByteArray &coverData) = 0;

    // Progress tracking
    virtual int getReadingProgress(const QString &comicPath) = 0;
    virtual void setReadingProgress(const QString &comicPath, int currentPage, int totalPages) = 0;
    virtual bool isComicRead(const QString &comicPath) = 0;
    virtual void markComicAsRead(const QString &comicPath, bool read = true) = 0;

    // Search
    virtual QList<ComicFileInfo> searchComics(const QString &query, const QString &path = "/") = 0;

signals:
    void progressChanged(qint64 current, qint64 total);
    void errorOccurred(const QString &error);
    void connectionLost();
    void dataReady(const QByteArray &data);

protected:
    bool m_connected = false;
    QString m_basePath;
};

#endif // I_COMIC_STORAGE_H
