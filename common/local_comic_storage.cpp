#include "local_comic_storage.h"
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDirIterator>

LocalComicStorage::LocalComicStorage(const QString &rootPath, QObject *parent)
    : IComicStorage(parent), m_rootPath(rootPath)
{
    m_metadataPath = QDir(rootPath).filePath(".yacreaderlibrary");
    m_coversPath = QDir(m_metadataPath).filePath("covers");
}

bool LocalComicStorage::initialize()
{
    QDir rootDir(m_rootPath);
    
    if (!rootDir.exists()) {
        emit errorOccurred("Root path does not exist: " + m_rootPath);
        m_connected = false;
        return false;
    }
    
    // Create metadata directories if they don't exist
    QDir().mkpath(m_metadataPath);
    QDir().mkpath(m_coversPath);
    
    m_connected = true;
    return true;
}

QString LocalComicStorage::normalizePath(const QString &path) const
{
    QString normalized = path;
    
    if (path == "/" || path.isEmpty()) {
        return m_rootPath;
    }
    
    // Remove leading slash and combine with root
    if (normalized.startsWith("/")) {
        normalized = normalized.mid(1);
    }
    
    return QDir(m_rootPath).filePath(normalized);
}

bool LocalComicStorage::isComicFile(const QString &filename) const
{
    static const QStringList extensions = {
        ".cbz", ".cbr", ".cb7", ".cbt", ".pdf",
        ".zip", ".rar", ".7z", ".tar", ".gz"
    };
    
    QString lower = filename.toLower();
    for (const auto &ext : extensions) {
        if (lower.endsWith(ext)) {
            return true;
        }
    }
    return false;
}

QStringList LocalComicStorage::getComicExtensions() const
{
    return {
        "*.cbz", "*.cbr", "*.cb7", "*.cbt", "*.pdf",
        "*.zip", "*.rar", "*.7z", "*.tar", "*.gz",
        "*.CBZ", "*.CBR", "*.CB7", "*.CBT", "*.PDF",
        "*.ZIP", "*.RAR", "*.7Z", "*.TAR", "*.GZ"
    };
}

QList<FolderInfo> LocalComicStorage::listFolders(const QString &path)
{
    QList<FolderInfo> folders;
    QString dirPath = normalizePath(path);
    
    QDir dir(dirPath);
    QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    
    for (const auto &entry : entries) {
        FolderInfo info;
        info.name = entry.fileName();
        info.path = "/" + dir.relativeFilePath(entry.filePath());
        info.parentId = path;
        info.isRoot = (path == "/");
        
        // Count subfolders
        QDir subDir(entry.filePath());
        info.subfolderCount = subDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot).count();
        
        // Count comics
        int comicCount = 0;
        QStringList comics = subDir.entryList(getComicExtensions(), QDir::Files);
        comicCount = comics.count();
        info.comicCount = comicCount;
        
        folders.append(info);
    }
    
    return folders;
}

QList<ComicFileInfo> LocalComicStorage::listComics(const QString &path)
{
    QList<ComicFileInfo> comics;
    QString dirPath = normalizePath(path);
    
    QDir dir(dirPath);
    QStringList filters = getComicExtensions();
    
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
    
    for (const auto &file : files) {
        ComicFileInfo info;
        info.name = file.fileName();
        info.path = "/" + dir.relativeFilePath(file.filePath());
        info.size = file.size();
        info.lastModified = file.lastModified();
        info.hash = computeHash(file.filePath());
        info.isRemote = false;
        
        comics.append(info);
    }
    
    return comics;
}

QList<ComicFileInfo> LocalComicStorage::listComicsRecursive(const QString &path)
{
    QList<ComicFileInfo> comics;
    QString dirPath = normalizePath(path);
    
    QDirIterator it(dirPath, getComicExtensions(), QDir::Files, QDirIterator::Subdirectories);
    
    while (it.hasNext()) {
        it.next();
        QFileInfo file = it.fileInfo();
        
        ComicFileInfo info;
        info.name = file.fileName();
        info.path = "/" + dir.relativeFilePath(file.filePath());
        info.size = file.size();
        info.lastModified = file.lastModified();
        info.hash = computeHash(file.filePath());
        info.isRemote = false;
        
        comics.append(info);
    }
    
    return comics;
}

