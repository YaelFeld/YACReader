#include "comiccontroller_v2.h"

#include "db_helper.h"
#include "yacreader_libraries.h"
#include "yacreader_http_session.h"

#include "template.h"
#include "../static.h"

#include "comic_db.h"
#include "comic.h"

#include "qnaturalsorting.h"

#include "QsLog.h"

#include <typeinfo>
#include <QTemporaryFile>
#include <QDir>

// WebDAV support
#include "webdav_client.h"

using stefanfrings::HttpRequest;
using stefanfrings::HttpResponse;

ComicControllerV2::ComicControllerV2() { }

void ComicControllerV2::service(HttpRequest &request, HttpResponse &response)
{

    QByteArray token = request.getHeader("x-request-id");
    YACReaderHttpSession *ySession = Static::yacreaderSessionStore->getYACReaderSessionHttpSession(token);

    if (ySession == nullptr) {
        response.setStatus(404, "not found");
        response.write("404 not found", true);
        return;
    }

    QString path = QUrl::fromPercentEncoding(request.getPath()).toUtf8();
    QStringList pathElements = path.split('/');
    qulonglong libraryId = pathElements.at(3).toLongLong();
    QString libraryName = DBHelper::getLibraryName(libraryId);
    qulonglong comicId = pathElements.at(5).toULongLong();

    bool remoteComic = path.endsWith("remote");

    YACReaderLibraries libraries = DBHelper::getLibraries();

    ComicDB comic = DBHelper::getComicInfo(libraryId, comicId);

    if (!comic.info.existOnDb) {
        response.setStatus(404, "Not Found");
        response.write("", true);
        return;
    }

    QString comicFilePath;
    bool isWebDAVLibrary = libraries.isWebDAVLibrary(static_cast<int>(libraryId));
    
    if (isWebDAVLibrary) {
        // For WebDAV libraries, download the comic to a temporary file
        QLOG_TRACE() << "Loading comic from WebDAV library:" << libraryId;
        
        // Get library info - we need to find the library to get WebDAV config
        auto libList = libraries.getLibraries();
        YACReaderLibrary webdavLib;
        bool found = false;
        for (const auto &lib : libList) {
            if (lib.getLegacyId() == static_cast<int>(libraryId)) {
                webdavLib = lib;
                found = true;
                break;
            }
        }
        
        if (!found) {
            response.setStatus(404, "Library not found");
            response.write("404 Library not found", true);
            return;
        }
        
        // Download comic from WebDAV
        WebDAVClient webdavClient;
        webdavClient.setServerUrl(webdavLib.getServerUrl());
        webdavClient.setCredentials(webdavLib.getUsername(), "");  // Password should be stored securely
        
        QString remotePath = webdavLib.getBasePath() + comic.path;
        QByteArray comicData = webdavClient.downloadFile(remotePath);
        
        if (comicData.isEmpty()) {
            response.setStatus(404, "Failed to download comic from WebDAV");
            response.write("404 Failed to download comic", true);
            return;
        }
        
        // Save to temporary file
        QString tempDir = QDir::tempPath() + "/yacreader_webdav/" + webdavLib.getId().toString();
        QDir().mkpath(tempDir);
        
        QFileInfo comicFileInfo(comic.path);
        QString tempFilePath = tempDir + "/" + comicFileInfo.fileName();
        
        QFile tempFile(tempFilePath);
        if (!tempFile.open(QIODevice::WriteOnly)) {
            response.setStatus(500, "Failed to create temporary file");
            response.write("500 Server Error", true);
            return;
        }
        tempFile.write(comicData);
        tempFile.close();
        
        comicFilePath = tempFilePath;
        QLOG_TRACE() << "Comic downloaded to temporary file:" << comicFilePath;
    } else {
        // Local library - use path directly
        comicFilePath = libraries.getPath(libraryId) + comic.path;
    }

    Comic *comicFile = FactoryComic::newComic(comicFilePath);

    if (comicFile != nullptr) {
        QThread *thread = nullptr;

        thread = new QThread();

        comicFile->moveToThread(thread);

        connect(comicFile, QOverload<>::of(&Comic::errorOpening), thread, &QThread::quit);
        connect(comicFile, QOverload<QString>::of(&Comic::errorOpening), thread, &QThread::quit);
        connect(comicFile, &Comic::imagesLoaded, thread, &QThread::quit);
        connect(thread, &QThread::started, comicFile, &Comic::process);
        connect(thread, &QThread::finished, thread, &QObject::deleteLater);

        comicFile->load(comicFilePath);

        if (thread != nullptr)
            thread->start();

        if (remoteComic) {
            QLOG_TRACE() << "remote comic requested";
            ySession->setCurrentRemoteComic(comic.id, comicFile);

        } else {
            QLOG_TRACE() << "comic requested";
            ySession->setCurrentComic(comic.id, comicFile);
        }

        response.setHeader("Content-Type", "text/plain; charset=utf-8");
        // TODO this field is not used by the client!
        response.write(QString("library:%1\r\n").arg(libraryName).toUtf8());
        response.write(QString("libraryId:%1\r\n").arg(libraryId).toUtf8());
        if (remoteComic) // send previous and next comics id
        {
            QList<LibraryItem *> siblings = DBHelper::getFolderComicsFromLibrary(libraryId, comic.parentId, false);

            std::sort(siblings.begin(), siblings.end(), LibraryItemSorter());

            bool found = false;
            int i;
            for (i = 0; i < siblings.length(); i++) {
                if (siblings.at(i)->id == comic.id) {
                    found = true;
                    break;
                }
            }
            if (found) {
                if (i > 0) {
                    ComicDB *previousComic = static_cast<ComicDB *>(siblings.at(i - 1));
                    response.write(QString("previousComic:%1\r\n").arg(previousComic->id).toUtf8());
                    response.write(QString("previousComicHash:%1\r\n").arg(previousComic->info.hash).toUtf8());
                }
                if (i < siblings.length() - 1) {
                    ComicDB *nextComic = static_cast<ComicDB *>(siblings.at(i + 1));
                    response.write(QString("nextComic:%1\r\n").arg(nextComic->id).toUtf8());
                    response.write(QString("nextComicHash:%1\r\n").arg(nextComic->info.hash).toUtf8());
                }
            } else {
                // ERROR
            }
            qDeleteAll(siblings);
        }
        response.write(comic.toTXT().toUtf8(), true);
    } else {
        // delete comicFile;
        response.setStatus(404, "not found");
        response.write("404 not found", true);
    }
    // response.write(t.toLatin1(),true);
}
