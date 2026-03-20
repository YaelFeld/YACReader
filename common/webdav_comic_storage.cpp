#include "webdav_comic_storage.h"
#include <QDir>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QEventLoop>
#include <QTimer>

WebDAVComicStorage::WebDAVComicStorage(QObject *parent)
    : IComicStorage(parent),
      m_cacheDir(nullptr),
      m_dataCache(MAX_CACHE_SIZE),
      m_metadataCache(1000)  // 1000 items max
{
    // Create temporary directory for caching
    m_cacheDir = new QTemporaryDir(QDir::tempPath() + "/yacreader_webdav_cache_");
}

WebDAVComicStorage::~WebDAVComicStorage()
{
    clearCache();
    delete m_cacheDir;
}

void WebDAVComicStorage::setServerUrl(const QString &url)
{
    m_webdavClient.setServerUrl(url);
}

void WebDAVComicStorage::setCredentials(const QString &username, const QString &password)
{
    m_webdavClient.setCredentials(username, password);
    m_username = username;
}

void WebDAVComicStorage::setBasePath(const QString &path)
{
    m_basePath = normalizePath(path);
}

bool WebDAVComicStorage::initialize()
{
    if (!m_cacheDir->isValid()) {
        emit errorOccurred("Failed to create cache directory");
        m_connected = false;
        return false;
    }

    // Test connection to WebDAV server
    m_connected = testConnection();
    
    if (m_connected) {
        // Ensure base path exists on server
        if (!m_basePath.isEmpty() && m_basePath != "/") {
            m_webdavClient.createDirectory(m_basePath);
        }
    }
    
    return m_connected;
}

bool WebDAVComicStorage::testConnection()
{
    return m_webdavClient.testConnection();
}

QString WebDAVComicStorage::normalizePath(const QString &path) const
{
    QString normalized = path;
    
    // Ensure path starts with /
    if (!normalized.startsWith("/")) {
        normalized = "/" + normalized;
    }
    
    // Remove trailing slash except for root
    if (normalized.length() > 1 && normalized.endsWith("/")) {
        normalized.chop(1);
    }
    
    return normalized;
}

QString WebDAVComicStorage::getLocalCachePath(const QString &remotePath) const
{
    QString normalized = remotePath;
    if (normalized.startsWith("/")) {
        normalized = normalized.mid(1);
    }
    
    // Replace path separators with underscores for safe filenames
    normalized.replace("/", "_");
    
    return m_cacheDir->path() + "/" + normalized;
}

QString WebDAVComicStorage::getMetadataPath(const QString &comicPath) const
{
    // Store metadata in a sidecar file with .yacreader extension
    return comicPath + ".yacreader";
}

QString WebDAVComicStorage::getCoverPath(const QString &comicPath) const
{
    // Store cover in cache with hash of path
    QString hash = computeHash(comicPath.toUtf8());
    return m_cacheDir->path() + "/covers/" + hash + ".jpg";
}

QString WebDAVComicStorage::computeHash(const QByteArray &data) const
{
    return QCryptographicHash::hash(data, QCryptographicHash::Md5).toHex();
}

bool WebDAVComicStorage::isComicFile(const QString &filename) const
{
    static const QStringList extensions = {
        ".cbz", ".cbr", ".cb7", ".cbt", ".pdf",
        ".zip", ".rar", ".7z", ".tar"
    };
    
    QString lower = filename.toLower();
    for (const auto &ext : extensions) {
        if (lower.endsWith(ext)) {
            return true;
        }
    }
    return false;
}

ComicFileInfo WebDAVComicStorage::webDAVItemToComicFileInfo(const WebDAVItem &item) const
{
    ComicFileInfo info;
    info.name = item.name;
    info.path = item.path;
    info.size = item.contentLength;
    info.lastModified = QDateTime::fromString(item.lastModified, Qt::RFC2822Date);
    if (!info.lastModified.isValid()) {
        // Try ISO format
        info.lastModified = QDateTime::fromString(item.lastModified, Qt::ISODate);
    }
    info.hash = item.etag;
    info.isRemote = true;
    return info;
}