QByteArray LocalComicStorage::getComicData(const QString &path)
{
    QString filePath = normalizePath(path);
    QFile file(filePath);
    
    if (!file.open(QIODevice::ReadOnly)) {
        emit errorOccurred("Cannot open comic file: " + filePath);
        return QByteArray();
    }
    
    return file.readAll();
}

QByteArray LocalComicStorage::getComicPageData(const QString &comicPath, int pageIndex)
{
    // For local storage, we return the whole comic
    // The comic extraction logic will handle page extraction
    return getComicData(comicPath);
}

bool LocalComicStorage::saveComicMetadata(const QString &comicPath, const QMap<QString, QVariant> &metadata)
{
    QString metaFilePath = getMetadataPath(comicPath);
    QDir().mkpath(QFileInfo(metaFilePath).absolutePath());
    
    QFile file(metaFilePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    QJsonDocument doc(QJsonObject::fromVariantMap(metadata));
    file.write(doc.toJson());
    return true;
}

QMap<QString, QVariant> LocalComicStorage::loadComicMetadata(const QString &comicPath)
{
    QString metaFilePath = getMetadataPath(comicPath);
    QFile file(metaFilePath);
    
    if (!file.open(QIODevice::ReadOnly)) {
        return QMap<QString, QVariant>();
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return doc.object().toVariantMap();
}

QByteArray LocalComicStorage::getCoverImage(const QString &comicPath)
{
    QString coverFilePath = getCoverPath(comicPath);
    QFile file(coverFilePath);
    
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    
    return file.readAll();
}

bool LocalComicStorage::saveCoverImage(const QString &comicPath, const QByteArray &coverData)
{
    QString coverFilePath = getCoverPath(comicPath);
    QDir().mkpath(QFileInfo(coverFilePath).absolutePath());
    
    QFile file(coverFilePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    file.write(coverData);
    return true;
}

int LocalComicStorage::getReadingProgress(const QString /*comicPath*/)
{
    // TODO: Integrate with YACReader's existing progress tracking
    return 0;
}

void LocalComicStorage::setReadingProgress(const QString /*comicPath*/, int /*currentPage*/, int /*totalPages*/)
{
    // TODO: Integrate with YACReader's existing progress tracking
}

bool LocalComicStorage::isComicRead(const QString /*comicPath*/)
{
    // TODO: Integrate with YACReader's existing progress tracking
    return false;
}

void LocalComicStorage::markComicAsRead(const QString /*comicPath*/, bool /*read*/)
{
    // TODO: Integrate with YACReader's existing progress tracking
}

QList<ComicFileInfo> LocalComicStorage::searchComics(const QString &query, const QString &path)
{
    QList<ComicFileInfo> results;
    QList<ComicFileInfo> comics = listComicsRecursive(path);
    
    QString lowerQuery = query.toLower();
    
    for (const auto &comic : comics) {
        if (comic.name.toLower().contains(lowerQuery)) {
            results.append(comic);
        }
    }
    
    return results;
}

QString LocalComicStorage::getMetadataPath(const QString &comicPath) const
{
    QString normalized = comicPath;
    if (normalized.startsWith("/")) {
        normalized = normalized.mid(1);
    }
    
    // Replace file extension with .meta.json
    QFileInfo info(normalized);
    QString metaName = info.completeBaseName() + ".meta.json";
    
    return QDir(m_metadataPath).filePath(metaName);
}

QString LocalComicStorage::getCoverPath(const QString &comicPath) const
{
    QString normalized = comicPath;
    if (normalized.startsWith("/")) {
        normalized = normalized.mid(1);
    }
    
    // Replace file extension with .jpg
    QFileInfo info(normalized);
    QString coverName = info.completeBaseName() + ".jpg";
    
    return QDir(m_coversPath).filePath(coverName);
}

QString LocalComicStorage::computeHash(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(&file);
    return hash.result().toHex();
}
