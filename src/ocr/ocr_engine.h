#pragma once
#include <QString>
#include <QImage>
#include <QSystemTrayIcon>
#include <tesseract/baseapi.h>

class OcrEngine
{
public:
    explicit OcrEngine(QSystemTrayIcon* tray, const QString& initialLang = "eng");
    ~OcrEngine();

    QString extractText(const QImage& image);

    bool setLanguage(const QString& lang);

private:
    tesseract::TessBaseAPI *m_api = nullptr;
    QSystemTrayIcon* m_tray = nullptr;
    QString m_currentLang;
    QString m_tessdataPath;
    QString m_initError;

    bool initTesseract(const QString& lang);
};
