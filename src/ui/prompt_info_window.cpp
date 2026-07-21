#include "ui/prompt_info_window.h"
#include "core/localization_manager.h"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QCloseEvent>
#include <QScreen>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPropertyAnimation>
#include <QVBoxLayout>
#include <QDebug>
#include <windows.h>
#include <dwmapi.h>

QList<PromptInfoWindow*> PromptInfoWindow::s_activeWindows;

PromptInfoWindow::PromptInfoWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);

    s_activeWindows.append(this);

    setupUI();

    QScreen *screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->availableGeometry();
    adjustSize();
    int x = screenGeometry.right() - width() - 20;
    int y = screenGeometry.bottom() - height() - 20;
    move(x, y);

    setWindowOpacity(0.0);
    show();

    {
        HWND hwnd = reinterpret_cast<HWND>(winId());
        const COLORREF noBorder = 0xFFFFFFFE;
        DwmSetWindowAttribute(hwnd, 34 , &noBorder, sizeof(noBorder));
    }

    auto* anim = new QPropertyAnimation(this, "windowOpacity", this);
    anim->setDuration(280);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->start(QAbstractAnimation::DeleteWhenStopped);

    qInfo() << "[UI] PromptInfoWindow shown";
}

void PromptInfoWindow::closeAll()
{
    if (s_activeWindows.isEmpty()) {
        return;
    }

    qInfo() << "[UI] Zamykanie" << s_activeWindows.size() << "aktywnych okien PromptInfoWindow";

    QList<PromptInfoWindow*> windowsToClose = s_activeWindows;
    for (PromptInfoWindow* window : windowsToClose) {
        if (window) {
            window->close();
        }
    }
}

void PromptInfoWindow::setupUI()
{
    constexpr int kMargin = 15;
    constexpr int kWidth  = 500;

    setStyleSheet(R"(
        QLabel#content {
            color: #E0E0E0;
            font-size: 13px;
            background: transparent;
            border: none;
        }
        QLabel#hint {
            color: #888888;
            font-size: 10px;
            background: transparent;
            border: none;
        }
    )");

    auto* content = new QLabel(Loc::get("menu.ai_settings.help_message"), this);
    content->setObjectName("content");
    content->setWordWrap(true);
    content->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    auto* sepBot = new QWidget(this);
    sepBot->setFixedHeight(1);
    sepBot->setStyleSheet("background: rgba(255,255,255,30);");

    auto* hint = new QLabel(Loc::get("help_window.hint"), this);
    hint->setObjectName("hint");
    hint->setAlignment(Qt::AlignCenter);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kMargin, kMargin, kMargin, kMargin);
    layout->setSpacing(8);
    layout->addWidget(content);
    layout->addWidget(sepBot);
    layout->addWidget(hint);

    setFixedWidth(kWidth);

    content->setFixedWidth(kWidth - 2 * kMargin);
    content->adjustSize();
}

void PromptInfoWindow::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.setCompositionMode(QPainter::CompositionMode_Clear);
    painter.fillRect(rect(), Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    painter.setBrush(QColor(45, 45, 45, 220));
    painter.setPen(QPen(QColor(100, 100, 100, 150), 1));
    painter.drawRoundedRect(rect(), 12, 12);
}

void PromptInfoWindow::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        close();
    }
}

void PromptInfoWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        close();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void PromptInfoWindow::closeEvent(QCloseEvent* event)
{
    s_activeWindows.removeOne(this);
    qDebug() << "[UI] PromptInfoWindow zamknięty. Pozostało aktywnych:" << s_activeWindows.size();
    QWidget::closeEvent(event);
}
