#include "MainWindow.h"
#include "spacer.h"
#include "constants.h"
#include "iconloader/qticonloader.h"
#include "global.h"
#include "videodefinition.h"
#include "fontutils.h"
#include "globalshortcuts.h"
#ifdef Q_WS_X11
#include "gnomeglobalshortcutbackend.h"
#endif
#ifdef QTOPIA
#include <QSoftMenuBar>
#endif
#ifdef Q_WS_MAC
#include "mac_startup.h"
#include "macfullscreen.h"
#include "macsupport.h"
#include "macutils.h"
#endif
#ifndef Q_WS_X11
#include "extra.h"
#endif
#include "downloadmanager.h"
#include "youtubesuggest.h"
#include "updatechecker.h"
#ifdef APP_DEMO
#include "demostartupview.h"
#endif
#include "temporary.h"
#ifdef APP_MAC
#include "searchlineedit_mac.h"
#else
#include "searchlineedit.h"
#endif
#include <iostream>

static MainWindow *singleton = 0;

MainWindow* MainWindow::instance() {
    if (!singleton) singleton = new MainWindow();
    return singleton;
}

MainWindow::MainWindow() :
        updateChecker(0),
        aboutView(0),
        downloadView(0),
        mediaObject(0),
        audioOutput(0),
        m_fullscreen(false) {

    singleton = this;

    // views mechanism
    history = new QStack<QWidget*>();
    views = new QStackedWidget(this);
    setCentralWidget(views);

    // views
    searchView = new SearchView(this);
    connect(searchView, SIGNAL(search(SearchParams*)), this, SLOT(showMedia(SearchParams*)));
    views->addWidget(searchView);

    mediaView = new MediaView(this);
    mediaView->setEnabled(false);
    views->addWidget(mediaView);

    // build ui
    createActions();
    createMenus();
    createToolBars();
    createStatusBar();

    initPhonon();
    mediaView->setSlider(seekSlider);
    mediaView->setMediaObject(mediaObject);

    // remove that useless menu/toolbar context menu
    this->setContextMenuPolicy(Qt::NoContextMenu);

    // mediaView init stuff thats needs actions
    mediaView->initialize();

    // event filter to block ugly toolbar tooltips
    qApp->installEventFilter(this);

    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    // restore window position
    readSettings();

    // fix stacked widget minimum size
    for (int i = 0; i < views->count(); i++) {
        QWidget* view = views->widget(i);
        if (view) view->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    }
    setMinimumWidth(0);

    // show the initial view
#ifdef APP_DEMO
        QWidget *demoStartupView = new DemoStartupView(this);
        views->addWidget(demoStartupView);
        showWidget(demoStartupView);
#else
    showSearch();
#endif

    // Global shortcuts
    GlobalShortcuts &shortcuts = GlobalShortcuts::instance();
#ifdef Q_WS_X11
    if (GnomeGlobalShortcutBackend::IsGsdAvailable())
        shortcuts.setBackend(new GnomeGlobalShortcutBackend(&shortcuts));
#endif
#ifdef Q_WS_MAC
    mac::MacSetup();
#endif
    connect(&shortcuts, SIGNAL(PlayPause()), pauseAct, SLOT(trigger()));
    connect(&shortcuts, SIGNAL(Stop()), this, SLOT(stop()));
    connect(&shortcuts, SIGNAL(Next()), skipAct, SLOT(trigger()));
    connect(&shortcuts, SIGNAL(Previous()), skipBackwardAct, SLOT(trigger()));
    // connect(&shortcuts, SIGNAL(StopAfter()), The::globalActions()->value("stopafterthis"), SLOT(toggle()));

    connect(DownloadManager::instance(), SIGNAL(statusMessageChanged(QString)),
            SLOT(updateDownloadMessage(QString)));
    connect(DownloadManager::instance(), SIGNAL(finished()),
            SLOT(downloadsFinished()));

    setAcceptDrops(true);

    mouseTimer = new QTimer(this);
    mouseTimer->setInterval(5000);
    mouseTimer->setSingleShot(true);
    connect(mouseTimer, SIGNAL(timeout()), SLOT(hideMouse()));

#ifndef QTOPIA
    QTimer::singleShot(0, this, SLOT(checkForUpdate()));
#endif
}

MainWindow::~MainWindow() {
    delete history;
}

void MainWindow::changeEvent(QEvent* event) {
#ifdef APP_MAC
    if (event->type() == QEvent::WindowStateChange) {
        The::globalActions()->value("minimize")->setEnabled(!isMinimized());
    }
#endif
    QMainWindow::changeEvent(event);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {

    if (m_fullscreen && event->type() == QEvent::MouseMove) {

        QMouseEvent *mouseEvent = static_cast<QMouseEvent*> (event);
        const int x = mouseEvent->pos().x();
        const QString className = QString(obj->metaObject()->className());
        const bool isHoveringVideo = (className == "QGLWidget") || (className == "VideoAreaWidget");

        // qDebug() << obj << mouseEvent->pos() << isHoveringVideo << mediaView->isPlaylistVisible();

        if (mediaView->isPlaylistVisible()) {
            if (isHoveringVideo && x > 5) mediaView->setPlaylistVisible(false);
        } else {
            if (isHoveringVideo && x >= 0 && x < 5) mediaView->setPlaylistVisible(true);
        }

#ifndef APP_MAC
        const int y = mouseEvent->pos().y();
        if (mainToolBar->isVisible()) {
            if (isHoveringVideo && y > 5) mainToolBar->setVisible(false);
        } else {
            if (isHoveringVideo && y >= 0 && y < 5) mainToolBar->setVisible(true);
        }
#endif

        // show the normal cursor
#ifndef QTOPIA
        unsetCursor();
#endif
        // then hide it again after a few seconds
        mouseTimer->start();

    }

    if (event->type() == QEvent::ToolTip) {
        // kill tooltips
        return true;
    }
    // standard event processing
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::createActions() {

    QMap<QString, QAction*> *actions = The::globalActions();

    stopAct = new QAction(QtIconLoader::icon("media-playback-stop"), tr("&Stop"), this);
    stopAct->setStatusTip(tr("Stop playback and go back to the search view"));
    stopAct->setShortcuts(QList<QKeySequence>() << QKeySequence(Qt::Key_Escape) << QKeySequence(Qt::Key_MediaStop));
    stopAct->setEnabled(false);
    actions->insert("stop", stopAct);
    connect(stopAct, SIGNAL(triggered()), this, SLOT(stop()));

    skipBackwardAct = new QAction(
                QtIconLoader::icon("media-skip-backward"),
                tr("P&revious"), this);
    skipBackwardAct->setStatusTip(tr("Go back to the previous track"));
    skipBackwardAct->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Left));
#if QT_VERSION >= 0x040600
    skipBackwardAct->setPriority(QAction::LowPriority);
#endif
    skipBackwardAct->setEnabled(false);
    actions->insert("previous", skipBackwardAct);
    connect(skipBackwardAct, SIGNAL(triggered()), mediaView, SLOT(skipBackward()));

    skipAct = new QAction(QtIconLoader::icon("media-skip-forward"), tr("S&kip"), this);
    skipAct->setStatusTip(tr("Skip to the next video"));
    skipAct->setShortcuts(QList<QKeySequence>() << QKeySequence(Qt::CTRL + Qt::Key_Right) << QKeySequence(Qt::Key_MediaNext));
    skipAct->setEnabled(false);
    actions->insert("skip", skipAct);
    connect(skipAct, SIGNAL(triggered()), mediaView, SLOT(skip()));

    pauseAct = new QAction(QtIconLoader::icon("media-playback-pause"), tr("&Pause"), this);
    pauseAct->setStatusTip(tr("Pause playback"));
    pauseAct->setShortcuts(QList<QKeySequence>() << QKeySequence(Qt::Key_Space) << QKeySequence(Qt::Key_MediaPlay));
    pauseAct->setEnabled(false);
    actions->insert("pause", pauseAct);
    connect(pauseAct, SIGNAL(triggered()), mediaView, SLOT(pause()));

    fullscreenAct = new QAction(QtIconLoader::icon("view-fullscreen"), tr("&Full Screen"), this);
    fullscreenAct->setStatusTip(tr("Go full screen"));
    QList<QKeySequence> fsShortcuts;
#ifdef APP_MAC
    fsShortcuts << QKeySequence(Qt::CTRL + Qt::META + Qt::Key_F);
#else
    fsShortcuts << QKeySequence(Qt::Key_F11) << QKeySequence(Qt::ALT + Qt::Key_Return);
#endif
    fullscreenAct->setShortcuts(fsShortcuts);
    fullscreenAct->setShortcutContext(Qt::ApplicationShortcut);
#if QT_VERSION >= 0x040600
    fullscreenAct->setPriority(QAction::LowPriority);
#endif
    actions->insert("fullscreen", fullscreenAct);
    connect(fullscreenAct, SIGNAL(triggered()), this, SLOT(fullscreen()));

    compactViewAct = new QAction(tr("&Compact Mode"), this);
    compactViewAct->setStatusTip(tr("Hide the playlist and the toolbar"));
#ifdef APP_MAC
    compactViewAct->setShortcut(QKeySequence(Qt::CTRL + Qt::META + Qt::Key_C));
#else
    compactViewAct->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_C));
