#include "network/fallback_translator.h"
#include "core/localization_manager.h"
#include "ui/result_popup.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrl>
#include <QDebug>

static QString toMyMemoryLangCode(const QString& tessCode)
{
    static const QMap<QString, QString> mapping = {
        {"eng", "en"},
        {"pol", "pl"},
        {"deu", "de"},
        {"fra", "fr"},
        {"spa", "es"},
        {"ita", "it"},
        {"rus", "ru"},
        {"jpn", "ja"},
        {"chi_sim", "zh-CN"},
        {"chi_tra", "zh-TW"},
        {"kor", "ko"},
        {"ara", "ar"},
        {"por", "pt"},
        {"nld", "nl"},
        {"swe", "sv"},
        {"nor", "no"},
        {"dan", "da"},
        {"fin", "fi"},
        {"ces", "cs"},
        {"ukr", "uk"}
    };
    return mapping.value(tessCode, tessCode);
}

FallbackTranslator::FallbackTranslator(QSystemTrayIcon* tray, QObject* parent)
    : QObject(parent), m_tray(tray)
{}

QStringList FallbackTranslator::splitIntoChunks(const QString& text, int maxChunkSize)
{
    QStringList sentences;
    int start = 0;
    for (int i = 0; i < text.length(); ++i) {
        const QChar c = text[i];
        const bool sentenceEnd = (c == '.' || c == '!' || c == '?')
                                 && (i + 1 < text.length() && text[i + 1].isSpace());
        if (sentenceEnd) {
            const QString s = text.mid(start, i - start + 1).trimmed();
            if (!s.isEmpty()) sentences << s;
            start = i + 1;
        }
    }
    const QString tail = text.mid(start).trimmed();
    if (!tail.isEmpty()) sentences << tail;

    if (sentences.isEmpty()) return { text };

    QStringList chunks;
    QString     current;
    for (const QString& s : std::as_const(sentences)) {
        if (s.length() > maxChunkSize) {
            if (!current.isEmpty()) { chunks << current; current.clear(); }
            chunks << s.left(maxChunkSize);
        } else if (current.isEmpty()) {
            current = s;
        } else if (current.length() + 1 + s.length() <= maxChunkSize) {
            current += ' ' + s;
        } else {
            chunks << current;
            current = s;
        }
    }
    if (!current.isEmpty()) chunks << current;
    return chunks;
}

void FallbackTranslator::translate(const QString& text, const QPoint& pos, const QString& engineName,
                                    const QString& sourceLang, const QString& targetLang)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        qWarning() << "[FALLBACK] Empty text received, skipping translation.";
        return;
    }

    const QStringList rawBlocks =
        trimmed.split(QRegularExpression("\\n{2,}"), Qt::SkipEmptyParts);

    QStringList paragraphs;
    for (const QString& block : rawBlocks) {
        QString cleaned = block;
        cleaned.replace('\n', ' ');
        cleaned = cleaned.simplified();
        if (!cleaned.isEmpty())
            paragraphs << cleaned;
    }

    if (paragraphs.isEmpty()) {
        qWarning() << "[FALLBACK] No non-empty paragraphs after normalization.";
        return;
    }

    qInfo() << "[FALLBACK] Paragraphs:" << paragraphs.size()
            << "| Total length:" << trimmed.length();

    auto paraResults = QSharedPointer<QVector<QString>>::create(paragraphs.size());
    auto paraPending = QSharedPointer<int>::create(paragraphs.size());

    for (int i = 0; i < paragraphs.size(); ++i)
        translateParagraph(paragraphs[i], i, paraResults, paraPending, text, pos, engineName, sourceLang, targetLang);
}

void FallbackTranslator::translateParagraph(
    const QString&                   paragraph,
    int                              paraIndex,
    QSharedPointer<QVector<QString>> paraResults,
    QSharedPointer<int>              paraPending,
    const QString&                   fullOriginal,
    const QPoint&                    pos,
    const QString&                   engineName,
    const QString&                   sourceLang,
    const QString&                   targetLang)
{
    QStringList chunks = (paragraph.length() > 500)
                             ? splitIntoChunks(paragraph, 500)
                             : QStringList{ paragraph };

    if (chunks.size() > 1)
        qInfo() << "[FALLBACK] Paragraph" << (paraIndex + 1)
                << "split into" << chunks.size() << "chunk(s).";

    auto chunkResults = QSharedPointer<QVector<QString>>::create(chunks.size());
    auto chunkPending = QSharedPointer<int>::create(chunks.size());

    for (int j = 0; j < chunks.size(); ++j)
        translateChunk(chunks[j], j, chunkResults, chunkPending,
                       paraIndex, paraResults, paraPending, fullOriginal, pos, engineName, sourceLang, targetLang);
}

