#include "core/prompt_manager.h"
#include "core/localization_manager.h"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include <QDebug>
#include <QRegularExpression>

static QString generateDefaultPromptContent()
{
    QString content = "[LLM]\n";

    for (const QString& line : Loc::get("prompt.comment.endpoint").split('\n')) {
        content += "# " + line + "\n";
    }
    content += "endpoint=\n\n";

    for (const QString& line : Loc::get("prompt.comment.api_key").split('\n')) {
        content += "# " + line + "\n";
    }
    content += "api_key=\n\n";

    for (const QString& line : Loc::get("prompt.comment.model").split('\n')) {
        content += "# " + line + "\n";
    }
    content += "model=\n\n";

    for (const QString& line : Loc::get("prompt.comment.temperature").split('\n')) {
        content += "# " + line + "\n";
    }
    content += "temperature=\n\n";

    content += "# " + Loc::get("prompt.comment.translator_section") + "\n";
    content += "[Translator]\n";

    for (const QString& line : Loc::get("prompt.comment.skill").split('\n')) {
        content += "# " + line + "\n";
    }
    content += "skill=\"" + Loc::get("prompt.default.skill_value") + "\"\n\n";

    for (const QString& line : Loc::get("prompt.comment.user_content_prefix").split('\n')) {
        content += "# " + line + "\n";
    }
    content += "userContentPrefix=\"" + Loc::get("prompt.default.user_content_prefix_value") + "\"\n";

    return content;
}

PromptManager::PromptManager()
{
    m_promptsDir = QCoreApplication::applicationDirPath() + "/prompts";

    QDir dir(m_promptsDir);
    if (!dir.exists()) {
        qInfo() << "[PROMPT] Katalog prompts/ nie istnieje, tworzę:" << m_promptsDir;
        dir.mkpath(".");
    }

    ensureDefaultPromptExists();
}

void PromptManager::ensureDefaultPromptExists()
{
    const QString defaultPath = m_promptsDir + "/Default.txt";
    QFile file(defaultPath);

    if (file.exists()) {
        qDebug() << "[PROMPT] Plik Default.txt już istnieje";
        return;
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCritical() << "[PROMPT] Nie można utworzyć pliku Default.txt:" << defaultPath;
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << generateDefaultPromptContent();
    file.close();

    qInfo() << "[PROMPT] Utworzono domyślny plik promptu: Default.txt";
}

QStringList PromptManager::availablePrompts() const
{
    QDir dir(m_promptsDir);
    if (!dir.exists()) {
        qWarning() << "[PROMPT] Katalog prompts/ nie istnieje:" << m_promptsDir;
        return QStringList();
    }

    QStringList filters;
    filters << "*.txt";
    const QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Name);

    QStringList result;
    for (const QFileInfo& fileInfo : files) {
        result << fileInfo.fileName();
        qDebug() << "[PROMPT] Znaleziono profil:" << fileInfo.fileName();
    }

    if (result.isEmpty()) {
        qInfo() << "[PROMPT] Brak plików *.txt w katalogu prompts/";
    }

    return result;
}

QStringList PromptManager::specialPrompts() const
{
    QDir dir(m_promptsDir);
    if (!dir.exists()) {
        qWarning() << "[PROMPT] Katalog prompts/ nie istnieje:" << m_promptsDir;
        return QStringList();
    }

    QStringList filters;
    filters << "#*.txt";
    const QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Name);

    QStringList result;
    for (const QFileInfo& fileInfo : files) {
        QString fileName = fileInfo.fileName();
        if (fileName.startsWith("#") && fileName.endsWith(".txt")) {
            fileName = fileName.mid(1);
            fileName.chop(4);
            result << fileName;
            qDebug() << "[PROMPT] Znaleziono specjalny prompt:" << fileName;
        }
    }

    return result;
}

QString PromptManager::getSkillContent(const QString& fileName) const
{
    return getPromptData(fileName).skillContent;
}

PromptData PromptManager::getPromptData(const QString& fileName) const
{
    if (m_dataCache.contains(fileName)) {
        qDebug() << "[PROMPT] Używam cache dla:" << fileName;
        return m_dataCache[fileName];
    }

    const QString fullPath = m_promptsDir + "/" + fileName;
    QFile file(fullPath);

    PromptData data;

    if (!file.exists()) {
        qWarning() << "[PROMPT] Plik nie istnieje:" << fullPath;
        return data;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCritical() << "[PROMPT] Nie można otworzyć pliku:" << fullPath;
        return data;
    }

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);

    QString fileContent = in.readAll();
    file.close();

    QRegularExpression apiKeyRegex(R"(api_key\s*=\s*(.+))");
    QRegularExpressionMatch apiKeyMatch = apiKeyRegex.match(fileContent);
    if (apiKeyMatch.hasMatch()) {
        data.apiKey = apiKeyMatch.captured(1).trimmed();
        if (!data.apiKey.isEmpty()) {
            qInfo() << "[PROMPT] Znaleziono api_key w pliku:" << fileName;
        }
    }

    QRegularExpression prefixRegex(R"(userContentPrefix\s*=\s*(.+))");
    QRegularExpressionMatch prefixMatch = prefixRegex.match(fileContent);
    if (prefixMatch.hasMatch()) {
        data.userContentPrefix = prefixMatch.captured(1).trimmed();
        if (!data.userContentPrefix.isEmpty()) {
            qInfo() << "[PROMPT] Znaleziono userContentPrefix w pliku:" << fileName;
        }
    }

    QRegularExpression endpointRegex(R"(endpoint\s*=\s*(.+))");
    QRegularExpressionMatch endpointMatch = endpointRegex.match(fileContent);
    if (endpointMatch.hasMatch()) {
        data.endpoint = endpointMatch.captured(1).trimmed();
        if (!data.endpoint.isEmpty()) {
            qInfo() << "[PROMPT] Znaleziono endpoint w pliku:" << fileName;
        }
    }

    QRegularExpression modelRegex(R"(model\s*=\s*(.+))");
    QRegularExpressionMatch modelMatch = modelRegex.match(fileContent);
    if (modelMatch.hasMatch()) {
        data.model = modelMatch.captured(1).trimmed();
        if (!data.model.isEmpty()) {
            qInfo() << "[PROMPT] Znaleziono model w pliku:" << fileName;
        }
    }

    QRegularExpression tempRegex(R"(temperature\s*=\s*(.+))");
    QRegularExpressionMatch tempMatch = tempRegex.match(fileContent);
    if (tempMatch.hasMatch()) {
        QString tempStr = tempMatch.captured(1).trimmed();
        bool ok;
        double tempValue = tempStr.toDouble(&ok);
        if (ok) {
            data.temperature = tempValue;
            qInfo() << "[PROMPT] Znaleziono temperature w pliku:" << fileName << "=" << tempValue;
        }
    }

    QRegularExpression skillRegex(R"regex(skill\s*=\s*"((?:[^"\\]|\\.)*)")regex");
    QRegularExpressionMatch skillMatch = skillRegex.match(fileContent);
    if (skillMatch.hasMatch()) {
        data.skillContent = skillMatch.captured(1).trimmed();
    }

    qInfo() << "[PROMPT] Wczytano profil z pliku:" << fileName
            << "(" << data.skillContent.length() << "znaków)";
    if (!data.apiKey.isEmpty()) {
        qInfo() << "[PROMPT] API key obecny (długość:" << data.apiKey.length() << "znaków)";
    }

    m_dataCache[fileName] = data;
    m_cache[fileName] = data.skillContent;

    return data;
}
