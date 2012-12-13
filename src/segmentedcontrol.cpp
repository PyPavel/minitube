#include "segmentedcontrol.h"
#include "fontutils.h"

static const QColor borderColor = QColor(0x26, 0x26, 0x26);

class SegmentedControl::Private {
public:
    QList<QAction *> actionList;
    QAction *checkedAction;
    QAction *hoveredAction;
    QAction *pressedAction;
};

SegmentedControl::SegmentedControl (QWidget *parent)
    : QWidget(parent), d(new SegmentedControl::Private) {

    setMouseTracking(true);

    d->hoveredAction = 0;
    d->checkedAction = 0;
    d->pressedAction = 0;
}

SegmentedControl::~SegmentedControl() {
    delete d;
}

QAction *SegmentedControl::addAction(QAction *action) {
    QWidget::addAction(action);
    action->setCheckable(true);
    d->actionList.append(action);
    return action;
}

bool SegmentedControl::setCheckedAction(int index) {
    if (index < 0) {
        d->checkedAction = 0;
        return true;
    }
    QAction* newCheckedAction = d->actionList.at(index);
    return setCheckedAction(newCheckedAction);
}

bool SegmentedControl::setCheckedAction(QAction *action) {
    if (d->checkedAction == action) {
        return false;
    }
    if (d->checkedAction)
        d->checkedAction->setChecked(false);
    d->checkedAction = action;
    d->checkedAction->setChecked(true);
    update();
    return true;
}

QSize SegmentedControl::minimumSizeHint (void) const {
    int itemsWidth = calculateButtonWidth() * d->actionList.size() * 1.2;
    return(QSize(itemsWidth, QFontMetrics(font()).height() * 1.9));
}

void SegmentedControl::paintEvent (QPaintEvent * /*event*/) {
    int height = rect().height();
    int width = rect().width();

    QPainter p(this);

    QLinearGradient linearGrad(rect().topLeft(), rect().bottomLeft());
    linearGrad.setColorAt(0, borderColor);
    linearGrad.setColorAt(1, QColor(0x3c, 0x3c, 0x3c));
    p.fillRect(rect(), QBrush(linearGrad));

    // Calculate Buttons Size & Location
    const int buttonWidth = width / d->actionList.size();

    // Draw Buttons
    QRect rect(0, 0, buttonWidth, height);
    const int actionCount = d->actionList.size();
    for (int i = 0; i < actionCount; i++) {
        QAction *action = d->actionList.at(i);

        if (i + 1 == actionCount) {
            rect.setWidth(width - buttonWidth * (actionCount-1));
            drawButton(&p, rect, action);
        } else {
            drawButton(&p, rect, action);
            rect.moveLeft(rect.x() + rect.width());
        }

    }

}

void SegmentedControl::mouseMoveEvent (QMouseEvent *event) {
    QWidget::mouseMoveEvent(event);

    QAction *action = hoveredAction(event->pos());

    if (!action && d->hoveredAction) {
        d->hoveredAction = 0;
        update();
    } else if (action && action != d->hoveredAction) {
        d->hoveredAction = action;
        action->hover();
        update();

        // status tip
        QMainWindow* mainWindow = static_cast<QMainWindow*>(window());
        if (mainWindow) mainWindow->statusBar()->showMessage(action->statusTip());
    }
}

void SegmentedControl::mousePressEvent(QMouseEvent *event) {
    QWidget::mousePressEvent(event);
    if (d->hoveredAction) {
        d->pressedAction = d->hoveredAction;
        update();
    }
}

void SegmentedControl::mouseReleaseEvent(QMouseEvent *event) {
    QWidget::mouseReleaseEvent(event);
    d->pressedAction = 0;
    if (d->hoveredAction) {
        bool changed = setCheckedAction(d->hoveredAction);
        if (changed) d->hoveredAction->trigger();
    }
}

