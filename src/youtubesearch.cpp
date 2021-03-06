#include "youtubesearch.h"
#include "youtubestreamreader.h"
#include "constants.h"
#include "networkaccess.h"

namespace The {
    NetworkAccess* http();
}

YouTubeSearch::YouTubeSearch() : QObject() {}

void YouTubeSearch::search(SearchParams *searchParams, int max, int skip) {
    this->abortFlag = false;

    QUrl url("http://gdata.youtube.com/feeds/api/videos/");
    url.addQueryItem("v", "2");

    url.addQueryItem("max-results", QString::number(max));
    url.addQueryItem("start-index", QString::number(skip));

    if (!searchParams->keywords().isEmpty()) {
        if (searchParams->keywords().startsWith("http://") ||
                searchParams->keywords().startsWith("https://")) {
            url.addQueryItem("q", YouTubeSearch::videoIdFromUrl(searchParams->keywords()));
        } else url.addQueryItem("q", searchParams->keywords());
    }

    if (!searchParams->author().isEmpty())
        url.addQueryItem("author", searchParams->author());

    switch (searchParams->sortBy()) {
    case SearchParams::SortByNewest:
        url.addQueryItem("orderby", "published");
        break;
    case SearchParams::SortByViewCount:
        url.addQueryItem("orderby", "viewCount");
        break;
    case SearchParams::SortByRating:
        url.addQueryItem("orderby", "rating");
        break;
    }

    switch (searchParams->duration()) {
    case SearchParams::DurationShort:
        url.addQueryItem("duration", "short");
        break;
    case SearchParams::DurationMedium:
        url.addQueryItem("duration", "medium");
        break;
    case SearchParams::DurationLong:
        url.addQueryItem("duration", "long");
        break;
    }

    switch (searchParams->time()) {
    case SearchParams::TimeToday:
        url.addQueryItem("time", "today");
        break;
    case SearchParams::TimeWeek:
        url.addQueryItem("time", "this_week");
        break;
    case SearchParams::TimeMonth:
        url.addQueryItem("time", "this_month");
        break;
    }

    switch (searchParams->quality()) {
    case SearchParams::QualityHD:
        url.addQueryItem("hd", "true");
        break;
    }

    QObject *reply = The::http()->get(url);
    connect(reply, SIGNAL(data(QByteArray)), SLOT(parseResults(QByteArray)));
    connect(reply, SIGNAL(error(QNetworkReply*)), SLOT(error(QNetworkReply*)));

}

void YouTubeSearch::error(QNetworkReply *reply) {
    emit error(reply->errorString());
}

void YouTubeSearch::parseResults(QByteArray data) {

    YouTubeStreamReader reader;
    if (!reader.read(data)) {
        qDebug() << "Error parsing XML";
    }
    videos = reader.getVideos();
    suggestions = reader.getSuggestions();

    foreach (Video *video, videos) {
        // send it to the model
        emit gotVideo(video);
    }

    foreach (Video *video, videos) {
        // preload the thumb
        if (abortFlag) return;
        video->preloadThumbnail();
    }

    emit finished(videos.size());
}

QList<Video*> YouTubeSearch::getResults() {
    return videos;
}

const QStringList & YouTubeSearch::getSuggestions() const {
    return suggestions;
}

void YouTubeSearch::abort() {
    this->abortFlag = true;
}

QString YouTubeSearch::videoIdFromUrl(QString url) {
    QRegExp re = QRegExp("^.*\\?v=([^&]+).*$");
    if (re.exactMatch(url)) {
        QString videoId = re.cap(1);
        videoId.remove('-');
        return videoId;
    }
    return QString();
}
