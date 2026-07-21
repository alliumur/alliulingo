#include "localization_manager.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

LocalizationManager& LocalizationManager::instance()
{
    static LocalizationManager inst;
    return inst;
}

bool LocalizationManager::loadFromFile(const QString& filePath)
{
    qInfo() << "[LOC] Wczytywanie tłumaczeń z pliku:" << filePath;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCritical() << "[LOC] Nie można otworzyć pliku strings.json:" << filePath;
        qCritical() << "[LOC] Aplikacja będzie używać surowych kluczy";
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qCritical() << "[LOC] Błąd parsowania JSON:" << parseError.errorString();
        qCritical() << "[LOC] Aplikacja będzie używać surowych kluczy";
        return false;
    }

    if (!doc.isObject()) {
        qCritical() << "[LOC] Plik strings.json nie zawiera obiektu JSON";
        return false;
    }

    QJsonObject obj = doc.object();
    m_strings.clear();

    for (auto it = obj.begin(); it != obj.end(); ++it) {
        m_strings[it.key()] = it.value().toString();
    }

    qInfo() << "[LOC] Wczytano" << m_strings.size() << "tłumaczeń";
    return true;
}

QString LocalizationManager::get(const QString& key) const
{
    if (m_strings.contains(key)) {
        return m_strings[key];
    }

    return key;
}
