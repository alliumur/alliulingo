#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QPoint>
#include <QSharedPointer>
#include <QSystemTrayIcon>
#include <QVector>
#include <functional>

class FallbackTranslator : public QObject
{
    Q_OBJECT
public:
    explicit FallbackTranslator(QSystemTrayIcon* tray, QObject* parent = nullptr);

    void translate(const QString& text, const QPoint& pos, const QString& engineName,
                   const QString& sourceLang, const QString& targetLang);

    void translateWithCallback(const QString& text,
                              const QString& sourceLang,
                              const QString& targetLang,
                              std::function<void(const QString&)> callback);

private:
    QSystemTrayIcon*      m_tray;
    QNetworkAccessManager m_nam;

    static QStringList splitIntoChunks(const QString& text, int maxChunkSize = 500);

    void translateParagraph(const QString&                   paragraph,
                            int                              paraIndex,
                            QSharedPointer<QVector<QString>> paraResults,
                            QSharedPointer<int>              paraPending,
                            const QString&                   fullOriginal,
                            const QPoint&                    pos,
                            const QString&                   engineName,
                            const QString&                   sourceLang,
                            const QString&                   targetLang);

    void translateChunk(const QString&                   chunk,
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
                        const QString&                   targetLang);

    void translateParagraphWithCallback(
        const QString&                                      paragraph,
        int                                                 paraIndex,
        QSharedPointer<QVector<QString>>                    paraResults,
        QSharedPointer<int>                                 paraPending,
        std::shared_ptr<std::function<void(const QString&)>> callback,
        const QString&                                      sourceLang,
        const QString&                                      targetLang);

    void translateChunkWithCallback(
        const QString&                                      chunk,
        int                                                 chunkIndex,
        QSharedPointer<QVector<QString>>                    chunkResults,
        QSharedPointer<int>                                 chunkPending,
        int                                                 paraIndex,
        QSharedPointer<QVector<QString>>                    paraResults,
        QSharedPointer<int>                                 paraPending,
        std::shared_ptr<std::function<void(const QString&)>> callback,
        const QString&                                      sourceLang,
        const QString&                                      targetLang);
};