#endif
    compactViewAct->setCheckable(true);
    compactViewAct->setChecked(false);
    compactViewAct->setEnabled(false);
    actions->insert("compactView", compactViewAct);
    connect(compactViewAct, SIGNAL(toggled(bool)), this, SLOT(compactView(bool)));

    webPageAct = new QAction(tr("Open the &YouTube Page"), this);
    webPageAct->setStatusTip(tr("Go to the YouTube video page and pause playback"));
    webPageAct->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Y));
    webPageAct->setEnabled(false);
    actions->insert("webpage", webPageAct);
    connect(webPageAct, SIGNAL(triggered()), mediaView, SLOT(openWebPage()));

    copyPageAct = new QAction(tr("Copy the YouTube &Link"), this);
    copyPageAct->setStatusTip(tr("Copy the current video YouTube link to the clipboard"));
    copyPageAct->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_L));
    copyPageAct->setEnabled(false);
    actions->insert("pagelink", copyPageAct);
    connect(copyPageAct, SIGNAL(triggered()), mediaView, SLOT(copyWebPage()));

    copyLinkAct = new QAction(tr("Copy the Video Stream &URL"), this);
    copyLinkAct->setStatusTip(tr("Copy the current video stream URL to the clipboard"));
    copyLinkAct->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_U));
    copyLinkAct->setEnabled(false);
    actions->insert("videolink", copyLinkAct);
    connect(copyLinkAct, SIGNAL(triggered()), mediaView, SLOT(copyVideoLink()));

    findVideoPartsAct = new QAction(tr("Find Video &Parts"), this);
    findVideoPartsAct->setStatusTip(tr("Find other video parts hopefully in the right order"));
    findVideoPartsAct->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_P));
    findVideoPartsAct->setEnabled(false);
    connect(findVideoPartsAct, SIGNAL(triggered()), mediaView, SLOT(findVideoParts()));
    actions->insert("findVideoParts", findVideoPartsAct);

    removeAct = new QAction(tr("&Remove"), this);
    removeAct->setStatusTip(tr("Remove the selected videos from the playlist"));
    removeAct->setShortcuts(QList<QKeySequence>() << QKeySequence("Del") << QKeySequence("Backspace"));
    removeAct->setEnabled(false);
    actions->insert("remove", removeAct);
    connect(removeAct, SIGNAL(triggered()), mediaView, SLOT(removeSelected()));

    moveUpAct = new QAction(tr("Move &Up"), this);
    moveUpAct->setStatusTip(tr("Move up the selected videos in the playlist"));
    moveUpAct->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Up));
    moveUpAct->setEnabled(false);
    actions->insert("moveUp", moveUpAct);
    connect(moveUpAct, SIGNAL(triggered()), mediaView, SLOT(moveUpSelected()));

    moveDownAct = new QAction(tr("Move &Down"), this);
    moveDownAct->setStatusTip(tr("Move down the selected videos in the playlist"));
    moveDownAct->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Down));
    moveDownAct->setEnabled(false);
    actions->insert("moveDown", moveDownAct);
    connect(moveDownAct, SIGNAL(triggered()), mediaView, SLOT(moveDownSelected()));

    clearAct = new QAction(tr("&Clear Recent Searches"), this);
    clearAct->setMenuRole(QAction::ApplicationSpecificRole);
    clearAct->setShortcuts(QList<QKeySequence>()
                           << QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_Delete)
                           << QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_Backspace));
    clearAct->setStatusTip(tr("Clear the search history. Cannot be undone."));
    clearAct->setEnabled(true);
    actions->insert("clearRecentKeywords", clearAct);
    connect(clearAct, SIGNAL(triggered()), SLOT(clearRecentKeywords()));

    quitAct = new QAction(tr("&Quit"), this);
    quitAct->setMenuRole(QAction::QuitRole);
    quitAct->setShortcut(QKeySequence(QKeySequence::Quit));
    quitAct->setStatusTip(tr("Bye"));
    actions->insert("quit", quitAct);
    connect(quitAct, SIGNAL(triggered()), SLOT(quit()));

    siteAct = new QAction(tr("&Website"), this);
    siteAct->setShortcut(QKeySequence::HelpContents);
    siteAct->setStatusTip(tr("%1 on the Web").arg(Constants::NAME));
    actions->insert("site", siteAct);
    connect(siteAct, SIGNAL(triggered()), this, SLOT(visitSite()));

#if !defined(APP_MAC) && !defined(APP_WIN)
    donateAct = new QAction(tr("Make a &Donation"), this);
    donateAct->setStatusTip(tr("Please support the continued development of %1").arg(Constants::NAME));
    actions->insert("donate", donateAct);
    connect(donateAct, SIGNAL(triggered()), this, SLOT(donate()));
