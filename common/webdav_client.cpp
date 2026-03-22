#include "webdav_client.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QAuthenticator>
#include <QXmlStreamReader>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QEventLoop>
#include <QTimer>
#include <QRegularExpression>

const QStringList WebDAVClient::COMIC_EXTENSIONS = {
    ".cbz", ".cbr", ".cb7", ".cbt", ".pdf",
    ".zip", ".rar", ".7z", ".tar", ".gz"
};

WebDAVClient::WebDAVClient(QObject *parent)
    : QObject(parent)
{
}

WebDAVClient::~WebDAVClient()
{
    m_networkManager.clearAccessCache();
}

void WebDAVClient::setServerUrl(const QString &url)
{
    m_serverUrl = QUrl(url);
    if (!m_serverUrl.isValid()) {
        emit errorOccurred("Invalid server URL: " + url);
    }
}

void WebDAVClient::setCredentials(const QString &username, const QString &password)
{
    m_username = username;
    m_password = password;
}

QString WebDAVClient::serverUrl() const
{
    return m_serverUrl.toString();
}

QString WebDAVClient::username() const
{
    return m_username;
}

QString WebDAVClient::base64Credentials() const
{
    QString credentials = m_username + ":" + m_password;
    return credentials.toUtf8().toBase64();
}

QNetworkRequest WebDAVClient::createRequest(const QString &path)
{
    QUrl url = m_serverUrl;
    QString cleanPath = sanitizePath(path);
    
    // Ensure path starts with /
    if (!cleanPath.startsWith("/")) {
        cleanPath = "/" + cleanPath;
    }
    
    // Nextcloud WebDAV base path
    url.setPath("/remote.php/dav" + cleanPath);
    
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", "Basic " + base64Credentials().toUtf8());
    request.setRawHeader("User-Agent", "YACReader/1.0");
    
    return request;
}

QString WebDAVClient::sanitizePath(const QString &path)
{
    QString cleaned = path;
    
    // Remove double slashes
    cleaned.replace(QRegularExpression("/+"), "/");
    
    // Ensure no trailing slash for files
    if (cleaned.length() > 1 && cleaned.endsWith("/")) {
        cleaned.chop(1);
    }
    
    return cleaned;
}

bool WebDAVClient::isComicFile(const QString &filename)
{
    QString lower = filename.toLower();
    for (const auto &ext : COMIC_EXTENSIONS) {
        if (lower.endsWith(ext)) {
            return true;
        }
    }
    return false;
}

QNetworkReply* WebDAVClient::sendRequest(const QString &method, const QString &path,
                                          const QByteArray &body,
                                          const QMap<QString, QString> &headers)
{
    QNetworkRequest request = createRequest(path);
    
    for (auto it = headers.begin(); it != headers.end(); ++it) {
        request.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
    }
    
    QNetworkReply *reply = m_networkManager.sendCustomRequest(request, method.toUtf8(), body);
    
    // Authentication is handled via Basic Auth header set in createRequest()
    // Qt 6 removed QNetworkReply::authenticationRequired signal
    
    return reply;
}

bool WebDAVClient::testConnection()
{
    QEventLoop loop;
    bool success = false;
    QString errorMessage;
    
    QNetworkReply *reply = sendRequest("PROPFIND", "/");
    
    connect(reply, &QNetworkReply::finished, this, [&]() {
        if (reply->error() == QNetworkReply::NoError) {
            success = true;
            emit connectionSuccess();
        } else {
            errorMessage = reply->errorString();
            emit connectionFailed(errorMessage);
        }
        loop.quit();
    });
    
    // Timeout after 10 seconds
    QTimer::singleShot(10000, &loop, &QEventLoop::quit);
    
    loop.exec();
    
    reply->deleteLater();
    
    if (!success) {
        emit errorOccurred("Connection failed: " + errorMessage);
    }
    
    return success;
}

