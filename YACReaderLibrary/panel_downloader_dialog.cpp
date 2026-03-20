#include "panel_downloader_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QProgressBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QSettings>
#include <QStandardPaths>

PanelDownloaderDialog::PanelDownloaderDialog(QWidget *parent)
    : QDialog(parent),
      m_importer(new PanelDownloaderImporter(this))
{
    setWindowTitle(tr("Panel Downloader Import"));
    setMinimumSize(700, 500);
    
    setupUI();
    createConnections();
    loadSettings();
    
    // Connect importer signals
    connect(m_importer, &PanelDownloaderImporter::comicDetected,
            this, &PanelDownloaderDialog::onComicDetected);
    connect(m_importer, &PanelDownloaderImporter::importProgress,
            this, &PanelDownloaderDialog::onImportProgress);
    connect(m_importer, &PanelDownloaderImporter::comicImported,
            this, &PanelDownloaderDialog::onComicImported);
    connect(m_importer, &PanelDownloaderImporter::conversionCompleted,
            this, &PanelDownloaderDialog::onConversionCompleted);
    connect(m_importer, &PanelDownloaderImporter::errorOccurred,
            this, &PanelDownloaderDialog::onErrorOccurred);
}

PanelDownloaderDialog::~PanelDownloaderDialog() = default;

void PanelDownloaderDialog::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    
    // Settings group
    auto *settingsLayout = new QGridLayout();
    
    // Watch folder
    auto *watchLabel = new QLabel(tr("Download Folder:"));
    m_watchFolderEdit = new QLineEdit();
    m_watchFolderEdit->setPlaceholderText(tr("Select folder where Panel Downloader saves comics"));
    m_browseWatchButton = new QPushButton(tr("Browse..."));
    
    settingsLayout->addWidget(watchLabel, 0, 0);
    settingsLayout->addWidget(m_watchFolderEdit, 0, 1);
    settingsLayout->addWidget(m_browseWatchButton, 0, 2);
    
    // Target folder (for CBZ conversion)
    auto *targetLabel = new QLabel(tr("Library Folder:"));
    m_targetFolderEdit = new QLineEdit();
    m_targetFolderEdit->setPlaceholderText(tr("Select where to save CBZ files"));
    m_browseTargetButton = new QPushButton(tr("Browse..."));
    
    settingsLayout->addWidget(targetLabel, 1, 0);
    settingsLayout->addWidget(m_targetFolderEdit, 1, 1);
    settingsLayout->addWidget(m_browseTargetButton, 1, 2);
    
    mainLayout->addLayout(settingsLayout);
    
    // Options
    auto *optionsLayout = new QHBoxLayout();
    
    m_autoImportCheck = new QCheckBox(tr("Auto-import new downloads"));
    m_autoConvertCheck = new QCheckBox(tr("Auto-convert to CBZ"));
    m_deleteAfterImportCheck = new QCheckBox(tr("Delete original images after import"));
    
    optionsLayout->addWidget(m_autoImportCheck);
    optionsLayout->addWidget(m_autoConvertCheck);
    optionsLayout->addWidget(m_deleteAfterImportCheck);
    optionsLayout->addStretch();
    
    mainLayout->addLayout(optionsLayout);
    
    // Comics table
    auto *tableLabel = new QLabel(tr("Detected Comics:"));
    mainLayout->addWidget(tableLabel);
    
    m_comicsTable = new QTableWidget();
    m_comicsTable->setColumnCount(4);
    m_comicsTable->setHorizontalHeaderLabels({
        tr("Select"),
        tr("Name"),
        tr("Pages"),
        tr("Location")
    });
    m_comicsTable->horizontalHeader()->setStretchLastSection(true);
    m_comicsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_comicsTable->setAlternatingRowColors(true);
    
    mainLayout->addWidget(m_comicsTable);
    
    // Status and progress
    m_statusLabel = new QLabel(tr("Click 'Scan' to find comics"));
    mainLayout->addWidget(m_statusLabel);
    
    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);
    mainLayout->addWidget(m_progressBar);
    
    // Buttons
    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    m_scanButton = new QPushButton(tr("Scan"));
    m_scanButton->setDefault(true);
    
    m_importSelectedButton = new QPushButton(tr("Import Selected"));
    m_importSelectedButton->setEnabled(false);
    
    m_importAllButton = new QPushButton(tr("Import All"));
    m_importAllButton->setEnabled(false);
    
    m_closeButton = new QPushButton(tr("Close"));
    
    buttonLayout->addWidget(m_scanButton);
    buttonLayout->addWidget(m_importSelectedButton);
    buttonLayout->addWidget(m_importAllButton);
    buttonLayout->addWidget(m_closeButton);
    
    mainLayout->addLayout(buttonLayout);
}

void PanelDownloaderDialog::createConnections()
{
    connect(m_browseWatchButton, &QPushButton::clicked, 
            this, &PanelDownloaderDialog::browseWatchFolder);
    connect(m_browseTargetButton, &QPushButton::clicked,
            this, &PanelDownloaderDialog::browseTargetFolder);
    
    connect(m_scanButton, &QPushButton::clicked,
            this, &PanelDownloaderDialog::scanForComics);
    connect(m_importSelectedButton, &QPushButton::clicked,
            this, &PanelDownloaderDialog::importSelected);
    connect(m_importAllButton, &QPushButton::clicked,
            this, &PanelDownloaderDialog::importAll);
    connect(m_closeButton, &QPushButton::clicked,
            this, &QDialog::accept);
    
    connect(m_autoImportCheck, &QCheckBox::toggled,
            this, &PanelDownloaderDialog::updateUIState);
}

