#include "videoareawidget.h"
#include "videomimedata.h"
#ifndef Q_WS_X11
#include "extra.h"
#endif

VideoAreaWidget::VideoAreaWidget(QWidget *parent) : QWidget(parent) {
    QBoxLayout *vLayout = new QVBoxLayout(this);
    vLayout->setMargin(0);
    vLayout->setSpacing(0);

    QPalette p = palette();
    p.setBrush(QPalette::Window, Qt::black);
    setPalette(p);
    setAutoFillBackground(true);

    // hidden message widget
    messageLabel = new QLabel(this);
    messageLabel->setOpenExternalLinks(true);
    messageLabel->setMargin(7);
    messageLabel->setBackgroundRole(QPalette::ToolTipBase);
    messageLabel->setForegroundRole(QPalette::ToolTipText);
    messageLabel->setAutoFillBackground(true);
    messageLabel->setWordWrap(true);
    messageLabel->hide();
    vLayout->addWidget(messageLabel);

    stackedLayout = new QStackedLayout();
    vLayout->addLayout(stackedLayout);

    snapshotPreview = new QLabel(this);
    stackedLayout->addWidget(snapshotPreview);
    
    setLayout(vLayout);
    setAcceptDrops(true);
    
    setMouseTracking(true);
}

void VideoAreaWidget::setVideoWidget(QWidget *videoWidget) {
    this->videoWidget = videoWidget;
    videoWidget->setMouseTracking(true);
    stackedLayout->addWidget(videoWidget);
}

void VideoAreaWidget::setLoadingWidget(LoadingWidget *loadingWidget) {
    this->loadingWidget = loadingWidget;
    stackedLayout->addWidget(loadingWidget);
}

void VideoAreaWidget::showVideo() {
    stackedLayout->setCurrentWidget(videoWidget);
}

void VideoAreaWidget::showError(QString message) {
    // loadingWidget->setError(message);
    messageLabel->setText(message);
    messageLabel->show();
    stackedLayout->setCurrentWidget(loadingWidget);
}

void VideoAreaWidget::showLoading(Video *video) {
    stackedLayout->setCurrentWidget(loadingWidget);
    this->loadingWidget->setVideo(video);
    messageLabel->hide();
    messageLabel->clear();
}

void VideoAreaWidget::showSnapshotPreview(QPixmap pixmap) {
    snapshotPreview->setPixmap(pixmap);
    stackedLayout->setCurrentWidget(snapshotPreview);
#if !defined(Q_WS_X11) && !defined(QTOPIA)
    Extra::flashInWidget(snapshotPreview);
#endif
    QTimer::singleShot(1500, this, SLOT(hideSnapshotPreview()));
}

void VideoAreaWidget::hideSnapshotPreview() {
    stackedLayout->setCurrentWidget(videoWidget);
}

void VideoAreaWidget::clear() {
    stackedLayout->setCurrentWidget(loadingWidget);
    loadingWidget->clear();
    messageLabel->hide();
    messageLabel->clear();
    snapshotPreview->clear();
}

void VideoAreaWidget::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton)
        emit doubleClicked();
}

void VideoAreaWidget::mousePressEvent(QMouseEvent *event) {
    QWidget::mousePressEvent(event);

    if(event->button() == Qt::RightButton)
        emit rightClicked();
}

#ifndef QTOPIA
void VideoAreaWidget::dragEnterEvent(QDragEnterEvent *event) {
    // qDebug() << event->mimeData()->formats();
    if (event->mimeData()->hasFormat("application/x-minitube-video")) {
        event->acceptProposedAction();
    }
}

void VideoAreaWidget::dropEvent(QDropEvent *event) {
    
    const VideoMimeData* videoMimeData = dynamic_cast<const VideoMimeData*>( event->mimeData() );
    if(!videoMimeData ) return;
    
    QList<Video*> droppedVideos = videoMimeData->videos();
    if (droppedVideos.isEmpty())
        return;
    Video *video = droppedVideos.first();
    int row = listModel->rowForVideo(video);
    if (row != -1)
        listModel->setActiveRow(row);
    event->acceptProposedAction();
}
#endif