QList<WebDAVItem> WebDAVClient::listDirectory(const QString &path)
{
    QList<WebDAVItem> items;
    
    QEventLoop loop;
    QByteArray responseData;
    
    // PROPFIND request to list directory contents
    QByteArray body = R"(
        <?xml version="1.0" encoding="utf-8"?>
        <d:propfind xmlns:d="DAV:">
            <d:prop>
                <d:displayname/>
                <d:getcontentlength/>
                <d:getcontenttype/>
                <d:getlastmodified/>
                <d:resourcetype/>
                <d:getetag/>
            </d:prop>
        </d:propfind>
    )";
    
    QNetworkReply *reply = sendRequest("PROPFIND", path, body, 
                                        {{"Content-Type", "application/xml"}});
    
    connect(reply, &QNetworkReply::finished, this, [&]() {
        if (reply->error() == QNetworkReply::NoError) {
            responseData = reply->readAll();
        }
        loop.quit();
    });
    
    QTimer::singleShot(10000, &loop, &QEventLoop::quit);
    loop.exec();
    
    if (responseData.isEmpty()) {
        emit errorOccurred("Failed to list directory: " + reply->errorString());
        reply->deleteLater();
        return items;
    }
    
    items = parsePropfindResponse(responseData);
    reply->deleteLater();
    
    return items;
}

QList<WebDAVItem> WebDAVClient::parsePropfindResponse(const QByteArray &xml)
{
    QList<WebDAVItem> items;
    QXmlStreamReader reader(xml);
    
    WebDAVItem currentItem;
    bool inResponse = false;
    bool inPropstat = false;
    bool inProp = false;
    
    while (!reader.atEnd()) {
        reader.readNext();
        
        if (reader.isStartElement()) {
            QStringView name = reader.name();
            
            if (name == "response") {
                inResponse = true;
                currentItem = WebDAVItem();
            } else if (name == "propstat") {
                inPropstat = true;
            } else if (name == "prop") {
                inProp = true;
            } else if (inProp) {
                if (name == "href") {
                    currentItem.href = reader.readElementText();
                    currentItem.path = extractPathFromHref(currentItem.href);
                } else if (name == "displayname") {
                    currentItem.name = reader.readElementText();
                } else if (name == "getcontentlength") {
                    currentItem.contentLength = reader.readElementText().toLongLong();
                } else if (name == "getcontenttype") {
                    currentItem.contentType = reader.readElementText();
                } else if (name == "getlastmodified") {
                    currentItem.lastModified = reader.readElementText();
                } else if (name == "getetag") {
                    currentItem.etag = reader.readElementText();
                } else if (name == "resourcetype") {
                    // Check if it's a directory
                    reader.readNext();
                    if (reader.name() == "collection") {
                        currentItem.isDirectory = true;
                    }
                }
            }
        } else if (reader.isEndElement()) {
            if (reader.name() == "response") {
                inResponse = false;
                // Skip the root directory itself
                if (!currentItem.path.isEmpty() && currentItem.path != "/") {
                    // Use href to get filename if name is empty
                    if (currentItem.name.isEmpty() && !currentItem.href.isEmpty()) {
                        QUrl url(currentItem.href);
                        currentItem.name = url.fileName();
                    }
                    items.append(currentItem);
                }
            } else if (reader.name() == "prop") {
                inProp = false;
            } else if (reader.name() == "propstat") {
                inPropstat = false;
            }
        }
    }
    
    return items;
}

QString WebDAVClient::extractPathFromHref(const QString &href)
{
    QUrl url(href);
    QString path = url.path();
    
    // Remove /remote.php/dav prefix
    QString prefix = "/remote.php/dav";
    if (path.startsWith(prefix)) {
        path = path.mid(prefix.length());
    }
    
    if (path.isEmpty()) {
        path = "/";
    }
    
    return path;
}

QByteArray WebDAVClient::downloadFile(const QString &path)
{
    QEventLoop loop;
    QByteArray data;
    
    QNetworkReply *reply = sendRequest("GET", path);
    
    connect(reply, &QNetworkReply::finished, this, [&]() {
        if (reply->error() == QNetworkReply::NoError) {
            data = reply->readAll();
        } else {
            emit errorOccurred("Download failed: " + reply->errorString());
        }
        loop.quit();
    });
    
    connect(reply, &QNetworkReply::downloadProgress, this, 
            [this](qint64 received, qint64 total) {
        emit downloadProgress(received, total);
    });
    
    QTimer::singleShot(60000, &loop, &QEventLoop::quit); // 60s timeout for large files
    loop.exec();
    
    reply->deleteLater();
    return data;
}