void FallbackTranslator::translateChunk(
    const QString&                   chunk,
    int                              chunkIndex,
    QSharedPointer<QVector<QString>> chunkResults,
    QSharedPointer<int>              chunkPending,
    int                              paraIndex,
    QSharedPointer<QVector<QString>> paraResults,
    QSharedPointer<int>              paraPending,
    const QString&                   fullOriginal,
    const QPoint&                    pos,
    const QString&                   engineName,
    const QString&                   sourceLang,
    const QString&                   targetLang)
{
    const QString encoded = QString::fromLatin1(QUrl::toPercentEncoding(chunk));
    const QString langpair = toMyMemoryLangCode(sourceLang) + "|" + toMyMemoryLangCode(targetLang);
    const QUrl    url("https://api.mymemory.translated.net/get?q="
                      + encoded + "&langpair=" + langpair);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Alliulingo/1.0");

    qInfo()  << "[FALLBACK] Para" << (paraIndex + 1)
             << "chunk" << (chunkIndex + 1) << "/" << *chunkPending
             << "| length:" << chunk.length()
             << "| langpair:" << langpair;
    qDebug() << "[FALLBACK] Chunk content (first 100):" << chunk.left(100);

    QNetworkReply* reply = m_nam.get(request);

    connect(reply, &QNetworkReply::finished, this,
        [this, reply, chunkIndex, chunkResults, chunkPending,
         paraIndex, paraResults, paraPending, fullOriginal, pos, engineName, sourceLang, targetLang]()
    {
        reply->deleteLater();

        QString translation;

        if (reply->error() != QNetworkReply::NoError) {
            qCritical() << "[FALLBACK] Network error (para" << (paraIndex + 1)
                        << "chunk" << (chunkIndex + 1) << "):" << reply->errorString();
            translation = Loc::get("error.network");
        } else {
            const QByteArray    raw = reply->readAll();
            const QJsonDocument doc = QJsonDocument::fromJson(raw);

            if (doc.isObject()) {
                const QJsonObject root   = doc.object();
                const int         status = root["responseStatus"].toInt();
                if (status == 200) {
                    translation = root["responseData"].toObject()["translatedText"]
                                      .toString().trimmed();
                    qInfo() << "[FALLBACK] Para" << (paraIndex + 1)
                            << "chunk" << (chunkIndex + 1)
                            << "translated (first 100):" << translation.left(100);
                } else {
                    qCritical() << "[FALLBACK] API status" << status
                                << "(para" << (paraIndex + 1)
                                << "chunk" << (chunkIndex + 1) << ")";
                    translation = Loc::get("error.no_translation");
                }
            } else {
                qWarning() << "[FALLBACK] Invalid JSON (para" << (paraIndex + 1)
                           << "chunk" << (chunkIndex + 1) << ")";
                translation = Loc::get("error.response");
            }
        }

        (*chunkResults)[chunkIndex] = translation;
        --(*chunkPending);

        if (*chunkPending == 0) {
            QString paraTranslation;
            for (const QString& part : std::as_const(*chunkResults)) {
                if (!paraTranslation.isEmpty()) paraTranslation += ' ';
                paraTranslation += part;
            }

            (*paraResults)[paraIndex] = paraTranslation;
            --(*paraPending);

            if (*paraPending == 0) {
                QString combined;
                for (const QString& para : std::as_const(*paraResults)) {
                    if (!combined.isEmpty()) combined += "\n\n";
                    combined += para;
                }
                qInfo() << "[FALLBACK] All paragraphs assembled. Final length:"
                        << combined.length();

                auto* popup = new ResultPopup(combined, fullOriginal, pos, engineName,
                                              nullptr, nullptr, nullptr);
                Q_UNUSED(popup);
            }
        }
    });
}

void FallbackTranslator::translateWithCallback(const QString& text,
                                              const QString& sourceLang,
                                              const QString& targetLang,
                                              std::function<void(const QString&)> callback)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        qWarning() << "[FALLBACK] Empty text received, skipping translation.";
        callback("(pusty tekst)");
        return;
    }

    const QStringList rawBlocks =
        trimmed.split(QRegularExpression("\\n{2,}"), Qt::SkipEmptyParts);

    QStringList paragraphs;
    for (const QString& block : rawBlocks) {
        QString cleaned = block;
        cleaned.replace('\n', ' ');
        cleaned = cleaned.simplified();
        if (!cleaned.isEmpty())
            paragraphs << cleaned;
    }

    if (paragraphs.isEmpty()) {
        qWarning() << "[FALLBACK] No non-empty paragraphs after normalization.";
        callback("(brak tekstu)");
        return;
    }

    qInfo() << "[FALLBACK] Callback mode - Paragraphs:" << paragraphs.size()
            << "| Total length:" << trimmed.length();

    auto paraResults = QSharedPointer<QVector<QString>>::create(paragraphs.size());
    auto paraPending = QSharedPointer<int>::create(paragraphs.size());

    auto callbackPtr = std::make_shared<std::function<void(const QString&)>>(callback);

    for (int i = 0; i < paragraphs.size(); ++i) {
        translateParagraphWithCallback(paragraphs[i], i, paraResults, paraPending,
                                      callbackPtr, sourceLang, targetLang);
    }
}

