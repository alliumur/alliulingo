#pragma once
#include <QWidget>
#include <QAbstractNativeEventFilter>
#include <QSystemTrayIcon>
#include <QPointer>
#include "network/llm_client.h"

class SelectionOverlay;
class ConfigManager;
class HelpWindow;
class ManualTranslateWindow;

class HotkeyFilter : public QWidget, public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    explicit HotkeyFilter(QSystemTrayIcon* tray, LlmClient* llm, ConfigManager& config);
    ~HotkeyFilter();

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

    void setHelpWindow(QPointer<HelpWindow>* helpWindow) { m_helpWindow = helpWindow; }

    void openManualWindow(bool toggleIfOpen = false);

private:
    QSystemTrayIcon* m_tray;
    LlmClient*       m_llm;
    ConfigManager& m_config;
    QPointer<SelectionOverlay> m_activeOverlay;
    QPointer<ManualTranslateWindow> m_manualWindow;
    QPointer<HelpWindow>* m_helpWindow = nullptr;
    static constexpr int HOTKEY_TRANSLATE_ID = 1001;
    static constexpr int HOTKEY_CLIPBOARD_ID = 1002;
    static constexpr int HOTKEY_MANUAL_ID    = 1003;
};
