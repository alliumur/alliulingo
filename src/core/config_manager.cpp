#include "core/config_manager.h"
#include <QSettings>
#include <QDebug>
#include <QDir>
#include <QCoreApplication>

ConfigManager::ConfigManager(const QString& filePath)
    : m_filePath(filePath)
{
    QFile file(filePath);
    const bool fileExists = file.exists();

    QSettings cfg(filePath, QSettings::IniFormat);

    if (!fileExists) {
        qInfo() << "[CONFIG] Plik nie istnieje, tworzę nowy z domyślnymi wartościami:" << filePath;

        cfg.setValue("Translator/mode", 1);
        cfg.setValue("Translator/source_lang", "");
        cfg.setValue("Translator/target_lang", "");
        cfg.setValue("Translator/active_prompt_file", "Default.txt");
        cfg.sync();
    }

    m_translatorMode = cfg.value("Translator/mode",    1).toInt();
    m_sourceLang     = cfg.value("Translator/source_lang", "").toString();
    m_targetLang     = cfg.value("Translator/target_lang", "").toString();
    m_activePromptFile = cfg.value("Translator/active_prompt_file", "Default.txt").toString();

    m_manualSourceLang = cfg.value("Translator/manual_source_lang", "").toString();
    m_manualTargetLang = cfg.value("Translator/manual_target_lang", "").toString();

    qInfo() << "[CONFIG] Wczytano plik:" << filePath;
    qInfo() << "[CONFIG] Tryb tłumacza:" << (m_translatorMode == 0 ? "AI" : "MyMemory");
    qInfo() << "[CONFIG] Para językowa:" << m_sourceLang << "->" << m_targetLang;
    if (!m_activePromptFile.isEmpty())
        qInfo() << "[CONFIG] Aktywny prompt:" << m_activePromptFile;

    detectAvailableLanguages();
}

QString ConfigManager::engineName() const
{
    return m_translatorMode == 0 ? QString(kEngineLmStudio) : QString(kEngineOnline);
}

void ConfigManager::setTranslatorMode(int mode)
{
    m_translatorMode = mode;
    QSettings cfg(m_filePath, QSettings::IniFormat);
    cfg.setValue("Translator/mode", mode);
    cfg.sync();
}

void ConfigManager::setSourceLang(const QString& lang)
{
    m_sourceLang = lang;
    QSettings cfg(m_filePath, QSettings::IniFormat);
    cfg.setValue("Translator/source_lang", lang);
    cfg.sync();
    qInfo() << "[LANG] Zmieniono język źródłowy na:" << lang;
}

void ConfigManager::setTargetLang(const QString& lang)
{
    m_targetLang = lang;
    QSettings cfg(m_filePath, QSettings::IniFormat);
    cfg.setValue("Translator/target_lang", lang);
    cfg.sync();
    qInfo() << "[LANG] Zmieniono język docelowy na:" << lang;
}

void ConfigManager::setActivePromptFile(const QString& fileName)
{
    m_activePromptFile = fileName;
    QSettings cfg(m_filePath, QSettings::IniFormat);
    cfg.setValue("Translator/active_prompt_file", fileName);
    cfg.sync();
    qInfo() << "[CONFIG] Zmieniono aktywny prompt na:" << fileName;
}

void ConfigManager::setSkill(const QString& skill)
{
    m_skill = skill;
}

void ConfigManager::setUserContentPrefix(const QString& prefix)
{
    m_userContentPrefix = prefix;
}

void ConfigManager::setApiKey(const QString& apiKey)
{
    m_apiKey = apiKey;
    if (!apiKey.isEmpty()) {
        qInfo() << "[CONFIG] API key ustawiony (długość:" << apiKey.length() << "znaków)";
    }
}

void ConfigManager::setEndpoint(const QString& endpoint)
{
    m_endpoint = endpoint;
}

void ConfigManager::setModel(const QString& model)
{
    m_model = model;
}

void ConfigManager::setTemperature(double temperature)
{
    m_temperature = temperature;
}