void SegmentedControl::leaveEvent(QEvent *event) {
    QWidget::leaveEvent(event);
    // status tip
    QMainWindow* mainWindow = static_cast<QMainWindow*>(window());
    if (mainWindow) mainWindow->statusBar()->clearMessage();
    d->hoveredAction = 0;
    d->pressedAction = 0;
    update();
}

QAction *SegmentedControl::hoveredAction(const QPoint& pos) const {
    if (pos.y() <= 0 || pos.y() >= height())
        return 0;

    int buttonWidth = width() / d->actionList.size();
    int buttonsWidth = width();
    int buttonsX = 0;

    if (pos.x() <= buttonsX || pos.x() >= (buttonsX + buttonsWidth))
        return 0;

    int buttonIndex = (pos.x() - buttonsX) / buttonWidth;

    if (buttonIndex >= d->actionList.size())
        return 0;
    return(d->actionList[buttonIndex]);
}

int SegmentedControl::calculateButtonWidth (void) const {
    QFont smallerBoldFont = FontUtils::smallBold();
    QFontMetrics fontMetrics(smallerBoldFont);
    int tmpItemWidth, itemWidth = 0;
    foreach (QAction *action, d->actionList) {
        tmpItemWidth = fontMetrics.width(action->text());
        if (itemWidth < tmpItemWidth) itemWidth = tmpItemWidth;
    }
    return itemWidth;
}

void SegmentedControl::drawButton (QPainter *painter,
                              const QRect& rect,
                              const QAction *action) {
    if (action == d->checkedAction)
        drawSelectedButton(painter, rect, action);
    else
        drawUnselectedButton(painter, rect, action);
}

void SegmentedControl::drawUnselectedButton (QPainter *painter,
                                        const QRect& rect,
                                        const QAction *action) {
    paintButton(painter, rect, action);
}

void SegmentedControl::drawSelectedButton (QPainter *painter,
                                      const QRect& rect,
                                      const QAction *action) {
    painter->save();
    painter->translate(rect.topLeft());

    const int width = rect.width();
    const int height = rect.height();
    const int hCenter = width * .5;
    QRadialGradient gradient(hCenter, 0,
                             width,
                             hCenter, 0);
    gradient.setColorAt(1, Qt::black);
    gradient.setColorAt(0, QColor(0x33, 0x33, 0x33));
    painter->fillRect(0, 0, width, height, QBrush(gradient));

    painter->restore();
    paintButton(painter, rect, action);
}

void SegmentedControl::paintButton(QPainter *painter, const QRect& rect, const QAction *action) {
    painter->save();
    painter->translate(rect.topLeft());

    const int height = rect.height();
    const int width = rect.width();

    if (action == d->pressedAction && action != d->checkedAction) {
        const int hCenter = width * .5;
        QRadialGradient gradient(hCenter, 0,
                                 width,
                                 hCenter, 0);
        gradient.setColorAt(1, QColor(0x00, 0x00, 0x00, 0));
        gradient.setColorAt(0, QColor(0x00, 0x00, 0x00, 16));
        painter->fillRect(0, 0, width, height, QBrush(gradient));
    } else if (action == d->hoveredAction && action != d->checkedAction) {
        const int hCenter = width * .5;
        QRadialGradient gradient(hCenter, 0,
                                 width,
                                 hCenter, 0);
        gradient.setColorAt(1, QColor(0xff, 0xff, 0xff, 0));
        gradient.setColorAt(0, QColor(0xff, 0xff, 0xff, 16));
        painter->fillRect(0, 0, width, height, QBrush(gradient));
    }

    painter->setPen(borderColor);
#if defined(APP_MAC) | defined(APP_WIN)
    painter->drawRect(-1, -1, width, height);
#else
    painter->drawRect(0, 0, width, height - 1);
#endif

    painter->setFont(FontUtils::smallBold());

    // text shadow
    painter->setPen(QColor(0, 0, 0, 128));
    painter->drawText(0, -1, width, height, Qt::AlignCenter, action->text());

    painter->setPen(QPen(Qt::white, 1));
    painter->drawText(0, 0, width, height, Qt::AlignCenter, action->text());

    painter->restore();
}

