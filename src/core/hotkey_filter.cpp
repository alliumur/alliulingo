#include "hotkey_filter.h"
#include "ui/selection_overlay.h"
#include "ui/result_popup.h"
#include "ui/prompt_info_window.h"
#include "ui/help_window.h"
#include "ui/manual_translate_window.h"
#include "core/config_manager.h"
#include <windows.h>
#include <QDebug>
#include <QGuiApplication>
#include <QScreen>
#include <QCursor>

HotkeyFilter::HotkeyFilter(QSystemTrayIcon* tray, LlmClient* llm, ConfigManager& config)
    : m_tray(tray), m_llm(llm), m_config(config)
{
    HWND hwnd = (HWND)this->winId();

    if (RegisterHotKey(hwnd, HOTKEY_TRANSLATE_ID, MOD_SHIFT | MOD_ALT | MOD_NOREPEAT, 0x58))
        qDebug() << "[HOTKEY] Shift+Alt+X (Translate) zarejestrowany.";
    else
        qCritical() << "[HOTKEY] Nie udało się zarejestrować Shift+Alt+X. WinAPI error:" << GetLastError();

    if (RegisterHotKey(hwnd, HOTKEY_CLIPBOARD_ID, MOD_SHIFT | MOD_ALT | MOD_NOREPEAT, 0x43))
        qDebug() << "[HOTKEY] Shift+Alt+C (Clipboard) zarejestrowany.";
    else
        qCritical() << "[HOTKEY] Nie udało się zarejestrować Shift+Alt+C. WinAPI error:" << GetLastError();

    if (RegisterHotKey(hwnd, HOTKEY_MANUAL_ID, MOD_SHIFT | MOD_ALT | MOD_NOREPEAT, 0x5A))
        qDebug() << "[HOTKEY] Shift+Alt+Z (Manual) zarejestrowany.";
    else
        qCritical() << "[HOTKEY] Nie udało się zarejestrować Shift+Alt+Z. WinAPI error:" << GetLastError();
}

HotkeyFilter::~HotkeyFilter()
{
    HWND hwnd = (HWND)this->winId();
    UnregisterHotKey(hwnd, HOTKEY_TRANSLATE_ID);
    UnregisterHotKey(hwnd, HOTKEY_CLIPBOARD_ID);
    UnregisterHotKey(hwnd, HOTKEY_MANUAL_ID);
}

bool HotkeyFilter::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_HOTKEY) {
        if (msg->wParam == HOTKEY_MANUAL_ID) {
            qDebug() << "[HOTKEY] Shift+Alt+Z wciśnięty.";
            openManualWindow();
            return true;
        }

        if (m_activeOverlay) {
            qDebug() << "[HOTKEY] Hotkey zignorowany — nakładka zaznaczania jest już aktywna.";
            return true;
        }

        QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
        if (!screen) screen = QGuiApplication::primaryScreen();

        ResultPopup::closeAll();
        if (m_helpWindow && *m_helpWindow) {
            (*m_helpWindow)->close();
            qInfo() << "[UI] HelpWindow zamknięte przed pokazaniem nakładki";
        }

        QPixmap fullScreen = screen->grabWindow(0);

        if (msg->wParam == HOTKEY_TRANSLATE_ID) {
            qDebug() << "[HOTKEY] Shift+Alt+X wciśnięty. Zamrażanie ekranu:" << screen->name() << screen->geometry();
            PromptInfoWindow::closeAll();
            m_activeOverlay = new SelectionOverlay(m_tray, m_llm, m_config, fullScreen, screen->geometry(), false);
            return true;
        }
        else if (msg->wParam == HOTKEY_CLIPBOARD_ID) {
            qDebug() << "[HOTKEY] Shift+Alt+C wciśnięty. Zamrażanie ekranu:" << screen->name() << screen->geometry();
            PromptInfoWindow::closeAll();
            m_activeOverlay = new SelectionOverlay(m_tray, m_llm, m_config, fullScreen, screen->geometry(), true);
            return true;
        }
    }
    return false;
}

void HotkeyFilter::openManualWindow(bool toggleIfOpen)
{
    qDebug() << "[HOTKEY] Otwieranie okna ręcznego tłumaczenia.";

    if (m_manualWindow && toggleIfOpen) {
        m_manualWindow->close();
        return;
    }

    ResultPopup::closeAll();
    PromptInfoWindow::closeAll();
    if (m_helpWindow && *m_helpWindow) {
        (*m_helpWindow)->close();
    }

    if (m_manualWindow) {
        m_manualWindow->raise();
        m_manualWindow->activateWindow();
        return;
    }

    m_manualWindow = new ManualTranslateWindow(m_llm, m_config);
    m_manualWindow->show();
}