#endif

    aboutAct = new QAction(tr("&About"), this);
    aboutAct->setMenuRole(QAction::AboutRole);
    aboutAct->setStatusTip(tr("Info about %1").arg(Constants::NAME));
    actions->insert("about", aboutAct);
    connect(aboutAct, SIGNAL(triggered()), this, SLOT(about()));

    // Invisible actions

    searchFocusAct = new QAction(this);
    searchFocusAct->setShortcut(QKeySequence::Find);
    searchFocusAct->setStatusTip(tr("Search"));
    actions->insert("search", searchFocusAct);
    connect(searchFocusAct, SIGNAL(triggered()), this, SLOT(searchFocus()));
    addAction(searchFocusAct);

    volumeUpAct = new QAction(this);
    volumeUpAct->setShortcuts(QList<QKeySequence>() << QKeySequence(Qt::CTRL + Qt::Key_Plus));
    actions->insert("volume-up", volumeUpAct);
    connect(volumeUpAct, SIGNAL(triggered()), this, SLOT(volumeUp()));
    addAction(volumeUpAct);

    volumeDownAct = new QAction(this);
    volumeDownAct->setShortcuts(QList<QKeySequence>() << QKeySequence(Qt::CTRL + Qt::Key_Minus));
    actions->insert("volume-down", volumeDownAct);
    connect(volumeDownAct, SIGNAL(triggered()), this, SLOT(volumeDown()));
    addAction(volumeDownAct);

    volumeMuteAct = new QAction(this);
    volumeMuteAct->setIcon(QtIconLoader::icon("audio-volume-high"));
    volumeMuteAct->setStatusTip(tr("Mute volume"));
    volumeMuteAct->setShortcuts(QList<QKeySequence>() << QKeySequence(Qt::CTRL + Qt::Key_E));
    actions->insert("volume-mute", volumeMuteAct);
    connect(volumeMuteAct, SIGNAL(triggered()), SLOT(volumeMute()));
    addAction(volumeMuteAct);

    QAction *definitionAct = new QAction(this);
    definitionAct->setIcon(QtIconLoader::icon("video-display"));
    definitionAct->setShortcuts(QList<QKeySequence>() << QKeySequence(Qt::CTRL + Qt::Key_D));
    /*
    QMenu *definitionMenu = new QMenu(this);
    foreach (QString definition, VideoDefinition::getDefinitionNames()) {
        definitionMenu->addAction(definition);
    }
    definitionAct->setMenu(definitionMenu);
    */
    actions->insert("definition", definitionAct);
    connect(definitionAct, SIGNAL(triggered()), SLOT(toggleDefinitionMode()));
    addAction(definitionAct);

    QAction *action;

    action = new QAction(QtIconLoader::icon("media-playback-start"), tr("&Manually Start Playing"), this);
    action->setStatusTip(tr("Manually start playing videos"));
    action->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_B));
    action->setCheckable(true);
    connect(action, SIGNAL(toggled(bool)), SLOT(setManualPlay(bool)));
    actions->insert("manualplay", action);

    action = new QAction(tr("&Downloads"), this);
    action->setStatusTip(tr("Show details about video downloads"));
    action->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_J));
    action->setCheckable(true);
    action->setIcon(QtIconLoader::icon("go-down"));
    action->setVisible(false);
    connect(action, SIGNAL(toggled(bool)), SLOT(toggleDownloads(bool)));
    actions->insert("downloads", action);

    action = new QAction(tr("&Download"), this);
    action->setStatusTip(tr("Download the current video"));
#ifndef APP_NO_DOWNLOADS
    action->setShortcut(QKeySequence::Save);
#endif
    action->setIcon(QtIconLoader::icon("go-down"));
    action->setEnabled(false);
#if QT_VERSION >= 0x040600
    action->setPriority(QAction::LowPriority);
#endif
    connect(action, SIGNAL(triggered()), mediaView, SLOT(downloadVideo()));
    actions->insert("download", action);

    /*
    action = new QAction(tr("&Snapshot"), this);
    action->setShortcut(QKeySequence(Qt::CTRL + Qt::ALT + Qt::Key_S));
    actions->insert("snapshot", action);
    connect(action, SIGNAL(triggered()), mediaView, SLOT(snapshot()));
    */

    QString shareTip = tr("Share the current video using %1");

    action = new QAction("&Twitter", this);
    action->setStatusTip(shareTip.arg("Twitter"));
    actions->insert("twitter", action);
    connect(action, SIGNAL(triggered()), mediaView, SLOT(shareViaTwitter()));

    action = new QAction("&Facebook", this);
    action->setStatusTip(shareTip.arg("Facebook"));
    actions->insert("facebook", action);
    connect(action, SIGNAL(triggered()), mediaView, SLOT(shareViaFacebook()));

    action = new QAction("&Buffer", this);
    action->setStatusTip(shareTip.arg("Buffer"));
    actions->insert("buffer", action);
    connect(action, SIGNAL(triggered()), mediaView, SLOT(shareViaBuffer()));

    action = new QAction(tr("&Email"), this);
    action->setStatusTip(shareTip.arg(tr("Email")));
    actions->insert("email", action);
    connect(action, SIGNAL(triggered()), mediaView, SLOT(shareViaEmail()));

    action = new QAction(tr("&Close"), this);
    action->setShortcut(QKeySequence(QKeySequence::Close));
    actions->insert("close", action);
    connect(action, SIGNAL(triggered()), SLOT(close()));

    action = new QAction(Constants::NAME, this);
    action->setShortcut(QKeySequence(Qt::CTRL + Qt::ALT + Qt::Key_1));
    actions->insert("restore", action);
    connect(action, SIGNAL(triggered()), SLOT(restore()));

    action = new QAction(QtIconLoader::icon("go-top"), tr("&Float on Top"), this);
    action->setCheckable(true);
    actions->insert("ontop", action);
    connect(action, SIGNAL(toggled(bool)), SLOT(floatOnTop(bool)));

    action = new QAction(QtIconLoader::icon("media-playback-stop"), tr("&Stop After This Video"), this);
    action->setShortcut(QKeySequence(Qt::SHIFT + Qt::Key_Escape));
    action->setCheckable(true);
    action->setEnabled(false);
    actions->insert("stopafterthis", action);
    connect(action, SIGNAL(toggled(bool)), SLOT(showStopAfterThisInStatusBar(bool)));

    action = new QAction(tr("&Report an Issue..."), this);
    actions->insert("report-issue", action);
    connect(action, SIGNAL(triggered()), SLOT(reportIssue()));

    action = new QAction(tr("&Refine Search..."), this);
    action->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_R));
    action->setCheckable(true);
    actions->insert("refine-search", action);

    // common action properties
    foreach (QAction *action, actions->values()) {

        // add actions to the MainWindow so that they work
        // when the menu is hidden
        addAction(action);

        // never autorepeat.
        // unexperienced users tend to keep keys pressed for a "long" time
        action->setAutoRepeat(false);

        // set to something more meaningful then the toolbar text
        if (!action->statusTip().isEmpty())
            action->setToolTip(action->statusTip());

        // show keyboard shortcuts in the status bar
        if (!action->shortcut().isEmpty())
            action->setStatusTip(action->statusTip() + " (" + action->shortcut().toString(QKeySequence::NativeText) + ")");

        // no icons in menus
        action->setIconVisibleInMenu(false);

    }

}

