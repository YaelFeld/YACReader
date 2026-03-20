#ifndef PANEL_DOWNLOADER_DIALOG_H
#define PANEL_DOWNLOADER_DIALOG_H

#include <QDialog>
#include "panel_downloader_importer.h"

class QLineEdit;
class QPushButton;
class QCheckBox;
class QTableWidget;
class QLabel;
class QProgressBar;
class QListWidget;

/**
 * @brief Dialog for configuring and managing Panel Downloader imports
 * 
 * This dialog allows users to:
 * - Configure the watch folder for Panel Downloader output
 * - Preview detected comics
 * - Import selected comics
 * - Auto-convert settings
 */
class PanelDownloaderDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PanelDownloaderDialog(QWidget *parent = nullptr);
    ~PanelDownloaderDialog() override;

    void setLibraryPath(const QString &path);

public slots:
    void scanForComics();
    void importSelected();
    void importAll();
    void browseWatchFolder();
    void browseTargetFolder();

private slots:
    void onComicDetected(const ImageSequenceComic &comic);
    void onImportProgress(int current, int total);
    void onComicImported(const QString &path);
    void onConversionCompleted(const QString &cbzPath);
    void onErrorOccurred(const QString &error);
    void updateUIState();

private:
    void setupUI();
    void createConnections();
    void loadSettings();
    void saveSettings();

    // UI Components
    QLineEdit *m_watchFolderEdit;
    QLineEdit *m_targetFolderEdit;
    QPushButton *m_browseWatchButton;
    QPushButton *m_browseTargetButton;
    
    QCheckBox *m_autoImportCheck;
    QCheckBox *m_autoConvertCheck;
    QCheckBox *m_deleteAfterImportCheck;
    
    QTableWidget *m_comicsTable;
    QLabel *m_statusLabel;
    QProgressBar *m_progressBar;
    
    QPushButton *m_scanButton;
    QPushButton *m_importSelectedButton;
    QPushButton *m_importAllButton;
    QPushButton *m_closeButton;

    // Data
    PanelDownloaderImporter *m_importer;
    QList<ImageSequenceComic> m_detectedComics;
    QString m_libraryPath;
};

#endif // PANEL_DOWNLOADER_DIALOG_H
