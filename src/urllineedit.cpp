/****************************************************************************
**
** Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies).
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the demonstration applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial Usage
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain
** additional rights. These rights are described in the Nokia Qt LGPL
** Exception version 1.0, included in the file LGPL_EXCEPTION.txt in this
** package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
** If you are unsure which license is appropriate for your use, please
** contact the sales department at http://www.qtsoftware.com/contact.
** $QT_END_LICENSE$
**
****************************************************************************/

#include "urllineedit.h"
#include "searchlineedit.h"

#include <QtCore/QEvent>

#include <QtGui/QApplication>
#include <QtGui/QCompleter>
#include <QtGui/QFocusEvent>
#include <QtGui/QHBoxLayout>
#include <QtGui/QLabel>
#include <QtGui/QLineEdit>
#include <QtGui/QPainter>
#include <QtGui/QStyle>
#include <QtGui/QStyleOptionFrameV2>

#include <QtCore/QDebug>

#ifdef QTOPIA
#include <qtopiaapplication.h>
#endif

ExLineEdit::ExLineEdit(QWidget *parent)
    : QWidget(parent)
    , m_leftWidget(0)
    , m_lineEdit(new QLineEdit(this))
    , m_clearButton(0)
{
    setFocusPolicy(m_lineEdit->focusPolicy());
    setAttribute(Qt::WA_InputMethodEnabled);
    setSizePolicy(m_lineEdit->sizePolicy());
    setBackgroundRole(m_lineEdit->backgroundRole());
    setMouseTracking(true);
    setAcceptDrops(true);
    setAttribute(Qt::WA_MacShowFocusRect, true);
    QPalette p = m_lineEdit->palette();
    setPalette(p);

    // line edit
    m_lineEdit->setFrame(false);
    m_lineEdit->setFocusProxy(this);
    m_lineEdit->setAttribute(Qt::WA_MacShowFocusRect, false);
    m_lineEdit->setStyleSheet("background:transparent");
    QPalette clearPalette = m_lineEdit->palette();
    clearPalette.setBrush(QPalette::Base, QBrush(Qt::transparent));
    m_lineEdit->setPalette(clearPalette);

    // clearButton
    m_clearButton = new ClearButton(this);
    connect(m_clearButton, SIGNAL(clicked()),
            m_lineEdit, SLOT(clear()));
    connect(m_lineEdit, SIGNAL(textChanged(const QString&)),
            m_clearButton, SLOT(textChanged(const QString&)));

#ifdef QTOPIA
    QtopiaApplication::setInputMethodHint(this, QtopiaApplication::Text);
#endif
}

void ExLineEdit::setFont(const QFont &font) {
    m_lineEdit->setFont(font);
    updateGeometries();
}

void ExLineEdit::setLeftWidget(QWidget *widget)
{
    m_leftWidget = widget;
}

QWidget *ExLineEdit::leftWidget() const
{
    return m_leftWidget;
}

void ExLineEdit::resizeEvent(QResizeEvent *event)
{
    Q_ASSERT(m_leftWidget);
    updateGeometries();
    QWidget::resizeEvent(event);
}

void ExLineEdit::updateGeometries()
{
    QStyleOptionFrameV2 panel;
    initStyleOption(&panel);
    QRect rect = style()->subElementRect(QStyle::SE_LineEditContents, &panel, this);

    int padding = 3;
    // int height = rect.height() + padding*2;
    int width = rect.width();

    // int m_leftWidgetHeight = m_leftWidget->height();
    m_leftWidget->setGeometry(rect.x() + 2,          0,
                              m_leftWidget->width(), m_leftWidget->height());

    int clearButtonWidth = this->height();
    m_lineEdit->setGeometry(m_leftWidget->x() + m_leftWidget->width(),        padding,
                            width - clearButtonWidth - m_leftWidget->width(), this->height() - padding*2);

    m_clearButton->setGeometry(this->width() - clearButtonWidth, 0,
                               clearButtonWidth, this->height());
}

void ExLineEdit::initStyleOption(QStyleOptionFrameV2 *option) const
{
    option->initFrom(this);
    option->rect = contentsRect();
    option->lineWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth, option, this);
    option->midLineWidth = 0;
    option->state |= QStyle::State_Sunken;
    if (m_lineEdit->isReadOnly())
        option->state |= QStyle::State_ReadOnly;
#ifdef QT_KEYPAD_NAVIGATION
    if (hasEditFocus())
        option->state |= QStyle::State_HasEditFocus;
#endif
    option->features = QStyleOptionFrameV2::None;
}

QSize ExLineEdit::sizeHint() const
{
    m_lineEdit->setFrame(true);
    QSize size = m_lineEdit->sizeHint();
    m_lineEdit->setFrame(false);
    size = size + QSize(3, 3);
    return size;
}

void ExLineEdit::focusInEvent(QFocusEvent *event)
{
    m_lineEdit->event(event);
    QWidget::focusInEvent(event);
}

void ExLineEdit::focusOutEvent(QFocusEvent *event)
{
    m_lineEdit->event(event);

    if (m_lineEdit->completer()) {
        connect(m_lineEdit->completer(), SIGNAL(activated(QString)),
                         m_lineEdit, SLOT(setText(QString)));
        connect(m_lineEdit->completer(), SIGNAL(highlighted(QString)),
                         m_lineEdit, SLOT(_q_completionHighlighted(QString)));
    }
    QWidget::focusOutEvent(event);
}

void ExLineEdit::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && !m_lineEdit->text().isEmpty()) {
        m_lineEdit->clear();
    }
    m_lineEdit->event(event);
    QWidget::keyPressEvent(event);
}

bool ExLineEdit::event(QEvent *event)
{
    if (event->type() == QEvent::ShortcutOverride ||
        event->type() == QEvent::InputMethod)
        m_lineEdit->event(event);
    return QWidget::event(event);
}

void ExLineEdit::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    QStyleOptionFrameV2 panel;
    initStyleOption(&panel);
    style()->drawPrimitive(QStyle::PE_PanelLineEdit, &panel, &p, this);
}
