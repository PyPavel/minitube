#include "downloadmanager.h"
#include "downloaditem.h"
#include "downloadmodel.h"
#include "video.h"
#include "constants.h"
#include "MainWindow.h"

static DownloadManager *downloadManagerInstance = 0;

DownloadManager::DownloadManager(QWidget *parent) :
    QObject(parent),
    downloadModel(new DownloadModel(this, this))
{ }

DownloadManager* DownloadManager::instance() {
    if (!downloadManagerInstance) downloadManagerInstance = new DownloadManager();
    return downloadManagerInstance;
}

void DownloadManager::clear() {
    qDeleteAll(items);
    items.clear();
    updateStatusMessage();
}

int DownloadManager::activeItems() {
    int num = 0;
    foreach (DownloadItem *item, items) {
        if (item->status() == Downloading || item->status() == Starting) num++;
    }
    return num;
}

DownloadItem* DownloadManager::itemForVideo(Video* video) {
    foreach (DownloadItem *item, items) {
        if (item->getVideo()->id() == video->id()) return item;
    }
    return 0;
}

void DownloadManager::addItem(Video *video) {
    // qDebug() << __FUNCTION__ << video->title();

#ifdef APP_DEMO
    if (video->duration() >= 60*4) {
        QMessageBox msgBox(MainWindow::instance());
        msgBox.setIconPixmap(QPixmap(":/images/app.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        msgBox.setText(tr("This is just the demo version of %1.").arg(Constants::NAME));
        msgBox.setInformativeText(
                    tr("It can only download videos shorter than %1 minutes so you can test the download functionality.")
                    .arg(4));
        msgBox.setModal(true);
        // make it a "sheet" on the Mac
        msgBox.setWindowModality(Qt::WindowModal);

        msgBox.addButton(tr("Continue"), QMessageBox::RejectRole);
        QPushButton *buyButton = msgBox.addButton(tr("Get the full version"), QMessageBox::ActionRole);

        msgBox.exec();

        if (msgBox.clickedButton() == buyButton) {
            QDesktopServices::openUrl(QUrl(QString(Constants::WEBSITE) + "#download"));
        }

        return;
    }
#endif

    DownloadItem *item = itemForVideo(video);
    if (item != 0) {
        if (item->status() == Failed || item->status() == Idle) {
            qDebug() << "Restarting download" << video->title();
            item->tryAgain();
        } else {
            qDebug() << "Already downloading video" << video->title();
        }
        return;
    }

    connect(video, SIGNAL(gotStreamUrl(QUrl)), SLOT(gotStreamUrl(QUrl)));
    // TODO handle signal errors
    // connect(video, SIGNAL(errorStreamUrl(QString)), SLOT(handleError(QString)));
    video->loadStreamUrl();

    // see you in gotStreamUrl()
}

void DownloadManager::gotStreamUrl(QUrl url) {

    Video *video = static_cast<Video*>(sender());
    if (!video) {
        qDebug() << "Cannot get video in" << __FUNCTION__;
        return;
    }

    video->disconnect(this);

    QString path = currentDownloadFolder();

    // TODO ensure all chars are filename compatible
    QString basename = video->title();
    basename.replace('(', '[');
    basename.replace(')', ']');
    basename.replace('/', ' ');
    basename.replace('\\', ' ');
    basename.replace('<', ' ');
    basename.replace('>', ' ');
    basename.replace(':', ' ');
    basename.replace('"', ' ');
    basename.replace('|', ' ');
    basename.replace('?', ' ');
    basename.replace('*', ' ');
    basename = basename.simplified();

    QString filename = path + "/" + basename + ".mp4";

    Video *videoCopy = video->clone();
    DownloadItem *item = new DownloadItem(videoCopy, url, filename, this);

    downloadModel->beginInsertRows(QModelIndex(), 0, 0);
    items.prepend(item);
    downloadModel->endInsertRows();

    // connect(item, SIGNAL(statusChanged()), SLOT(updateStatusMessage()));
    connect(item, SIGNAL(finished()), SLOT(itemFinished()));
    item->start();

    updateStatusMessage();
}

void DownloadManager::itemFinished() {
    if (activeItems() == 0) emit finished();
#ifdef Q_WS_MAC
    if (mac::canNotify()) {
        DownloadItem *item = static_cast<DownloadItem*>(sender());
        if (!item) {
            qDebug() << "Cannot get item in" << __FUNCTION__;
            return;
        }
        Video *video = item->getVideo();
        if (!video) return;
        QString stats = tr("%1 downloaded in %2").arg(
                    DownloadItem::formattedFilesize(item->bytesTotal()),
                    DownloadItem::formattedTime(item->totalTime(), false));
        mac::notify(tr("Download finished"), video->title(), stats);
    }
#endif
}

void DownloadManager::updateStatusMessage() {
    QString message = tr("%n Download(s)", "", items.size());
    emit statusMessageChanged(message);
}

QString DownloadManager::defaultDownloadFolder() {

#ifdef QTOPIA
    if(QDir("/media/card/Documents").exists())
        return "/media/card/Documents";
    return "/home/root/Documents";
#endif

    // download in the Movies system folder
    QString path = QDesktopServices::storageLocation(QDesktopServices::MoviesLocation);
    QDir moviesDir(path);
    if (!moviesDir.exists()) {
        // fallback to Desktop
        path = QDesktopServices::storageLocation(QDesktopServices::DesktopLocation);

        QDir desktopDir(path);
        if (!desktopDir.exists()) {
            // fallback to Home
            path = QDesktopServices::storageLocation(QDesktopServices::HomeLocation);
        }
    }
    return path;
}

QString DownloadManager::currentDownloadFolder() {
    QSettings settings;
    QString path = settings.value("downloadFolder").toString();
    if (path.isEmpty()) path = defaultDownloadFolder();
    return path;
}
