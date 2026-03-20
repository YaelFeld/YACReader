#include "panel_downloader_importer.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QDebug>
#include <QImage>
#include <QCryptographicHash>
#include <QDateTime>

#include <archive.h>
#include <archive_entry.h>

const QStringList PanelDownloaderImporter::SUPPORTED_IMAGE_FORMATS = {
    "*.png", "*.jpg", "*.jpeg", "*.gif", "*.bmp", "*.webp"
};

const QString PanelDownloaderImporter::SEQUENCE_PATTERN = 
    "^(?:image|page|panel|comic|slide)?_?(\\d+)\\.(png|jpg|jpeg|gif|bmp|webp)$";

PanelDownloaderImporter::PanelDownloaderImporter(QObject *parent)
    : QObject(parent),
      m_autoImport(false),
      m_autoConvertToCBZ(true),
      m_fileWatcher(nullptr),
      m_scanTimer(nullptr)
{
}

PanelDownloaderImporter::~PanelDownloaderImporter() = default;

void PanelDownloaderImporter::setWatchFolder(const QString &path)
{
    m_watchFolder = path;
    if (m_fileWatcher && !m_watchFolder.isEmpty()) {
        m_fileWatcher->addPath(m_watchFolder);
    }
}

void PanelDownloaderImporter::setAutoImportEnabled(bool enabled)
{
    m_autoImport = enabled;
    if (enabled) {
        startWatching();
    } else {
        stopWatching();
    }
}

void PanelDownloaderImporter::setAutoConvertToCBZ(bool enabled)
{
    m_autoConvertToCBZ = enabled;
}

void PanelDownloaderImporter::setTargetLibraryPath(const QString &path)
{
    m_targetLibraryPath = path;
}

void PanelDownloaderImporter::startWatching()
{
    if (!m_fileWatcher) {
        m_fileWatcher = new QFileSystemWatcher(this);
        connect(m_fileWatcher, &QFileSystemWatcher::directoryChanged, 
                this, &PanelDownloaderImporter::onFolderChanged);
    }
    
    if (!m_watchFolder.isEmpty() && !m_fileWatcher->directories().contains(m_watchFolder)) {
        m_fileWatcher->addPath(m_watchFolder);
    }
    
    // Set up periodic scanning timer
    if (!m_scanTimer) {
        m_scanTimer = new QTimer(this);
        connect(m_scanTimer, &QTimer::timeout, this, &PanelDownloaderImporter::scanAndImport);
    }
    m_scanTimer->start(30000);  // Scan every 30 seconds
}

void PanelDownloaderImporter::stopWatching()
{
    if (m_fileWatcher) {
        m_fileWatcher->removePaths(m_fileWatcher->directories());
    }
    if (m_scanTimer) {
        m_scanTimer->stop();
    }
}

void PanelDownloaderImporter::scanAndImport()
{
    if (m_watchFolder.isEmpty()) {
        return;
    }
    
    QList<ImageSequenceComic> comics = scanForComics(m_watchFolder);
    
    emit importProgress(0, comics.size());
    int current = 0;
    
    for (const auto &comic : comics) {
        if (!comic.isValid) {
            continue;
        }
        
        emit comicDetected(comic);
        
        if (m_autoImport) {
            if (importComic(comic)) {
                emit comicImported(comic.folderPath);
            }
        }
        
        emit importProgress(++current, comics.size());
    }
}

QList<ImageSequenceComic> PanelDownloaderImporter::scanForComics(const QString &folderPath)
{
    QList<ImageSequenceComic> comics;
    QDir rootDir(folderPath);
    
    // Scan immediate subdirectories for image sequences
    QFileInfoList entries = rootDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    
    for (const auto &entry : entries) {
        if (isImageSequenceFolder(entry.absoluteFilePath())) {
            ImageSequenceComic comic = analyzeFolder(entry.absoluteFilePath());
            if (comic.isValid) {
                comics.append(comic);
            }
        }
    }
    
    // Also check if the root folder itself is a comic
    if (isImageSequenceFolder(folderPath)) {
        ImageSequenceComic comic = analyzeFolder(folderPath);
        if (comic.isValid) {
            comics.append(comic);
        }
    }
    
    return comics;
}

bool PanelDownloaderImporter::isImageSequenceFolder(const QString &folderPath)
{
    QDir dir(folderPath);
    if (!dir.exists()) {
        return false;
    }
    
    // Check for image files matching the sequence pattern
    QRegularExpression regex(SEQUENCE_PATTERN, QRegularExpression::CaseInsensitiveOption);
    
    int sequenceFileCount = 0;
    QFileInfoList files = dir.entryInfoList(QDir::Files);
    
    for (const auto &file : files) {
        QRegularExpressionMatch match = regex.match(file.fileName());
        if (match.hasMatch()) {
            sequenceFileCount++;
        }
    }
    
    // Consider it a comic if we have at least 2 sequential images
    return sequenceFileCount >= 2;
}

