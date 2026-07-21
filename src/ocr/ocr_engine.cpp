#include "ocr/ocr_engine.h"
#include "core/localization_manager.h"
#include <leptonica/allheaders.h>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <cstdlib>

OcrEngine::OcrEngine(QSystemTrayIcon* tray, const QString& initialLang)
    : m_tray(tray), m_currentLang(initialLang)
{
    m_tessdataPath = QCoreApplication::applicationDirPath() + "/tessdata";
    _putenv_s("TESSDATA_PREFIX", m_tessdataPath.toUtf8().constData());

    m_api = new tesseract::TessBaseAPI();

    if (!initTesseract(initialLang)) {
        qCritical() << "[OCR] Nie udało się zainicjalizować Tesseract z językiem:" << initialLang;
        m_initError = Loc::get("error.ocr.message").arg(initialLang);
        delete m_api;
        m_api = nullptr;
    }
}

OcrEngine::~OcrEngine()
{
    if (m_api) {
        m_api->End();
        delete m_api;
    }
}

bool OcrEngine::initTesseract(const QString& lang)
{
    if (!m_api) return false;

    QDir tessdataDir(m_tessdataPath);
    if (!tessdataDir.exists()) {
        qCritical() << "[OCR] Katalog tessdata nie istnieje:" << m_tessdataPath;
        return false;
    }

    try {
        int result = m_api->Init(m_tessdataPath.toUtf8().constData(), lang.toUtf8().constData());

        if (result != 0)
            result = m_api->Init(nullptr, lang.toUtf8().constData());

        if (result == 0) {
            qInfo() << "[OCR] Tesseract zainicjalizowany. Język:" << lang << "| Katalog:" << m_tessdataPath;
            m_currentLang = lang;
            return true;
        }
    } catch (const std::exception& e) {
        qCritical() << "[OCR] Wyjątek podczas inicjalizacji Tesseract:" << e.what();
        return false;
    } catch (...) {
        qCritical() << "[OCR] Nieznany wyjątek podczas inicjalizacji Tesseract";
        return false;
    }

    return false;
}

bool OcrEngine::setLanguage(const QString& lang)
{
    if (!m_api) {
        qWarning() << "[OCR] Nie można zmienić języka – silnik nie został zainicjalizowany.";
        return false;
    }

    if (lang == m_currentLang) {
        qDebug() << "[OCR] Język już ustawiony na:" << lang;
        return true;
    }

    qInfo() << "[OCR] Zmiana języka z" << m_currentLang << "na" << lang;

    m_api->End();

    if (initTesseract(lang)) {
        qInfo() << "[LANG] Zmieniono parę tłumaczenia na:" << lang << "-> [TARGET]";
        return true;
    } else {
        qCritical() << "[OCR] Nie udało się załadować języka:" << lang;
        if (!initTesseract(m_currentLang)) {
            qCritical() << "[OCR] Nie udało się przywrócić poprzedniego języka:" << m_currentLang;
            delete m_api;
            m_api = nullptr;
        }
        return false;
    }
}

QString OcrEngine::extractText(const QImage& image)
{
    if (!m_api) {
        qCritical() << "[OCR] extractText: silnik nie został zainicjalizowany.";
        if (m_initError.isEmpty()) {
            return "##OCR_ERROR##" + Loc::get("error.ocr.message").arg(m_currentLang);
        }
        return "##OCR_ERROR##" + m_initError;
    }

    QImage gray = image.convertToFormat(QImage::Format_Grayscale8);

    m_api->SetImage(gray.bits(), gray.width(), gray.height(),
                    gray.depth() / 8, gray.bytesPerLine());

    char *rawText = m_api->GetUTF8Text();
    QString result = QString::fromUtf8(rawText);
    delete[] rawText;

    m_api->Clear();

    qDebug() << "[OCR-RAW]:" << result;
    qDebug() << "[OCR] Rozpoznany tekst:" << result.trimmed().left(120);

    return result;
}
