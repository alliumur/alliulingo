#include "ui/help_window.h"
#include "core/localization_manager.h"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScreen>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPropertyAnimation>
#include <windows.h>
#include <dwmapi.h>

HelpWindow::HelpWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);

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
        DwmSetWindowAttribute(hwnd, 34, &noBorder, sizeof(noBorder));
    }

    auto* anim = new QPropertyAnimation(this, "windowOpacity", this);
    anim->setDuration(280);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void HelpWindow::setupUI()
{
    constexpr int kMargin = 10;
    constexpr int kWidth  = 500;

    setStyleSheet(R"(
        QLabel#header {
            color: #888888;
            font-size: 11px;
            font-style: italic;
            background: transparent;
            border: none;
        }
        QLabel#shortcuts {
            color: #E0E0E0;
            font-size: 14px;
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

    auto* title = new QLabel(Loc::get("help_window.title"), this);
    title->setObjectName("header");
    title->setAlignment(Qt::AlignCenter);
    title->setContentsMargins(0, 0, 0, 0);

    auto* sepTop = new QWidget(this);
    sepTop->setFixedHeight(1);
    sepTop->setStyleSheet("background: rgba(255,255,255,30);");

    auto* shortcuts = new QLabel(this);
    shortcuts->setObjectName("shortcuts");
    shortcuts->setTextFormat(Qt::RichText);
    shortcuts->setWordWrap(true);
    shortcuts->setText(
        "<table cellspacing='2' cellpadding='2' style='margin:0;'>"
        "<tr><td colspan='2' style='color:#888888; font-size:11px; padding-top:2px; padding-bottom:3px;'>"
            + Loc::get("help_window.main_functions") + "</td></tr>"
        "<tr><td style='color:#ffffff; font-weight:bold; white-space:nowrap;'>Shift + Alt + X</td>"
            "<td>" + Loc::get("help_window.hotkey.shift_alt_x") + "</td></tr>"
        "<tr><td style='color:#ffffff; font-weight:bold; white-space:nowrap;'>Shift + Alt + C</td>"
            "<td>" + Loc::get("help_window.hotkey.shift_alt_c") + "</td></tr>"
        "<tr><td style='color:#ffffff; font-weight:bold; white-space:nowrap;'>Shift + Alt + Z</td>"
            "<td>" + Loc::get("help_window.hotkey.shift_alt_z") + "</td></tr>"
        "<tr><td colspan='2' style='color:#888888; font-size:11px; padding-top:8px; padding-bottom:3px;'>"
            + Loc::get("help_window.result_window") + "</td></tr>"
        "<tr><td style='color:#ffffff; font-weight:bold; white-space:nowrap;'>" + Loc::get("help_window.key.rclick") + "</td>"
            "<td>" + Loc::get("help_window.hotkey.rclick") + "</td></tr>"
        "<tr><td style='color:#ffffff; font-weight:bold; white-space:nowrap;'>" + Loc::get("help_window.key.alt_rclick") + "</td>"
            "<td>" + Loc::get("help_window.hotkey.alt_rclick") + "</td></tr>"
        "<tr><td style='color:#ffffff; font-weight:bold; white-space:nowrap;'>" + Loc::get("help_window.key.lclick") + "</td>"
            "<td>" + Loc::get("help_window.hotkey.lclick") + "</td></tr>"
        "</table>"
    );

    auto* sepBot = new QWidget(this);
    sepBot->setFixedHeight(1);
    sepBot->setStyleSheet("background: rgba(255,255,255,30);");

    auto* hint = new QLabel(Loc::get("help_window.hint"), this);
    hint->setObjectName("hint");
    hint->setAlignment(Qt::AlignCenter);
    hint->setContentsMargins(0, 0, 0, 0);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kMargin, kMargin, kMargin, kMargin);
    layout->setSpacing(4);
    layout->addWidget(title);
    layout->addWidget(sepTop);
    layout->addWidget(shortcuts);
    layout->addWidget(sepBot);
    layout->addWidget(hint);

    setFixedWidth(kWidth);
}

void HelpWindow::paintEvent(QPaintEvent* event)
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

void HelpWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
        close();
    QWidget::keyPressEvent(event);
}

void HelpWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton)
        close();
    QWidget::mousePressEvent(event);
}