void MainWindow::createMenus() {

    QMap<QString, QMenu*> *menus = The::globalMenus();

#ifdef QTOPIA
    QMenu *bar = QSoftMenuBar::menuFor(this);
#else
    QMenuBar *bar = menuBar();
#endif

    fileMenu = bar->addMenu(tr("&Application"));
#ifdef APP_DEMO
    QAction* action = new QAction(tr("Buy %1...").arg(Constants::NAME), this);
    action->setMenuRole(QAction::ApplicationSpecificRole);
    connect(action, SIGNAL(triggered()), SLOT(buy()));
    fileMenu->addAction(action);
#endif
    fileMenu->addAction(clearAct);
#ifndef APP_MAC
    fileMenu->addSeparator();
#endif
    fileMenu->addAction(quitAct);

    QMenu* playbackMenu = bar->addMenu(tr("&Playback"));
    menus->insert("playback", playbackMenu);
    playbackMenu->addAction(pauseAct);
    playbackMenu->addAction(stopAct);
    playbackMenu->addAction(The::globalActions()->value("stopafterthis"));
    playbackMenu->addSeparator();
    playbackMenu->addAction(skipAct);
    playbackMenu->addAction(skipBackwardAct);
    playbackMenu->addSeparator();
    playbackMenu->addAction(The::globalActions()->value("manualplay"));
#ifdef APP_MAC
    MacSupport::dockMenu(playbackMenu);
#endif

    playlistMenu = bar->addMenu(tr("&Playlist"));
    menus->insert("playlist", playlistMenu);
    playlistMenu->addAction(removeAct);
    playlistMenu->addSeparator();
    playlistMenu->addAction(moveUpAct);
    playlistMenu->addAction(moveDownAct);
    playlistMenu->addSeparator();
    playlistMenu->addAction(The::globalActions()->value("refine-search"));

    QMenu* videoMenu = bar->addMenu(tr("&Video"));
    menus->insert("video", videoMenu);
    videoMenu->addAction(findVideoPartsAct);
    videoMenu->addSeparator();
    videoMenu->addAction(webPageAct);
    videoMenu->addSeparator();
#ifndef APP_NO_DOWNLOADS
    videoMenu->addAction(The::globalActions()->value("download"));
    // videoMenu->addAction(copyLinkAct);
#endif
    // videoMenu->addAction(The::globalActions()->value("snapshot"));

    QMenu* viewMenu = bar->addMenu(tr("&View"));
    menus->insert("view", viewMenu);
    viewMenu->addAction(fullscreenAct);
    viewMenu->addAction(compactViewAct);
    viewMenu->addSeparator();
    viewMenu->addAction(The::globalActions()->value("ontop"));

    QMenu* shareMenu = bar->addMenu(tr("&Share"));
    menus->insert("share", shareMenu);
    shareMenu->addAction(copyPageAct);
    shareMenu->addSeparator();
    shareMenu->addAction(The::globalActions()->value("twitter"));
    shareMenu->addAction(The::globalActions()->value("facebook"));
    shareMenu->addAction(The::globalActions()->value("buffer"));
    shareMenu->addSeparator();
    shareMenu->addAction(The::globalActions()->value("email"));

#ifdef APP_MAC
    MacSupport::windowMenu(this);
#endif

    helpMenu = bar->addMenu(tr("&Help"));
    helpMenu->addAction(siteAct);
#if !defined(APP_MAC) && !defined(APP_WIN)
    helpMenu->addAction(donateAct);
#endif
    helpMenu->addAction(The::globalActions()->value("report-issue"));
    helpMenu->addAction(aboutAct);
}

void MainWindow::createToolBars() {

    setUnifiedTitleAndToolBarOnMac(true);

    mainToolBar = new QToolBar(this);
#if QT_VERSION < 0x040600 | defined(APP_MAC)
    mainToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
#else
    mainToolBar->setToolButtonStyle(Qt::ToolButtonFollowStyle);
#endif
    mainToolBar->setFloatable(false);
    mainToolBar->setMovable(false);

#if defined(APP_MAC) | defined(APP_WIN)
    mainToolBar->setIconSize(QSize(32, 32));
#endif

#ifdef QTOPIA
    QToolBar *mainToolBar2 = new QToolBar(this);
#if QT_VERSION < 0x040600 | defined(APP_MAC)
    mainToolBar2->setToolButtonStyle(Qt::ToolButtonIconOnly);
#else
    mainToolBar2->setToolButtonStyle(Qt::ToolButtonFollowStyle);
#endif
    mainToolBar2->setFloatable(false);
    mainToolBar2->setMovable(false);
#else
    QToolBar *mainToolBar2 = mainToolBar;
#endif

    mainToolBar->addAction(stopAct);
    mainToolBar->addAction(pauseAct);
    mainToolBar->addAction(skipAct);

    bool addFullScreenAct = true;
#ifdef Q_WS_MAC
    addFullScreenAct = !mac::CanGoFullScreen(winId());
#endif
    if (addFullScreenAct) mainToolBar->addAction(fullscreenAct);

#ifndef APP_NO_DOWNLOADS
    mainToolBar->addAction(The::globalActions()->value("download"));
#endif

    mainToolBar->addWidget(new Spacer());

    QFont smallerFont = FontUtils::small();
    currentTime = new QLabel(mainToolBar);
    currentTime->setFont(smallerFont);
    mainToolBar2->addWidget(currentTime);

    mainToolBar2->addWidget(new Spacer());

    seekSlider = new Phonon::SeekSlider(this);
    seekSlider->setIconVisible(false);
    seekSlider->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    mainToolBar2->addWidget(seekSlider);

/*
    mainToolBar->addWidget(new Spacer());
    slider = new QSlider(this);
    slider->setOrientation(Qt::Horizontal);
    slider->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    mainToolBar->addWidget(slider);
*/

    mainToolBar->addWidget(new Spacer());

    totalTime = new QLabel(mainToolBar);
    totalTime->setFont(smallerFont);
    mainToolBar2->addWidget(totalTime);

    mainToolBar2->addWidget(new Spacer());

    mainToolBar2->addAction(volumeMuteAct);

    volumeSlider = new Phonon::VolumeSlider(this);
    volumeSlider->setMuteVisible(false);
    // qDebug() << volumeSlider->children();
    // status tip for the volume slider
    QSlider* volumeQSlider = volumeSlider->findChild<QSlider*>();
    if (volumeQSlider)
        volumeQSlider->setStatusTip(tr("Press %1 to raise the volume, %2 to lower it").arg(
                volumeUpAct->shortcut().toString(QKeySequence::NativeText), volumeDownAct->shortcut().toString(QKeySequence::NativeText)));
    // this makes the volume slider smaller
    volumeSlider->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    mainToolBar2->addWidget(volumeSlider);

    mainToolBar2->addWidget(new Spacer());

#ifdef APP_MAC
    SearchWrapper* searchWrapper = new SearchWrapper(this);
    toolbarSearch = searchWrapper->getSearchLineEdit();
#else
    toolbarSearch = new SearchLineEdit(this);
#endif
    toolbarSearch->setMinimumWidth(toolbarSearch->fontInfo().pixelSize()*15);
    toolbarSearch->setSuggester(new YouTubeSuggest(this));
    connect(toolbarSearch, SIGNAL(search(const QString&)), this, SLOT(startToolbarSearch(const QString&)));
    connect(toolbarSearch, SIGNAL(suggestionAccepted(const QString&)), SLOT(startToolbarSearch(const QString&)));
    toolbarSearch->setStatusTip(searchFocusAct->statusTip());
#ifdef APP_MAC
    mainToolBar->addWidget(searchWrapper);
#elif defined(QTOPIA)
    toolbarSearch->setVisible(false);
#else
    mainToolBar->addWidget(toolbarSearch);
    Spacer* spacer = new Spacer();
    // spacer->setWidth(4);
    mainToolBar->addWidget(spacer);
#endif

    addToolBar(mainToolBar);
#ifdef QTOPIA
    addToolBar(Qt::BottomToolBarArea, mainToolBar2);
#endif
}