QString ConfigManager::processSkillTemplate(const QString& sourceLang, const QString& targetLang) const
{
    QString processed = m_skill;
    processed.replace("[sLang]", sourceLang);
    processed.replace("[tLang]", targetLang);
    return processed;
}

void ConfigManager::setManualSourceLang(const QString& lang)
{
    m_manualSourceLang = lang;
    QSettings cfg(m_filePath, QSettings::IniFormat);
    cfg.setValue("Translator/manual_source_lang", lang);
    cfg.sync();
    qInfo() << "[LANG] Zmieniono język źródłowy (manual) na:" << lang;
}

void ConfigManager::setManualTargetLang(const QString& lang)
{
    m_manualTargetLang = lang;
    QSettings cfg(m_filePath, QSettings::IniFormat);
    cfg.setValue("Translator/manual_target_lang", lang);
    cfg.sync();
    qInfo() << "[LANG] Zmieniono język docelowy (manual) na:" << lang;
}

void ConfigManager::detectAvailableLanguages()
{
    const QString tessdir = QCoreApplication::applicationDirPath() + "/tessdata";
    QDir dir(tessdir);

    if (!dir.exists()) {
        qWarning() << "[LANG] Katalog tessdata nie istnieje:" << tessdir;
        m_availableOcrLangs << "eng";
        return;
    }

    QStringList filters;
    filters << "*.traineddata";
    const QFileInfoList files = dir.entryInfoList(filters, QDir::Files);

    for (const QFileInfo& fileInfo : files) {
        QString baseName = fileInfo.baseName();
        m_availableOcrLangs << baseName;
    }

    if (m_availableOcrLangs.isEmpty()) {
        qWarning() << "[LANG] Brak plików *.traineddata w" << tessdir;
        m_availableOcrLangs << "eng";
    } else {
        qInfo() << "[LANG] Wykryto pliki OCR:" << m_availableOcrLangs.join(", ");
    }

    QSettings cfg(m_filePath, QSettings::IniFormat);
    if (m_sourceLang.isEmpty() && !m_availableOcrLangs.isEmpty()) {
        m_sourceLang = m_availableOcrLangs.first();
        cfg.setValue("Translator/source_lang", m_sourceLang);
        qInfo() << "[LANG] Ustawiono domyślny język źródłowy:" << m_sourceLang;
    }
    if (m_targetLang.isEmpty() && m_availableOcrLangs.size() > 1) {
        m_targetLang = m_availableOcrLangs.at(1);
        cfg.setValue("Translator/target_lang", m_targetLang);
        qInfo() << "[LANG] Ustawiono domyślny język docelowy:" << m_targetLang;
    } else if (m_targetLang.isEmpty() && m_availableOcrLangs.size() == 1) {
        m_targetLang = m_availableOcrLangs.first();
        cfg.setValue("Translator/target_lang", m_targetLang);
        qInfo() << "[LANG] Tylko jeden język dostępny, ustawiono jako target:" << m_targetLang;
    }

    if (m_manualSourceLang.isEmpty() && !m_availableOcrLangs.isEmpty()) {
        m_manualSourceLang = m_availableOcrLangs.first();
        cfg.setValue("Translator/manual_source_lang", m_manualSourceLang);
        qInfo() << "[LANG] Ustawiono domyślny język źródłowy (manual):" << m_manualSourceLang;
    }
    if (m_manualTargetLang.isEmpty() && m_availableOcrLangs.size() > 1) {
        m_manualTargetLang = m_availableOcrLangs.at(1);
        cfg.setValue("Translator/manual_target_lang", m_manualTargetLang);
        qInfo() << "[LANG] Ustawiono domyślny język docelowy (manual):" << m_manualTargetLang;
    } else if (m_manualTargetLang.isEmpty() && m_availableOcrLangs.size() == 1) {
        m_manualTargetLang = m_availableOcrLangs.first();
        cfg.setValue("Translator/manual_target_lang", m_manualTargetLang);
        qInfo() << "[LANG] Tylko jeden język dostępny, ustawiono jako target (manual):" << m_manualTargetLang;
    }

    cfg.sync();
}
