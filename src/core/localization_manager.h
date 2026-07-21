#ifndef LOCALIZATION_MANAGER_H
#define LOCALIZATION_MANAGER_H

#include <QString>
#include <QMap>

class LocalizationManager
{
public:
    static LocalizationManager& instance();

    bool loadFromFile(const QString& filePath);
    QString get(const QString& key) const;

private:
    LocalizationManager() = default;
    ~LocalizationManager() = default;
    LocalizationManager(const LocalizationManager&) = delete;
    LocalizationManager& operator=(const LocalizationManager&) = delete;

    QMap<QString, QString> m_strings;
};

namespace Loc {
    inline QString get(const QString& key) {
        return LocalizationManager::instance().get(key);
    }
}

#endif