void MainWindow::createStatusBar() {
    QToolBar* toolBar = new QToolBar(this);
    statusToolBar = toolBar;
    toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolBar->setIconSize(QSize(16, 16));
    toolBar->addAction(The::globalActions()->value("downloads"));
    toolBar->addAction(The::globalActions()->value("definition"));
    statusBar()->addPermanentWidget(toolBar);
    statusBar()->show();
}

void MainWindow::showStopAfterThisInStatusBar(bool show) {
    QAction* action = The::globalActions()->value("stopafterthis");
    showActionInStatusBar(action, show);
}

void MainWindow::showActionInStatusBar(QAction* action, bool show) {
    if (show) {
        statusToolBar->insertAction(statusToolBar->actions().first(), action);
    } else {
        statusToolBar->removeAction(action);
    }
}

void MainWindow::readSettings() {
    QSettings settings;
    if (settings.contains("geometry")) {
        restoreGeometry(settings.value("geometry").toByteArray());
#ifdef APP_MAC
    MacSupport::fixGeometry(this);
#endif
    } else {
        setGeometry(100, 100, 1000, 500);
    }
    setDefinitionMode(settings.value("definition", VideoDefinition::getDefinitionNames().first()).toString());
    audioOutput->setVolume(settings.value("volume", 1).toDouble());
    audioOutput->setMuted(settings.value("volumeMute").toBool());
    The::globalActions()->value("manualplay")->setChecked(settings.value("manualplay", false).toBool());
}

void MainWindow::writeSettings() {
    QSettings settings;

    settings.setValue("geometry", saveGeometry());
    mediaView->saveSplitterState();

    settings.setValue("volume", audioOutput->volume());
    settings.setValue("volumeMute", audioOutput->isMuted());
    settings.setValue("manualplay", The::globalActions()->value("manualplay")->isChecked());
}

void MainWindow::goBack() {
    if ( history->size() > 1 ) {
        history->pop();
        QWidget *widget = history->pop();
        showWidget(widget);
    }
}

void MainWindow::showWidget ( QWidget* widget ) {

    if (compactViewAct->isChecked())
        compactViewAct->toggle();

    setUpdatesEnabled(false);

    // call hide method on the current view
#ifndef QTOPIA
    View* oldView = dynamic_cast<View *> (views->currentWidget());
    if (oldView) {
        oldView->disappear();
        views->currentWidget()->setEnabled(false);
    }
#else
    QWidget* oldView = views->currentWidget();
    if (oldView) {
        oldView->hide();
        oldView->setEnabled(false);
    }
#endif

    // call show method on the new view
#ifndef QTOPIA
    View* newView = dynamic_cast<View *> (widget);
    if (newView) {
        widget->setEnabled(true);
        newView->appear();
        QMap<QString,QVariant> metadata = newView->metadata();
        QString windowTitle = metadata.value("title").toString();
        if (windowTitle.length())
            windowTitle += " - ";
        setWindowTitle(windowTitle + Constants::NAME);
        statusBar()->showMessage((metadata.value("description").toString()));
    }
#else
    QWidget* newView = widget;
    if (newView) {
        widget->setEnabled(true);
        widget->show();
        setWindowTitle(Constants::NAME);
/*        QMap<QString,QVariant> metadata = newView->metadata();
        QString windowTitle = metadata.value("title").toString();
        if (windowTitle.length())
            windowTitle += " - ";
        statusBar()->showMessage((metadata.value("description").toString()));*/
    }
#endif

    stopAct->setEnabled(widget == mediaView);
    compactViewAct->setEnabled(widget == mediaView);
    webPageAct->setEnabled(widget == mediaView);
    copyPageAct->setEnabled(widget == mediaView);
    copyLinkAct->setEnabled(widget == mediaView);
    findVideoPartsAct->setEnabled(widget == mediaView);
    toolbarSearch->setEnabled(widget == searchView || widget == mediaView || widget == downloadView);

    if (widget == searchView) {
        skipAct->setEnabled(false);
        The::globalActions()->value("previous")->setEnabled(false);
        The::globalActions()->value("download")->setEnabled(false);
        The::globalActions()->value("stopafterthis")->setEnabled(false);
    }

    The::globalActions()->value("twitter")->setEnabled(widget == mediaView);
    The::globalActions()->value("facebook")->setEnabled(widget == mediaView);
    The::globalActions()->value("buffer")->setEnabled(widget == mediaView);
    The::globalActions()->value("email")->setEnabled(widget == mediaView);

    aboutAct->setEnabled(widget != aboutView);
    The::globalActions()->value("downloads")->setChecked(widget == downloadView);

    // toolbar only for the mediaView
    /* mainToolBar->setVisible(
            (widget == mediaView && !compactViewAct->isChecked())
            || widget == downloadView
            ); */

    setUpdatesEnabled(true);

    QWidget *oldWidget = views->currentWidget();
    if (oldWidget)
        oldWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    views->setCurrentWidget(widget);
    widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    // adjustSize();

#ifndef Q_WS_X11
    Extra::fadeInWidget(oldWidget, widget);
#endif

    history->push(widget);
}

void MainWindow::about() {
    if (!aboutView) {
        aboutView = new AboutView(this);
        views->addWidget(aboutView);
    }
    showWidget(aboutView);
}

void MainWindow::visitSite() {
    QUrl url(Constants::WEBSITE);
    statusBar()->showMessage(QString(tr("Opening %1").arg(url.toString())));
    QDesktopServices::openUrl(url);
}

void MainWindow::donate() {
    QUrl url(QString(Constants::WEBSITE) + "#donate");
    statusBar()->showMessage(QString(tr("Opening %1").arg(url.toString())));
    QDesktopServices::openUrl(url);
}