bool WebDAVClient::downloadFileAsync(const QString &path, const QString &localPath)
{
    QEventLoop loop;
    bool success = false;
    
    // Create directory if it doesn't exist
    QFileInfo fileInfo(localPath);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    QFile file(localPath);
    if (!file.open(QIODevice::WriteOnly)) {
        emit errorOccurred("Cannot open file for writing: " + localPath);
        return false;
    }
    
    QNetworkReply *reply = sendRequest("GET", path);
    
    connect(reply, &QNetworkReply::readyRead, this, [&]() {
        file.write(reply->readAll());
    });
    
    connect(reply, &QNetworkReply::finished, this, [&]() {
        if (reply->error() == QNetworkReply::NoError) {
            // Write any remaining data
            file.write(reply->readAll());
            success = true;
        } else {
            emit errorOccurred("Download failed: " + reply->errorString());
        }
        file.close();
        loop.quit();
    });
    
    connect(reply, &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total) {
        emit downloadProgress(received, total);
    });
    
    QTimer::singleShot(120000, &loop, &QEventLoop::quit); // 2min timeout
    loop.exec();
    
    reply->deleteLater();
    return success;
}

QNetworkReply* WebDAVClient::streamFile(const QString &path, qint64 start, qint64 end)
{
    QNetworkRequest request = createRequest(path);
    
    // Set range for partial content (useful for streaming comic pages)
    if (start >= 0) {
        QString range = "bytes=" + QString::number(start);
        if (end > start) {
            range += "-" + QString::number(end);
        } else {
            range += "-";
        }
        request.setRawHeader("Range", range.toUtf8());
    }
    
    return m_networkManager.get(request);
}

QByteArray WebDAVClient::downloadRange(const QString &path, qint64 start, qint64 length)
{
    QEventLoop loop;
    QByteArray data;
    
    QNetworkReply *reply = streamFile(path, start, start + length - 1);
    
    connect(reply, &QNetworkReply::finished, this, [&]() {
        if (reply->error() == QNetworkReply::NoError) {
            data = reply->readAll();
        } else {
            emit errorOccurred("Range download failed: " + reply->errorString());
        }
        loop.quit();
    });
    
    QTimer::singleShot(30000, &loop, &QEventLoop::quit);
    loop.exec();
    
    reply->deleteLater();
    return data;
}

bool WebDAVClient::uploadFile(const QString &localPath, const QString &remotePath)
{
    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit errorOccurred("Cannot open file for reading: " + localPath);
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QEventLoop loop;
    bool success = false;
    
    QNetworkRequest request = createRequest(remotePath);
    request.setHeader(QNetworkRequest::ContentLengthHeader, data.size());
    
    QNetworkReply *reply = m_networkManager.put(request, data);
    
    connect(reply, &QNetworkReply::finished, this, [&]() {
        if (reply->error() == QNetworkReply::NoError) {
            success = true;
        } else {
            emit errorOccurred("Upload failed: " + reply->errorString());
        }
        loop.quit();
    });
    
    connect(reply, &QNetworkReply::uploadProgress, this,
            [this](qint64 sent, qint64 total) {
        emit uploadProgress(sent, total);
    });
    
    QTimer::singleShot(120000, &loop, &QEventLoop::quit);
    loop.exec();
    
    reply->deleteLater();
    return success;
}

bool WebDAVClient::deleteFile(const QString &path)
{
    QEventLoop loop;
    bool success = false;
    
    QNetworkReply *reply = sendRequest("DELETE", path);
    
    connect(reply, &QNetworkReply::finished, this, [&]() {
        if (reply->error() == QNetworkReply::NoError) {
            success = true;
        } else {
            emit errorOccurred("Delete failed: " + reply->errorString());
        }
        loop.quit();
    });
    
    QTimer::singleShot(10000, &loop, &QEventLoop::quit);
    loop.exec();
    
    reply->deleteLater();
    return success;
}

bool WebDAVClient::createDirectory(const QString &path)
{
    QEventLoop loop;
    bool success = false;
    
    // MKCOL is the WebDAV method for creating directories
    QNetworkReply *reply = sendRequest("MKCOL", path);
    
    connect(reply, &QNetworkReply::finished, this, [&]() {
        if (reply->error() == QNetworkReply::NoError) {
            success = true;
        } else {
            // Directory might already exist
            if (reply->error() == QNetworkReply::ProtocolFailure) {
                success = true; // Treat as success if it already exists
            } else {
                emit errorOccurred("Create directory failed: " + reply->errorString());
            }
        }
        loop.quit();
    });
    
    QTimer::singleShot(10000, &loop, &QEventLoop::quit);
    loop.exec();
    
    reply->deleteLater();
    return success;
}
