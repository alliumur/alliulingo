#include "network/llm_client.h"
#include "core/localization_manager.h"
#include "ui/result_popup.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QDebug>

LlmClient::LlmClient(const ConfigManager& config,
                     QSystemTrayIcon*    tray,
                     PromptManager*      promptManager,
                     QObject*            parent)
    : QObject(parent), m_config(config), m_tray(tray), m_promptManager(promptManager), m_fallback(tray, this)
{}

void LlmClient::translate(const QString& ocrText, const QPoint& pos)
{
    const bool isSpecialResult = m_config.activePromptFile().startsWith("#");

    if (m_config.sourceLang() == m_config.targetLang()) {
        qInfo() << "[NETWORK] Języki źródłowy i docelowy są takie same, pomijam tłumaczenie";
        ResultPopup* popup = new ResultPopup(ocrText, ocrText, pos, m_config.engineName(),
                                             m_promptManager, const_cast<ConfigManager*>(&m_config), this,
                                             isSpecialResult);
        Q_UNUSED(popup);
        return;
    }

    if (m_config.translatorMode() == 1) {
        qInfo() << "[NETWORK] Tryb Online — pomijam LM Studio, używam FallbackTranslator.";
        m_fallback.translate(ocrText, pos, m_config.engineName(),
                             m_config.sourceLang(), m_config.targetLang());
        return;
    }

    QString systemPrompt;
    if (!m_config.skill().isEmpty()) {
        systemPrompt = m_config.processSkillTemplate(m_config.sourceLang(), m_config.targetLang());
    } else {
        systemPrompt = Loc::get("fallback.system_prompt")
                           .arg(m_config.sourceLang(), m_config.targetLang());
    }

    qInfo() << "[NETWORK] System prompt:" << systemPrompt;

    QJsonArray messages;
    messages.append(QJsonObject{{ "role", "system" }, { "content", systemPrompt }});

    QString userContent = ocrText;
    if (!m_config.userContentPrefix().isEmpty()) {
        userContent = m_config.userContentPrefix() + " " + ocrText;
        qInfo() << "[NETWORK] Dodano userContentPrefix do user content";
    }

    messages.append(QJsonObject{{ "role", "user"   }, { "content", userContent }});

    QJsonObject body;
    body["messages"]    = messages;
    body["model"]       = m_config.model();
    body["temperature"] = m_config.temperature();

    const QByteArray jsonData = QJsonDocument(body).toJson(QJsonDocument::Compact);

    QNetworkRequest request{QUrl(m_config.endpoint())};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    if (!m_config.apiKey().isEmpty()) {
        const QString authHeader = "Bearer " + m_config.apiKey();
        request.setRawHeader("Authorization", authHeader.toUtf8());
    }

    qInfo()  << "[NETWORK] Sending request to LM Studio. URL:" << m_config.endpoint();
    qDebug() << "[NETWORK] Body:" << jsonData.left(300);

    QNetworkReply* reply = m_nam.post(request, jsonData);

    connect(reply, &QNetworkReply::finished, this, [this, reply, pos, ocrText, isSpecialResult]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            qCritical() << "[NETWORK] LM Studio error:" << reply->errorString();
            const QString errorMsg = Loc::get("network.error.llm_unavailable");
            ResultPopup* popup = new ResultPopup(errorMsg, ocrText, pos, m_config.engineName(),
                                                 m_promptManager, const_cast<ConfigManager*>(&m_config), this,
                                                 isSpecialResult);
            Q_UNUSED(popup);
            return;
        }

        qInfo() << "[NETWORK] Response received: Success";

        const QByteArray    data = reply->readAll();
        const QJsonDocument doc  = QJsonDocument::fromJson(data);

        QString translation;
        if (doc.isObject()) {
            const auto choices = doc.object()["choices"].toArray();
            if (!choices.isEmpty())
                translation = choices[0].toObject()["message"]
                                  .toObject()["content"]
                                  .toString()
                                  .trimmed();
        }

        if (translation.isEmpty()) {
            qWarning() << "[NETWORK] Empty or unrecognized response body.";
            translation = "(brak odpowiedzi)";
        } else {
            qInfo() << "[NETWORK] Translation:" << translation.left(120);
        }

        ResultPopup* popup = new ResultPopup(translation, ocrText, pos, m_config.engineName(),
                                             m_promptManager, const_cast<ConfigManager*>(&m_config), this,
                                             isSpecialResult);
        Q_UNUSED(popup);
    });
}