ImageSequenceComic PanelDownloaderImporter::analyzeFolder(const QString &folderPath)
{
    ImageSequenceComic comic;
    comic.folderPath = folderPath;
    comic.isValid = false;
    
    QDir dir(folderPath);
    if (!dir.exists()) {
        return comic;
    }
    
    // Get folder name as comic name
    comic.name = dir.dirName();
    
    // Find all image files matching the sequence pattern
    QRegularExpression regex(SEQUENCE_PATTERN, QRegularExpression::CaseInsensitiveOption);
    QFileInfoList files = dir.entryInfoList(QDir::Files);
    
    QStringList imageFiles;
    for (const auto &file : files) {
        QRegularExpressionMatch match = regex.match(file.fileName());
        if (match.hasMatch()) {
            imageFiles.append(file.absoluteFilePath());
        }
    }
    
    // Sort by sequence number
    std::sort(imageFiles.begin(), imageFiles.end(), 
        [&regex](const QString &a, const QString &b) {
            int numA = regex.match(QFileInfo(a).fileName()).captured(1).toInt();
            int numB = regex.match(QFileInfo(b).fileName()).captured(1).toInt();
            return numA < numB;
        });
    
    comic.imageFiles = imageFiles;
    comic.pageCount = imageFiles.size();
    comic.isValid = (comic.pageCount >= 2);
    
    return comic;
}

bool PanelDownloaderImporter::importComic(const ImageSequenceComic &comic)
{
    if (!comic.isValid || comic.imageFiles.isEmpty()) {
        emit errorOccurred("Invalid comic or no images found");
        return false;
    }
    
    if (m_autoConvertToCBZ && !m_targetLibraryPath.isEmpty()) {
        // Create CBZ file in library
        QString cbzName = comic.name + ".cbz";
        QString cbzPath = QDir(m_targetLibraryPath).filePath(cbzName);
        
        // If file exists, add timestamp
        if (QFile::exists(cbzPath)) {
            QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
            cbzName = comic.name + "_" + timestamp + ".cbz";
            cbzPath = QDir(m_targetLibraryPath).filePath(cbzName);
        }
        
        if (convertToCBZ(comic, cbzPath)) {
            emit conversionCompleted(cbzPath);
            
            // Optionally: remove source folder after successful conversion
            // QDir(comic.folderPath).removeRecursively();
            
            return true;
        }
    }
    
    return false;
}

bool PanelDownloaderImporter::convertToCBZ(const ImageSequenceComic &comic, const QString &outputPath)
{
    if (comic.imageFiles.isEmpty()) {
        emit errorOccurred("No images to convert");
        return false;
    }
    
    struct archive *a = archive_write_new();
    archive_write_set_format_zip(a);
    archive_write_open_filename(a, outputPath.toUtf8().constData());
    
    bool success = true;
    int pageNum = 1;
    
    for (const QString &imagePath : comic.imageFiles) {
        QFile file(imagePath);
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        
        QByteArray data = file.readAll();
        file.close();
        
        // Determine file extension
        QString ext = QFileInfo(imagePath).suffix().toLower();
        
        // Create entry name (001.jpg, 002.png, etc.)
        QString entryName = QString("%1.%2")
            .arg(pageNum, 3, 10, QChar('0'))
            .arg(ext);
        
        struct archive_entry *entry = archive_entry_new();
        archive_entry_set_pathname(entry, entryName.toUtf8().constData());
        archive_entry_set_size(entry, data.size());
        archive_entry_set_filetype(entry, AE_IFREG);
        archive_entry_set_perm(entry, 0644);
        archive_entry_set_mtime(entry, QDateTime::currentDateTime().toSecsSinceEpoch(), 0);
        
        archive_write_header(a, entry);
        archive_write_data(a, data.constData(), data.size());
        archive_entry_free(entry);
        
        pageNum++;
    }
    
    archive_write_close(a);
    archive_write_free(a);
    
    return success;
}

void PanelDownloaderImporter::onFolderChanged(const QString &path)
{
    Q_UNUSED(path)
    // Debounce rapid changes
    QTimer::singleShot(1000, this, &PanelDownloaderImporter::processPendingImports);
}

void PanelDownloaderImporter::processPendingImports()
{
    scanAndImport();
}