void MainWindow::reportIssue() {
    QUrl url("http://flavio.tordini.org/forums/forum/minitube-forums/minitube-troubleshooting");
    QDesktopServices::openUrl(url);
}

void MainWindow::quit() {
#ifdef APP_MAC
    if (!confirmQuit()) {
        return;
    }
#endif
    // do not save geometry when in full screen or in compact mode
    if (!m_fullscreen && !compactViewAct->isChecked()) {
        writeSettings();
    }
    Temporary::deleteAll();
    qApp->quit();
}

void MainWindow::closeEvent(QCloseEvent *event) {
#ifdef APP_MAC
    mac::closeWindow(winId());
    event->ignore();
#else
    if (!confirmQuit()) {
        event->ignore();
        return;
    }
    QWidget::closeEvent(event);
    quit();
#endif
}

bool MainWindow::confirmQuit() {
    if (DownloadManager::instance()->activeItems() > 0) {
        QMessageBox msgBox(this);
        msgBox.setIconPixmap(QPixmap(":/images/app.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        msgBox.setText(tr("Do you want to exit %1 with a download in progress?").arg(Constants::NAME));
        msgBox.setInformativeText(tr("If you close %1 now, this download will be cancelled.").arg(Constants::NAME));
        msgBox.setModal(true);
        // make it a "sheet" on the Mac
        msgBox.setWindowModality(Qt::WindowModal);

        msgBox.addButton(tr("Close and cancel download"), QMessageBox::RejectRole);
        QPushButton *waitButton = msgBox.addButton(tr("Wait for download to finish"), QMessageBox::ActionRole);

        msgBox.exec();

        if (msgBox.clickedButton() == waitButton) {
            return false;
        }
    }
    return true;
}

void MainWindow::showSearch() {
    showWidget(searchView);
    currentTime->clear();
    totalTime->clear();
}

void MainWindow::showMedia(SearchParams *searchParams) {
    mediaView->search(searchParams);
    showWidget(mediaView);
}

void MainWindow::stateChanged(Phonon::State newState, Phonon::State /* oldState */) {

    // qDebug() << "Phonon state: " << newState;

    switch (newState) {

    case Phonon::ErrorState:
        if (mediaObject->errorType() == Phonon::FatalError) {
            // Do not display because we try to play incomplete video files and sometimes trigger this
            // We retry automatically (in MediaView) so no need to show it
            // statusBar()->showMessage(tr("Fatal error: %1").arg(mediaObject->errorString()));
        } else {
            statusBar()->showMessage(tr("Error: %1").arg(mediaObject->errorString()));
        }
        break;

         case Phonon::PlayingState:
        pauseAct->setEnabled(true);
        pauseAct->setIcon(QtIconLoader::icon("media-playback-pause"));
        pauseAct->setText(tr("&Pause"));
        pauseAct->setStatusTip(tr("Pause playback") + " (" +  pauseAct->shortcut().toString(QKeySequence::NativeText) + ")");
        // stopAct->setEnabled(true);
        break;

         case Phonon::StoppedState:
        pauseAct->setEnabled(false);
        // stopAct->setEnabled(false);
        break;

         case Phonon::PausedState:
        pauseAct->setEnabled(true);
        pauseAct->setIcon(QtIconLoader::icon("media-playback-start"));
        pauseAct->setText(tr("&Play"));
        pauseAct->setStatusTip(tr("Resume playback") + " (" +  pauseAct->shortcut().toString(QKeySequence::NativeText) + ")");
        // stopAct->setEnabled(true);
        break;

         case Phonon::BufferingState:
         case Phonon::LoadingState:
        pauseAct->setEnabled(false);
        currentTime->clear();
        totalTime->clear();
        // stopAct->setEnabled(true);
        break;

         default:
        ;
    }
}

void MainWindow::stop() {
    mediaView->stop();
    showSearch();
}

void MainWindow::resizeEvent(QResizeEvent*) {
#ifdef Q_WS_MAC
    if (mac::CanGoFullScreen(winId())) {
        bool isFullscreen = mac::IsFullScreen(winId());
        if (isFullscreen != m_fullscreen) {
            if (compactViewAct->isChecked()) {
                compactViewAct->setChecked(false);
                compactView(false);
            }
            m_fullscreen = isFullscreen;
            updateUIForFullscreen();
        }
    }
#endif
}

void MainWindow::fullscreen() {

    if (compactViewAct->isChecked())
        compactViewAct->toggle();

#ifdef Q_WS_MAC
    WId handle = winId();
    if (mac::CanGoFullScreen(handle)) {
        mainToolBar->setVisible(true);
        mac::ToggleFullScreen(handle);
        return;
    }
#endif

    m_fullscreen = !m_fullscreen;

    if (m_fullscreen) {
        // Enter full screen

        m_maximized = isMaximized();

        // save geometry now, if the user quits when in full screen
        // geometry won't be saved
        writeSettings();

#ifdef Q_WS_MAC
        MacSupport::enterFullScreen(this, views);
#else
        mainToolBar->hide();
        showFullScreen();
#endif

    } else {
        // Exit full screen

#ifdef Q_WS_MAC
        MacSupport::exitFullScreen(this, views);
#else
        mainToolBar->show();
        if (m_maximized) showMaximized();
        else showNormal();
#endif

        // Make sure the window has focus
        activateWindow();

    }

    updateUIForFullscreen();

}

void MainWindow::updateUIForFullscreen() {
    static QList<QKeySequence> fsShortcuts;
    static QString fsText;

    if (m_fullscreen) {
        fsShortcuts = fullscreenAct->shortcuts();
        fsText = fullscreenAct->text();
        fullscreenAct->setShortcuts(QList<QKeySequence>(fsShortcuts)
                                    << QKeySequence(Qt::Key_Escape));
        fullscreenAct->setText(tr("Leave &Full Screen"));
    } else {
        fullscreenAct->setShortcuts(fsShortcuts);
        fullscreenAct->setText(fsText);
    }

    // No compact view action when in full screen
    compactViewAct->setVisible(!m_fullscreen);
    compactViewAct->setChecked(false);

    // Hide anything but the video
    mediaView->setPlaylistVisible(!m_fullscreen);
    statusBar()->setVisible(!m_fullscreen);

#ifndef APP_MAC
    menuBar()->setVisible(!m_fullscreen);
#endif

    if (m_fullscreen) {
        stopAct->setShortcuts(QList<QKeySequence>() << QKeySequence(Qt::Key_MediaStop));
    } else {
        stopAct->setShortcuts(QList<QKeySequence>() << QKeySequence(Qt::Key_Escape) << QKeySequence(Qt::Key_MediaStop));
    }

#ifdef Q_WS_MAC
    MacSupport::fullScreenActions(The::globalActions()->values(), m_fullscreen);
#endif

    if (views->currentWidget() == mediaView)
        mediaView->setFocus();

    if (m_fullscreen) {
        hideMouse();
    } else {
        mouseTimer->stop();
#ifndef QTOPIA
        unsetCursor();
#endif
    }
}

void MainWindow::compactView(bool enable) {

    static QList<QKeySequence> compactShortcuts;
    static QList<QKeySequence> stopShortcuts;

    const static QString key = "compactGeometry";
    QSettings settings;

#ifndef APP_MAC
    menuBar()->setVisible(!enable);
#endif

    if (enable) {
        setMinimumSize(160, 120);
#ifdef Q_WS_MAC
        mac::RemoveFullScreenWindow(winId());
#endif
        writeSettings();

        if (settings.contains(key))
            restoreGeometry(settings.value(key).toByteArray());
        else
            resize(320, 240);

        mainToolBar->setVisible(!enable);
        mediaView->setPlaylistVisible(!enable);
        statusBar()->setVisible(!enable);

        compactShortcuts = compactViewAct->shortcuts();
        stopShortcuts = stopAct->shortcuts();

        QList<QKeySequence> newStopShortcuts(stopShortcuts);
        newStopShortcuts.removeAll(QKeySequence(Qt::Key_Escape));
        stopAct->setShortcuts(newStopShortcuts);
        compactViewAct->setShortcuts(QList<QKeySequence>(compactShortcuts) << QKeySequence(Qt::Key_Escape));

        // ensure focus does not end up to the search box
        // as it would steal the Space shortcut
        mediaView->setFocus();

    } else {
        // unset minimum size
        setMinimumSize(0, 0);
#ifdef Q_WS_MAC
        mac::SetupFullScreenWindow(winId());
#endif
        settings.setValue(key, saveGeometry());
        mainToolBar->setVisible(!enable);
        mediaView->setPlaylistVisible(!enable);
        statusBar()->setVisible(!enable);
        readSettings();

        compactViewAct->setShortcuts(compactShortcuts);
        stopAct->setShortcuts(stopShortcuts);
    }

    // auto float on top
    floatOnTop(enable);
}

void MainWindow::searchFocus() {
    toolbarSearch->selectAll();
    toolbarSearch->setFocus();
}

void MainWindow::initPhonon() {
    // Phonon initialization
    if (mediaObject) delete mediaObject;
    if (audioOutput) delete audioOutput;
    mediaObject = new Phonon::MediaObject(this);
    mediaObject->setTickInterval(100);
    connect(mediaObject, SIGNAL(stateChanged(Phonon::State, Phonon::State)),
            this, SLOT(stateChanged(Phonon::State, Phonon::State)));
    connect(mediaObject, SIGNAL(tick(qint64)), this, SLOT(tick(qint64)));
    connect(mediaObject, SIGNAL(totalTimeChanged(qint64)), this, SLOT(totalTimeChanged(qint64)));
    seekSlider->setMediaObject(mediaObject);
    audioOutput = new Phonon::AudioOutput(Phonon::VideoCategory, this);
    connect(audioOutput, SIGNAL(volumeChanged(qreal)), this, SLOT(volumeChanged(qreal)));
    connect(audioOutput, SIGNAL(mutedChanged(bool)), this, SLOT(volumeMutedChanged(bool)));
    volumeSlider->setAudioOutput(audioOutput);
    Phonon::createPath(mediaObject, audioOutput);
}

void MainWindow::tick(qint64 time) {
    if (time <= 0) {
        // the "if" is important because tick is continually called
        // and we don't want to paint the toolbar every 100ms
        if (!currentTime->text().isEmpty()) currentTime->clear();
        return;
    }

    currentTime->setText(formatTime(time));

    // remaining time
    const qint64 remainingTime = mediaObject->remainingTime();
    currentTime->setStatusTip(tr("Remaining time: %1").arg(formatTime(remainingTime)));

    /*
    slider->blockSignals(true);
    slider->setValue(time/1000);
    slider->blockSignals(false);
    */
}

void MainWindow::totalTimeChanged(qint64 time) {
    if (time <= 0) {
        totalTime->clear();
        return;
    }
    totalTime->setText(formatTime(time));

    /*
    slider->blockSignals(true);
    slider->setMaximum(time/1000);
    slider->blockSignals(false);
    */

}

QString MainWindow::formatTime(qint64 time) {
    QTime displayTime;
    displayTime = displayTime.addMSecs(time);
    QString timeString;
    // 60 * 60 * 1000 = 3600000
    if (time > 3600000)
        timeString = displayTime.toString("h:mm:ss");
    else
        timeString = displayTime.toString("m:ss");
    return timeString;
}

void MainWindow::volumeUp() {
    qreal newVolume = volumeSlider->audioOutput()->volume() + .1;
    if (newVolume > volumeSlider->maximumVolume())
        newVolume = volumeSlider->maximumVolume();
    volumeSlider->audioOutput()->setVolume(newVolume);
}

void MainWindow::volumeDown() {
    qreal newVolume = volumeSlider->audioOutput()->volume() - .1;
    if (newVolume < 0)
        newVolume = 0;
    volumeSlider->audioOutput()->setVolume(newVolume);
}

void MainWindow::volumeMute() {
    volumeSlider->audioOutput()->setMuted(!volumeSlider->audioOutput()->isMuted());
}

void MainWindow::volumeChanged(qreal newVolume) {
    // automatically unmute when volume changes
    if (volumeSlider->audioOutput()->isMuted())
        volumeSlider->audioOutput()->setMuted(false);
    statusBar()->showMessage(tr("Volume at %1%").arg((int)(newVolume*100)));
}

void MainWindow::volumeMutedChanged(bool muted) {
    if (muted) {
        volumeMuteAct->setIcon(QtIconLoader::icon("audio-volume-muted"));
        statusBar()->showMessage(tr("Volume is muted"));
    } else {
        volumeMuteAct->setIcon(QtIconLoader::icon("audio-volume-high"));
        statusBar()->showMessage(tr("Volume is unmuted"));
    }
}

void MainWindow::setDefinitionMode(QString definitionName) {
    QAction *definitionAct = The::globalActions()->value("definition");
    definitionAct->setText(definitionName);
    definitionAct->setStatusTip(tr("Maximum video definition set to %1").arg(definitionAct->text())
                                + " (" +  definitionAct->shortcut().toString(QKeySequence::NativeText) + ")");
    statusBar()->showMessage(definitionAct->statusTip());
    QSettings settings;
    settings.setValue("definition", definitionName);
}

void MainWindow::toggleDefinitionMode() {
    QSettings settings;
    QString currentDefinition = settings.value("definition").toString();
    QStringList definitionNames = VideoDefinition::getDefinitionNames();
    int currentIndex = definitionNames.indexOf(currentDefinition);
    int nextIndex = 0;
    if (currentIndex != definitionNames.size() - 1) {
        nextIndex = currentIndex + 1;
    }
    QString nextDefinition = definitionNames.at(nextIndex);
    setDefinitionMode(nextDefinition);
}

void MainWindow::showFullscreenToolbar(bool show) {
    if (!m_fullscreen) return;
    mainToolBar->setVisible(show);
}

void MainWindow::showFullscreenPlaylist(bool show) {
    if (!m_fullscreen) return;
    mediaView->setPlaylistVisible(show);
}

void MainWindow::clearRecentKeywords() {
    QSettings settings;
    settings.remove("recentKeywords");
    settings.remove("recentChannels");
    searchView->updateRecentKeywords();
    searchView->updateRecentChannels();
    statusBar()->showMessage(tr("Your privacy is now safe"));
}

void MainWindow::setManualPlay(bool enabled) {
    QSettings settings;
    settings.setValue("manualplay", QVariant::fromValue(enabled));
    showActionInStatusBar(The::globalActions()->value("manualplay"), enabled);
}

void MainWindow::updateDownloadMessage(QString message) {
    The::globalActions()->value("downloads")->setText(message);
}

void MainWindow::downloadsFinished() {
    The::globalActions()->value("downloads")->setText(tr("&Downloads"));
    statusBar()->showMessage(tr("Downloads complete"));
}

void MainWindow::toggleDownloads(bool show) {

    if (show) {
        stopAct->setShortcuts(QList<QKeySequence>() << QKeySequence(Qt::Key_MediaStop));
        The::globalActions()->value("downloads")->setShortcuts(
                QList<QKeySequence>() << QKeySequence(Qt::CTRL + Qt::Key_J)
                << QKeySequence(Qt::Key_Escape));
    } else {
        The::globalActions()->value("downloads")->setShortcuts(
                QList<QKeySequence>() << QKeySequence(Qt::CTRL + Qt::Key_J));
        stopAct->setShortcuts(QList<QKeySequence>() << QKeySequence(Qt::Key_Escape) << QKeySequence(Qt::Key_MediaStop));
    }

    if (!downloadView) {
        downloadView = new DownloadView(this);
        views->addWidget(downloadView);
    }
    if (show) showWidget(downloadView);
    else goBack();
}

void MainWindow::startToolbarSearch(QString query) {
    query = query.trimmed();

    // check for empty query
    if (query.length() == 0) {
        return;
    }

    SearchParams *searchParams = new SearchParams();
    searchParams->setKeywords(query);

    // go!
    showMedia(searchParams);
}

#ifndef QTOPIA
void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasFormat("text/uri-list")) {
        QList<QUrl> urls = event->mimeData()->urls();
        if (urls.isEmpty())
            return;
        QUrl url = urls.first();
        QString videoId = YouTubeSearch::videoIdFromUrl(url.toString());
        if (!videoId.isNull())
            event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event) {
    QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty())
        return;
    QUrl url = urls.first();
    QString videoId = YouTubeSearch::videoIdFromUrl(url.toString());
    if (!videoId.isNull()) {
        setWindowTitle(url.toString());
        SearchParams *searchParams = new SearchParams();
        searchParams->setKeywords(videoId);
        showMedia(searchParams);
    }
}
#endif

