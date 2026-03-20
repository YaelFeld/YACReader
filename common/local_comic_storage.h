#ifndef LOCAL_COMIC_STORAGE_H
#define LOCAL_COMIC_STORAGE_H

#include "i_comic_storage.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

/**
 * @brief Local filesystem implementation of IComicStorage
 * 
 * This class wraps YACReader's existing local file access code
 * to conform to the new storage interface.
 */
class LocalComicStorage : public IComicStorage
{
    Q_OBJECT

public:
    explicit LocalComicStorage(const QString &rootPath, QObject *parent = nullptr);
    ~LocalComicStorage() override = default;

    // Connection/Initialization
    bool initialize() override;
    bool isConnected() const override { return m_connected; }
    QString storageType() const override { return "local"; }
    QString storageName() const override { return m_rootPath; }

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

    // Local-specific methods
    QString rootPath() const { return m_rootPath; }
    void setRootPath(const QString &path) { m_rootPath = path; }

private:
    QString normalizePath(const QString &path) const;
    QString getMetadataPath(const QString &comicPath) const;
    QString getCoverPath(const QString &comicPath) const;
    QString computeHash(const QString &filePath) const;
    bool isComicFile(const QString &filename) const;
    QStringList getComicExtensions() const;

    QString m_rootPath;
    QString m_metadataPath;  // .yacreaderlibrary equivalent
    QString m_coversPath;
};

#endif // LOCAL_COMIC_STORAGE_H
