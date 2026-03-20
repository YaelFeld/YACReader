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
    
    // Create a temporary directory with renamed files
    QString tempDir = QDir::tempPath() + "/yacreader_cbz_" + QString::number(QDateTime::currentDateTime().toSecsSinceEpoch());
    QDir().mkpath(tempDir);
    
    int pageNum = 1;
    QStringList tempFiles;
    
    for (const QString &imagePath : comic.imageFiles) {
        QString ext = QFileInfo(imagePath).suffix().toLower();
        QString newName = QString("%1.%2").arg(pageNum, 3, 10, QChar('0')).arg(ext);
        QString destPath = tempDir + "/" + newName;
        
        if (QFile::copy(imagePath, destPath)) {
            tempFiles.append(destPath);
        }
        
        pageNum++;
    }
    
    if (tempFiles.isEmpty()) {
        return false;
    }
    
    // Use system zip command to create CBZ
    QProcess zipProcess;
    QStringList args;
    args << "-r" << "-j" << outputPath;
    for (const QString &file : tempFiles) {
        args.append(file);
    }
    
    zipProcess.start("zip", args);
    zipProcess.waitForFinished(30000);
    
    bool success = (zipProcess.exitCode() == 0 && QFile::exists(outputPath));
    
    // Cleanup temp files
    for (const QString &file : tempFiles) {
        QFile::remove(file);
    }
    QDir(tempDir).rmdir(tempDir);
    
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
