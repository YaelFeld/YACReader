#ifndef WEBDAV_CLIENT_H
#define WEBDAV_CLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <QByteArray>
#include <QList>
#include <QMap>

/**
 * @brief Structure representing a file or folder from WebDAV
 */
struct WebDAVItem {
    QString name;           // File/folder name
    QString path;           // Full path on server
    QString href;           // WebDAV href
    bool isDirectory;       // true if folder, false if file
    qint64 contentLength;   // File size (0 for directories)
    QString lastModified;   // Last modified timestamp
    QString contentType;    // MIME type
    QString etag;           // Entity tag for caching
};

/**
 * @brief WebDAV client for Nextcloud integration
 * 
 * This class provides WebDAV protocol operations for connecting to Nextcloud
 * and other WebDAV-compatible servers.
 */
class WebDAVClient : public QObject
{
    Q_OBJECT

public:
    explicit WebDAVClient(QObject *parent = nullptr);
    ~WebDAVClient() override;

    // Configuration
    void setServerUrl(const QString &url);
    void setCredentials(const QString &username, const QString &password);
    QString serverUrl() const;
    QString username() const;

    // Connection testing
    bool testConnection();

    // File operations
    QList<WebDAVItem> listDirectory(const QString &path = "/");
    QByteArray downloadFile(const QString &path);
    bool downloadFileAsync(const QString &path, const QString &localPath);
    bool uploadFile(const QString &localPath, const QString &remotePath);
    bool deleteFile(const QString &path);
    bool createDirectory(const QString &path);

    // Streaming support for comics
    QNetworkReply* streamFile(const QString &path, qint64 start = 0, qint64 end = -1);
    QByteArray downloadRange(const QString &path, qint64 start, qint64 length);

    // Utility
    static QString sanitizePath(const QString &path);
    static bool isComicFile(const QString &filename);

signals:
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void uploadProgress(qint64 bytesSent, qint64 bytesTotal);
    void errorOccurred(const QString &error);
    void connectionSuccess();
    void connectionFailed(const QString &error);

private:
    QNetworkReply* sendRequest(const QString &method, const QString &path, 
                                const QByteArray &body = QByteArray(),
                                const QMap<QString, QString> &headers = {});
    
    QList<WebDAVItem> parsePropfindResponse(const QByteArray &xml);
    QString extractPathFromHref(const QString &href);
    QNetworkRequest createRequest(const QString &path);
    QString base64Credentials() const;

    QUrl m_serverUrl;
    QString m_username;
    QString m_password;
    QNetworkAccessManager m_networkManager;
    
    static const QStringList COMIC_EXTENSIONS;
};

#endif // WEBDAV_CLIENT_H
