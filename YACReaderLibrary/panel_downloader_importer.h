#ifndef PANEL_DOWNLOADER_IMPORTER_H
#define PANEL_DOWNLOADER_IMPORTER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QFileSystemWatcher>
#include <QTimer>

/**
 * @brief Structure representing a detected image sequence comic
 */
struct ImageSequenceComic {
    QString folderPath;           // Path to the folder containing images
    QString name;                 // Comic name (derived from folder name)
    QStringList imageFiles;       // List of image files in sequence
    int pageCount;                // Number of pages
    bool isValid;                 // Whether this is a valid comic sequence
};

/**
 * @brief Importer for Panel Downloader extension output
 * 
 * This class handles importing comics downloaded by the Panel Downloader
 * browser extension, which saves comics as sequential image files:
 * - image_001.png, image_002.png, etc.
 * - Preserves reading order
 * - Can auto-convert to CBZ format
 */
class PanelDownloaderImporter : public QObject
{
    Q_OBJECT

public:
    explicit PanelDownloaderImporter(QObject *parent = nullptr);
    ~PanelDownloaderImporter() override;

    // Configuration
    void setWatchFolder(const QString &path);
    void setAutoImportEnabled(bool enabled);
    void setAutoConvertToCBZ(bool enabled);
    void setTargetLibraryPath(const QString &path);
    
    QString watchFolder() const { return m_watchFolder; }
    bool autoImportEnabled() const { return m_autoImport; }
    bool autoConvertToCBZ() const { return m_autoConvertToCBZ; }

    // Manual import
    QList<ImageSequenceComic> scanForComics(const QString &folderPath);
    bool importComic(const ImageSequenceComic &comic);
    bool convertToCBZ(const ImageSequenceComic &comic, const QString &outputPath);
    
    // Validation
    static bool isImageSequenceFolder(const QString &folderPath);
    static ImageSequenceComic analyzeFolder(const QString &folderPath);

public slots:
    void startWatching();
    void stopWatching();
    void scanAndImport();

signals:
    void comicDetected(const ImageSequenceComic &comic);
    void comicImported(const QString &path);
    void conversionCompleted(const QString &cbzPath);
    void errorOccurred(const QString &error);
    void importProgress(int current, int total);

private slots:
    void onFolderChanged(const QString &path);
    void processPendingImports();

private:
    QStringList findImageSequences(const QString &rootPath);
    bool validateImageSequence(const QStringList &files);
    QString generateComicName(const QString &folderPath);
    
    // Settings
    QString m_watchFolder;
    QString m_targetLibraryPath;
    bool m_autoImport;
    bool m_autoConvertToCBZ;
    
    // File watching
    QFileSystemWatcher *m_fileWatcher;
    QTimer *m_scanTimer;
    QStringList m_pendingImports;
    
    static const QStringList SUPPORTED_IMAGE_FORMATS;
    static const QString SEQUENCE_PATTERN;  // Regex for image_001.png pattern
};

#endif // PANEL_DOWNLOADER_IMPORTER_H