QList<FolderInfo> WebDAVComicStorage::listFolders(const QString &path)
{
    QList<FolderInfo> folders;
    
    QString fullPath = m_basePath + normalizePath(path);
    QList<WebDAVItem> items = m_webdavClient.listDirectory(fullPath);
    
    for (const auto &item : items) {
        if (item.isDirectory) {
            FolderInfo info;
            info.name = item.name;
            info.path = item.path.mid(m_basePath.length());
            if (info.path.isEmpty()) {
                info.path = "/";
            }
            info.parentId = path;
            info.isRoot = (info.path == "/");
            
            // Count contents (this requires additional requests)
            QString subPath = m_basePath + normalizePath(info.path);
            QList<WebDAVItem> subItems = m_webdavClient.listDirectory(subPath);
            
            info.subfolderCount = 0;
            info.comicCount = 0;
            
            for (const auto &subItem : subItems) {
                if (subItem.isDirectory) {
                    info.subfolderCount++;
                } else if (isComicFile(subItem.name)) {
                    info.comicCount++;
                }
            }
            
            folders.append(info);
        }
    }
    
    return folders;
}

QList<ComicFileInfo> WebDAVComicStorage::listComics(const QString &path)
{
    QList<ComicFileInfo> comics;
    
    QString fullPath = m_basePath + normalizePath(path);
    QList<WebDAVItem> items = m_webdavClient.listDirectory(fullPath);
    
    for (const auto &item : items) {
        if (!item.isDirectory && isComicFile(item.name)) {
            comics.append(webDAVItemToComicFileInfo(item));
        }
    }
    
    return comics;
}

QList<ComicFileInfo> WebDAVComicStorage::listComicsRecursive(const QString &path)
{
    QList<ComicFileInfo> comics;
    
    // First, get comics in current folder
    comics.append(listComics(path));
    
    // Then recursively get comics from subfolders
    QList<FolderInfo> folders = listFolders(path);
    for (const auto &folder : folders) {
        comics.append(listComicsRecursive(folder.path));
    }
    
    return comics;
}

QByteArray WebDAVComicStorage::getComicData(const QString &path)
{
    QString fullPath = m_basePath + normalizePath(path);
    
    // Check cache first
    if (m_dataCache.contains(fullPath)) {
        return *m_dataCache.object(fullPath);
    }
    
    // Download from WebDAV
    QByteArray data = m_webdavClient.downloadFile(fullPath);
    
    if (!data.isEmpty()) {
        // Cache the data
        m_dataCache.insert(fullPath, new QByteArray(data));
        
        // Also save to disk cache for persistence
        QString cachePath = getLocalCachePath(fullPath);
        QDir().mkpath(QFileInfo(cachePath).absolutePath());
        
        QFile cacheFile(cachePath);
        if (cacheFile.open(QIODevice::WriteOnly)) {
            cacheFile.write(data);
        }
    }
    
    return data;
}

QByteArray WebDAVComicStorage::getComicPageData(const QString &comicPath, int pageIndex)
{
    // For WebDAV, we return the whole comic file
    // The comic extraction logic will handle page extraction
    Q_UNUSED(pageIndex)
    return getComicData(comicPath);
}

bool WebDAVComicStorage::saveComicMetadata(const QString &comicPath, const QMap<QString, QVariant> &metadata)
{
    QString metaPath = getMetadataPath(m_basePath + normalizePath(comicPath));
    
    QJsonDocument doc(QJsonObject::fromVariantMap(metadata));
    QByteArray data = doc.toJson();
    
    // Save to local cache
    m_metadataCache.insert(metaPath, new QString(data));
    
    // Upload to WebDAV
    QString tempPath = m_cacheDir->path() + "/temp_meta_" + computeHash(metaPath.toUtf8());
    QFile tempFile(tempPath);
    if (tempFile.open(QIODevice::WriteOnly)) {
        tempFile.write(data);
        tempFile.close();
        
        return m_webdavClient.uploadFile(tempPath, metaPath);
    }
    
    return false;
}

QMap<QString, QVariant> WebDAVComicStorage::loadComicMetadata(const QString &comicPath)
{
    QString metaPath = getMetadataPath(m_basePath + normalizePath(comicPath));
    
    // Check cache first
    if (m_metadataCache.contains(metaPath)) {
        QString data = *m_metadataCache.object(metaPath);
        QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8());
        return doc.object().toVariantMap();
    }
    
    // Download from WebDAV
    QByteArray data = m_webdavClient.downloadFile(metaPath);
    
    if (!data.isEmpty()) {
        m_metadataCache.insert(metaPath, new QString(data));
        QJsonDocument doc = QJsonDocument::fromJson(data);
        return doc.object().toVariantMap();
    }
    
    return QMap<QString, QVariant>();
}