void PanelDownloaderDialog::loadSettings()
{
    QSettings settings;
    
    QString defaultDownloadPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    m_watchFolderEdit->setText(settings.value("paneldownloader/watchFolder", defaultDownloadPath).toString());
    m_targetFolderEdit->setText(settings.value("paneldownloader/targetFolder", m_libraryPath).toString());
    
    m_autoImportCheck->setChecked(settings.value("paneldownloader/autoImport", false).toBool());
    m_autoConvertCheck->setChecked(settings.value("paneldownloader/autoConvert", true).toBool());
    m_deleteAfterImportCheck->setChecked(settings.value("paneldownloader/deleteAfter", false).toBool());
    
    // Apply settings to importer
    m_importer->setWatchFolder(m_watchFolderEdit->text());
    m_importer->setTargetLibraryPath(m_targetFolderEdit->text());
    m_importer->setAutoImportEnabled(m_autoImportCheck->isChecked());
    m_importer->setAutoConvertToCBZ(m_autoConvertCheck->isChecked());
}

void PanelDownloaderDialog::saveSettings()
{
    QSettings settings;
    
    settings.setValue("paneldownloader/watchFolder", m_watchFolderEdit->text());
    settings.setValue("paneldownloader/targetFolder", m_targetFolderEdit->text());
    settings.setValue("paneldownloader/autoImport", m_autoImportCheck->isChecked());
    settings.setValue("paneldownloader/autoConvert", m_autoConvertCheck->isChecked());
    settings.setValue("paneldownloader/deleteAfter", m_deleteAfterImportCheck->isChecked());
}

void PanelDownloaderDialog::setLibraryPath(const QString &path)
{
    m_libraryPath = path;
    if (m_targetFolderEdit->text().isEmpty()) {
        m_targetFolderEdit->setText(path);
    }
}

void PanelDownloaderDialog::browseWatchFolder()
{
    QString path = QFileDialog::getExistingDirectory(this,
        tr("Select Download Folder"),
        m_watchFolderEdit->text());
    
    if (!path.isEmpty()) {
        m_watchFolderEdit->setText(path);
        m_importer->setWatchFolder(path);
        saveSettings();
    }
}

void PanelDownloaderDialog::browseTargetFolder()
{
    QString path = QFileDialog::getExistingDirectory(this,
        tr("Select Library Folder"),
        m_targetFolderEdit->text());
    
    if (!path.isEmpty()) {
        m_targetFolderEdit->setText(path);
        m_importer->setTargetLibraryPath(path);
        saveSettings();
    }
}

void PanelDownloaderDialog::scanForComics()
{
    m_comicsTable->setRowCount(0);
    m_detectedComics.clear();
    
    QString watchFolder = m_watchFolderEdit->text();
    if (watchFolder.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("Please select a download folder first."));
        return;
    }
    
    m_statusLabel->setText(tr("Scanning for comics..."));
    m_progressBar->setRange(0, 0);
    m_progressBar->setVisible(true);
    m_scanButton->setEnabled(false);
    
    m_importer->setWatchFolder(watchFolder);
    m_importer->setTargetLibraryPath(m_targetFolderEdit->text());
    
    m_detectedComics = m_importer->scanForComics(watchFolder);
    
    m_progressBar->setVisible(false);
    m_scanButton->setEnabled(true);
    
    if (m_detectedComics.isEmpty()) {
        m_statusLabel->setText(tr("No comics found in the selected folder."));
        m_importSelectedButton->setEnabled(false);
        m_importAllButton->setEnabled(false);
    } else {
        m_statusLabel->setText(tr("Found %1 comics.").arg(m_detectedComics.size()));
        m_importSelectedButton->setEnabled(true);
        m_importAllButton->setEnabled(true);
    }
}

void PanelDownloaderDialog::onComicDetected(const ImageSequenceComic &comic)
{
    int row = m_comicsTable->rowCount();
    m_comicsTable->insertRow(row);
    
    // Checkbox
    auto *checkItem = new QTableWidgetItem();
    checkItem->setCheckState(Qt::Checked);
    m_comicsTable->setItem(row, 0, checkItem);
    
    // Name
    m_comicsTable->setItem(row, 1, new QTableWidgetItem(comic.name));
    
    // Pages
    m_comicsTable->setItem(row, 2, new QTableWidgetItem(QString::number(comic.pageCount)));
    
    // Location
    m_comicsTable->setItem(row, 3, new QTableWidgetItem(comic.folderPath));
}

void PanelDownloaderDialog::importSelected()
{
    // TODO: Implement import of selected comics
    QMessageBox::information(this, tr("Import"), tr("Import feature will be implemented in the next step."));
}

void PanelDownloaderDialog::importAll()
{
    m_progressBar->setRange(0, m_detectedComics.size());
    m_progressBar->setVisible(true);
    m_importAllButton->setEnabled(false);
    m_importSelectedButton->setEnabled(false);
    
    for (const auto &comic : m_detectedComics) {
        m_importer->importComic(comic);
    }
    
    saveSettings();
}

void PanelDownloaderDialog::onImportProgress(int current, int total)
{
    m_progressBar->setMaximum(total);
    m_progressBar->setValue(current);
}

void PanelDownloaderDialog::onComicImported(const QString &path)
{
    m_statusLabel->setText(tr("Imported: %1").arg(path));
}

void PanelDownloaderDialog::onConversionCompleted(const QString &cbzPath)
{
    m_statusLabel->setText(tr("Created CBZ: %1").arg(cbzPath));
}

void PanelDownloaderDialog::onErrorOccurred(const QString &error)
{
    m_statusLabel->setText(tr("Error: %1").arg(error));
    m_statusLabel->setStyleSheet("color: red;");
}

void PanelDownloaderDialog::updateUIState()
{
    m_importer->setAutoImportEnabled(m_autoImportCheck->isChecked());
}
