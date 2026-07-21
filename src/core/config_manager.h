#pragma once
#include <QString>
#include <QStringList>

class ConfigManager
{
public:
    static constexpr const char* kEngineLmStudio = "AI";
    static constexpr const char* kEngineOnline   = "MyMemory";

    explicit ConfigManager(const QString& filePath);

    QString skill()          const { return m_skill; }
    QString userContentPrefix() const { return m_userContentPrefix; }
    QString endpoint()       const { return m_endpoint; }
    QString model()          const { return m_model; }
    double  temperature()    const { return m_temperature; }
    int     translatorMode() const { return m_translatorMode; }
    QString sourceLang()     const { return m_sourceLang; }
    QString targetLang()     const { return m_targetLang; }
    QString activePromptFile() const { return m_activePromptFile; }
    QString apiKey()         const { return m_apiKey; }

    QString manualSourceLang() const { return m_manualSourceLang; }
    QString manualTargetLang() const { return m_manualTargetLang; }

    QString processSkillTemplate(const QString& sourceLang, const QString& targetLang) const;

    QStringList availableOcrLanguages() const { return m_availableOcrLangs; }

    QString engineName() const;

    void setTranslatorMode(int mode);

    void setSourceLang(const QString& lang);

    void setTargetLang(const QString& lang);

    void setActivePromptFile(const QString& fileName);

    void setSkill(const QString& skill);

    void setUserContentPrefix(const QString& prefix);

    void setApiKey(const QString& apiKey);

    void setEndpoint(const QString& endpoint);

    void setModel(const QString& model);

    void setTemperature(double temperature);

    void setManualSourceLang(const QString& lang);
    void setManualTargetLang(const QString& lang);

private:
    QString m_filePath;
    QString m_skill             = "Jesteś tłumaczem.";
    QString m_userContentPrefix;
    QString m_endpoint          = "http://localhost:1234/v1/chat/completions";
    QString m_model             = "local-model";
    double  m_temperature       = 0.2;
    int     m_translatorMode = 0;
    QString m_sourceLang     = "eng";
    QString m_targetLang     = "pol";
    QString m_activePromptFile;
    QString m_apiKey;
    QStringList m_availableOcrLangs;

    QString m_manualSourceLang = "eng";
    QString m_manualTargetLang = "pol";

    void detectAvailableLanguages();
};