QByteArray WebDAVComicStorage::getCoverImage(const QString &comicPath)
{
    // Try to get cached cover first
    QString coverCachePath = getCoverPath(comicPath);
    QFile cacheFile(coverCachePath);
    if (cacheFile.exists() && cacheFile.open(QIODevice::ReadOnly)) {
        QByteArray data = cacheFile.readAll();
        cacheFile.close();
        return data;
    }
    
    // Check if cover exists on WebDAV
    QString coverPath = comicPath + ".cover.jpg";
    QByteArray data = m_webdavClient.downloadFile(m_basePath + normalizePath(coverPath));
    
    if (!data.isEmpty()) {
        // Cache locally
        QDir().mkpath(QFileInfo(coverCachePath).absolutePath());
        if (cacheFile.open(QIODevice::WriteOnly)) {
            cacheFile.write(data);
            cacheFile.close();
        }
    }
    
    return data;
}

bool WebDAVComicStorage::saveCoverImage(const QString &comicPath, const QByteArray &coverData)
{
    // Cache locally first
    QString coverCachePath = getCoverPath(comicPath);
    QDir().mkpath(QFileInfo(coverCachePath).absolutePath());
    
    QFile cacheFile(coverCachePath);
    if (!cacheFile.open(QIODevice::WriteOnly)) {
        return false;
    }
    cacheFile.write(coverData);
    cacheFile.close();
    
    // Upload to WebDAV
    QString coverPath = m_basePath + normalizePath(comicPath) + ".cover.jpg";
    QString tempPath = m_cacheDir->path() + "/temp_cover_" + computeHash(coverPath.toUtf8());
    
    QFile tempFile(tempPath);
    if (tempFile.open(QIODevice::WriteOnly)) {
        tempFile.write(coverData);
        tempFile.close();
        
        return m_webdavClient.uploadFile(tempPath, coverPath);
    }
    
    return false;
}

int WebDAVComicStorage::getReadingProgress(const QString &comicPath)
{
    return m_readingProgress.value(m_basePath + normalizePath(comicPath), 0);
}

void WebDAVComicStorage::setReadingProgress(const QString &comicPath, int currentPage, int totalPages)
{
    QString fullPath = m_basePath + normalizePath(comicPath);
    m_readingProgress[fullPath] = currentPage;
    m_totalPages[fullPath] = totalPages;
    
    // Persist progress to metadata
    QMap<QString, QVariant> metadata = loadComicMetadata(comicPath);
    metadata["currentPage"] = currentPage;
    metadata["totalPages"] = totalPages;
    metadata["lastRead"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    saveComicMetadata(comicPath, metadata);
}

bool WebDAVComicStorage::isComicRead(const QString &comicPath)
{
    return m_readComics.contains(m_basePath + normalizePath(comicPath));
}

void WebDAVComicStorage::markComicAsRead(const QString &comicPath, bool read)
{
    QString fullPath = m_basePath + normalizePath(comicPath);
    
    if (read) {
        m_readComics.insert(fullPath);
    } else {
        m_readComics.remove(fullPath);
    }
    
    // Persist to metadata
    QMap<QString, QVariant> metadata = loadComicMetadata(comicPath);
    metadata["isRead"] = read;
    metadata["lastRead"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    saveComicMetadata(comicPath, metadata);
}

QList<ComicFileInfo> WebDAVComicStorage::searchComics(const QString &query, const QString &path)
{
    QList<ComicFileInfo> results;
    QList<ComicFileInfo> allComics = listComicsRecursive(path);
    
    QString lowerQuery = query.toLower();
    
    for (const auto &comic : allComics) {
        if (comic.name.toLower().contains(lowerQuery)) {
            results.append(comic);
        }
    }
    
    return results;
}

void WebDAVComicStorage::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    emit progressChanged(bytesReceived, bytesTotal);
}

void WebDAVComicStorage::cacheComic(const QString &remotePath)
{
    // Pre-download comic to local cache
    QByteArray data = m_webdavClient.downloadFile(remotePath);
    
    if (!data.isEmpty()) {
        QString cachePath = getLocalCachePath(remotePath);
        QDir().mkpath(QFileInfo(cachePath).absolutePath());
        
        QFile cacheFile(cachePath);
        if (cacheFile.open(QIODevice::WriteOnly)) {
            cacheFile.write(data);
        }
        
        m_dataCache.insert(remotePath, new QByteArray(data));
    }
}

bool WebDAVComicStorage::isCached(const QString &remotePath) const
{
    if (m_dataCache.contains(remotePath)) {
        return true;
    }
    
    QString cachePath = getLocalCachePath(remotePath);
    return QFile::exists(cachePath);
}

void WebDAVComicStorage::clearCache()
{
    m_dataCache.clear();
    m_metadataCache.clear();
    
    if (m_cacheDir && m_cacheDir->isValid()) {
        QDir dir(m_cacheDir->path());
        dir.removeRecursively();
    }
}
