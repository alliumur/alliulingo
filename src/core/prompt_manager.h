#pragma once
#include <QString>
#include <QStringList>
#include <QMap>

struct PromptData
{
    QString skillContent;
    QString apiKey;
    QString userContentPrefix;
    QString endpoint;
    QString model;
    double  temperature = -1.0;
};

class PromptManager
{
public:
    explicit PromptManager();

    QStringList availablePrompts() const;

    QStringList specialPrompts() const;

    QString getSkillContent(const QString& fileName) const;

    PromptData getPromptData(const QString& fileName) const;

    QString promptsDirectory() const { return m_promptsDir; }

    void clearCache() { m_cache.clear(); m_dataCache.clear(); }

private:
    QString m_promptsDir;

    mutable QMap<QString, QString> m_cache;

    mutable QMap<QString, PromptData> m_dataCache;

    void ensureDefaultPromptExists();
};