void MainWindow::checkForUpdate() {
    static const QString updateCheckKey = "updateCheck";

    // check every 24h
    QSettings settings;
    uint unixTime = QDateTime::currentDateTime().toTime_t();
    int lastCheck = settings.value(updateCheckKey).toInt();
    int secondsSinceLastCheck = unixTime - lastCheck;
    // qDebug() << "secondsSinceLastCheck" << unixTime << lastCheck << secondsSinceLastCheck;
    if (secondsSinceLastCheck < 86400) return;

    // check it out
    if (updateChecker) delete updateChecker;
    updateChecker = new UpdateChecker();
    connect(updateChecker, SIGNAL(newVersion(QString)),
            this, SLOT(gotNewVersion(QString)));
    updateChecker->checkForUpdate();
    settings.setValue(updateCheckKey, unixTime);

}

void MainWindow::gotNewVersion(QString version) {
    if (updateChecker) {
        delete updateChecker;
        updateChecker = 0;
    }

#if defined(APP_DEMO) || defined(APP_MAC_STORE) || defined(APP_USC)
    return;
#endif

    QSettings settings;
    QString checkedVersion = settings.value("checkedVersion").toString();
    if (checkedVersion == version) return;

    QMessageBox msgBox(this);
    msgBox.setIconPixmap(QPixmap(":/images/app.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    msgBox.setText(tr("%1 version %2 is now available.").arg(Constants::NAME, version));

    msgBox.setModal(true);
    // make it a "sheet" on the Mac
    msgBox.setWindowModality(Qt::WindowModal);

    QPushButton* laterButton = 0;
    QPushButton* updateButton = 0;

#if defined(APP_MAC) || defined(APP_WIN)
    msgBox.setInformativeText(
                tr("To get the updated version, download %1 again from the link you received via email and reinstall.")
                .arg(Constants::NAME)
                );
    laterButton = msgBox.addButton(tr("Remind me later"), QMessageBox::RejectRole);
    msgBox.addButton(QMessageBox::Ok);
#else
    msgBox.addButton(QMessageBox::Close);
    updateButton = msgBox.addButton(tr("Update"), QMessageBox::AcceptRole);
#endif

    msgBox.exec();

    if (msgBox.clickedButton() != laterButton) {
        settings.setValue("checkedVersion", version);
    }

    if (updateButton && msgBox.clickedButton() == updateButton) {
        QDesktopServices::openUrl(QUrl(QLatin1String(Constants::WEBSITE) + "#download"));
    }

}

void MainWindow::floatOnTop(bool onTop) {
    showActionInStatusBar(The::globalActions()->value("ontop"), onTop);
#ifdef APP_MAC
    mac::floatOnTop(winId(), onTop);
    return;
#endif
    if (onTop) {
        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
        show();
    } else {
        setWindowFlags(windowFlags() ^ Qt::WindowStaysOnTopHint);
        show();
    }
}

void MainWindow::restore() {
#ifdef APP_MAC
    mac::uncloseWindow(window()->winId());
#endif
}

void MainWindow::messageReceived(const QString &message) {
    if (message == "--toggle-playing") {
        if (pauseAct->isEnabled()) pauseAct->trigger();
    } else if (message == "--next") {
        if (skipAct->isEnabled()) skipAct->trigger();
    } else if (message == "--previous") {
        if (skipBackwardAct->isEnabled()) skipBackwardAct->trigger();
    }  else if (message.startsWith("--")) {
        MainWindow::printHelp();
    } else if (!message.isEmpty()) {
        SearchParams *searchParams = new SearchParams();
        searchParams->setKeywords(message);
        showMedia(searchParams);
    }
}

void MainWindow::hideMouse() {
#ifndef QTOPIA
    setCursor(Qt::BlankCursor);
#endif
    mediaView->setPlaylistVisible(false);
#ifndef APP_MAC
    mainToolBar->setVisible(false);
#endif
}

void MainWindow::printHelp() {
    QString msg = QString("%1 %2\n\n").arg(Constants::NAME, Constants::VERSION);
    msg += "Usage: minitube [options]\n";
    msg += "Options:\n";
    msg += "  --toggle-playing\t";
    msg += "Start or pause playback.\n";
    msg += "  --next\t\t";
    msg += "Skip to the next video.\n";
    msg += "  --previous\t\t";
    msg += "Go back to the previous video.\n";
    std::cout << msg.toLocal8Bit().data();
}
