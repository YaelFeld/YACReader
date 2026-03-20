#ifndef WEBDAV_COMIC_STORAGE_H
#define WEBDAV_COMIC_STORAGE_H

#include "i_comic_storage.h"
#include "webdav_client.h"
#include <QCache>
#include <QTemporaryDir>

/**
 * @brief WebDAV implementation of IComicStorage for Nextcloud integration
 * 
 * This class provides cloud storage capabilities by wrapping the WebDAVClient
 * and adding caching for performance optimization.
 */
class WebDAVComicStorage : public IComicStorage
{
    Q_OBJECT

public:
    explicit WebDAVComicStorage(QObject *parent = nullptr);
    ~WebDAVComicStorage() override;

    // Configuration
    void setServerUrl(const QString &url);
    void setCredentials(const QString &username, const QString &password);
    void setBasePath(const QString &path);  // Base path in Nextcloud (e.g., /Comics)
    
    QString serverUrl() const;
    QString username() const;
    QString basePath() const { return m_basePath; }

    // Connection/Initialization
    bool initialize() override;
    bool isConnected() const override { return m_connected; }
    QString storageType() const override { return "webdav"; }
    QString storageName() const override { return m_serverUrl.host() + m_basePath; }

    // Directory browsing
    QList<FolderInfo> listFolders(const QString &path = "/") override;
    QList<ComicFileInfo> listComics(const QString &path = "/") override;
    QList<ComicFileInfo> listComicsRecursive(const QString &path = "/") override;

    // File operations
    QByteArray getComicData(const QString &path) override;
    QByteArray getComicPageData(const QString &comicPath, int pageIndex) override;
    bool saveComicMetadata(const QString &comicPath, const QMap<QString, QVariant> &metadata) override;
    QMap<QString, QVariant> loadComicMetadata(const QString &comicPath) override;

    // Cover management
    QByteArray getCoverImage(const QString &comicPath) override;
    bool saveCoverImage(const QString &comicPath, const QByteArray &coverData) override;

    // Progress tracking
    int getReadingProgress(const QString &comicPath) override;
    void setReadingProgress(const QString &comicPath, int currentPage, int totalPages) override;
    bool isComicRead(const QString &comicPath) override;
    void markComicAsRead(const QString &comicPath, bool read = true) override;

    // Search
    QList<ComicFileInfo> searchComics(const QString &query, const QString &path = "/") override;

    // Test connection
    bool testConnection();

private slots:
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);

private:
    QString normalizePath(const QString &path) const;
    QString getLocalCachePath(const QString &remotePath) const;
    QString getMetadataPath(const QString &comicPath) const;
    QString getCoverPath(const QString &comicPath) const;
    QString computeHash(const QByteArray &data) const;
    bool isComicFile(const QString &filename) const;
    ComicFileInfo webDAVItemToComicFileInfo(const WebDAVItem &item) const;
    
    // Cache management
    void cacheComic(const QString &remotePath);
    bool isCached(const QString &remotePath) const;
    void clearCache();

    WebDAVClient m_webdavClient;
    QString m_basePath;
    QString m_username;
    
    // Local caching for performance
    QTemporaryDir *m_cacheDir;
    QCache<QString, QByteArray> m_dataCache;  // Cache for comic data
    QCache<QString, QString> m_metadataCache; // Cache for metadata
    
    // Progress tracking stored locally
    QMap<QString, int> m_readingProgress;   // comic path -> current page
    QMap<QString, int> m_totalPages;         // comic path -> total pages
    QSet<QString> m_readComics;              // Set of read comic paths
    
    static const int MAX_CACHE_SIZE = 100 * 1024 * 1024; // 100MB max cache
};

#endif // WEBDAV_COMIC_STORAGE_H