void FallbackTranslator::translateParagraphWithCallback(
    const QString&                                      paragraph,
    int                                                 paraIndex,
    QSharedPointer<QVector<QString>>                    paraResults,
    QSharedPointer<int>                                 paraPending,
    std::shared_ptr<std::function<void(const QString&)>> callback,
    const QString&                                      sourceLang,
    const QString&                                      targetLang)
{
    QStringList chunks = (paragraph.length() > 500)
                             ? splitIntoChunks(paragraph, 500)
                             : QStringList{ paragraph };

    if (chunks.size() > 1)
        qInfo() << "[FALLBACK] Callback mode - Paragraph" << (paraIndex + 1)
                << "split into" << chunks.size() << "chunk(s).";

    auto chunkResults = QSharedPointer<QVector<QString>>::create(chunks.size());
    auto chunkPending = QSharedPointer<int>::create(chunks.size());

    for (int j = 0; j < chunks.size(); ++j)
        translateChunkWithCallback(chunks[j], j, chunkResults, chunkPending,
                                  paraIndex, paraResults, paraPending,
                                  callback, sourceLang, targetLang);
}

void FallbackTranslator::translateChunkWithCallback(
    const QString&                                      chunk,
    int                                                 chunkIndex,
    QSharedPointer<QVector<QString>>                    chunkResults,
    QSharedPointer<int>                                 chunkPending,
    int                                                 paraIndex,
    QSharedPointer<QVector<QString>>                    paraResults,
    QSharedPointer<int>                                 paraPending,
    std::shared_ptr<std::function<void(const QString&)>> callback,
    const QString&                                      sourceLang,
    const QString&                                      targetLang)
{
    const QString encoded = QString::fromLatin1(QUrl::toPercentEncoding(chunk));
    const QString langpair = toMyMemoryLangCode(sourceLang) + "|" + toMyMemoryLangCode(targetLang);
    const QUrl    url("https://api.mymemory.translated.net/get?q="
                      + encoded + "&langpair=" + langpair);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Alliulingo/1.0");

    qInfo()  << "[FALLBACK] Callback mode - Para" << (paraIndex + 1)
             << "chunk" << (chunkIndex + 1) << "/" << *chunkPending
             << "| length:" << chunk.length()
             << "| langpair:" << langpair;

    QNetworkReply* reply = m_nam.get(request);

    connect(reply, &QNetworkReply::finished, this,
        [this, reply, chunkIndex, chunkResults, chunkPending,
         paraIndex, paraResults, paraPending, callback]()
    {
        reply->deleteLater();

        QString translation;

        if (reply->error() != QNetworkReply::NoError) {
            qCritical() << "[FALLBACK] Callback mode - Network error (para" << (paraIndex + 1)
                        << "chunk" << (chunkIndex + 1) << "):" << reply->errorString();
            translation = Loc::get("error.network");
        } else {
            const QByteArray    raw = reply->readAll();
            const QJsonDocument doc = QJsonDocument::fromJson(raw);

            if (doc.isObject()) {
                const QJsonObject root   = doc.object();
                const int         status = root["responseStatus"].toInt();
                if (status == 200) {
                    translation = root["responseData"].toObject()["translatedText"]
                                      .toString().trimmed();
                    qInfo() << "[FALLBACK] Callback mode - Para" << (paraIndex + 1)
                            << "chunk" << (chunkIndex + 1)
                            << "translated (first 100):" << translation.left(100);
                } else {
                    qCritical() << "[FALLBACK] Callback mode - API status" << status
                                << "(para" << (paraIndex + 1)
                                << "chunk" << (chunkIndex + 1) << ")";
                    translation = Loc::get("error.no_translation");
                }
            } else {
                qWarning() << "[FALLBACK] Callback mode - Invalid JSON (para" << (paraIndex + 1)
                           << "chunk" << (chunkIndex + 1) << ")";
                translation = Loc::get("error.response");
            }
        }

        (*chunkResults)[chunkIndex] = translation;
        --(*chunkPending);

        if (*chunkPending == 0) {
            QString paraTranslation;
            for (const QString& part : std::as_const(*chunkResults)) {
                if (!paraTranslation.isEmpty()) paraTranslation += ' ';
                paraTranslation += part;
            }

            (*paraResults)[paraIndex] = paraTranslation;
            --(*paraPending);

            if (*paraPending == 0) {
                QString combined;
                for (const QString& para : std::as_const(*paraResults)) {
                    if (!combined.isEmpty()) combined += "\n\n";
                    combined += para;
                }
                qInfo() << "[FALLBACK] Callback mode - All paragraphs assembled. Final length:"
                        << combined.length();

                (*callback)(combined);
            }
        }
    });
}
