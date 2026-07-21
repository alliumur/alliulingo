#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QSystemTrayIcon>
#include "core/config_manager.h"
#include "network/fallback_translator.h"

class PromptManager;

class LlmClient : public QObject
{
    Q_OBJECT
public:
    explicit LlmClient(const ConfigManager& config,
                       QSystemTrayIcon*    tray,
                       PromptManager*      promptManager,
                       QObject*            parent = nullptr);

    void translate(const QString& ocrText, const QPoint& pos);

private:
    const ConfigManager& m_config;
    QSystemTrayIcon*     m_tray;
    PromptManager*       m_promptManager;
    QNetworkAccessManager m_nam;
    FallbackTranslator   m_fallback;
};
