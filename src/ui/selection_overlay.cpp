#include "selection_overlay.h"
#include "core/config_manager.h"
#include "core/localization_manager.h"
#include "ui/result_popup.h"
#include <QPainter>
#include <QPen>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QApplication>
#include <QClipboard>
#include <QDebug>

SelectionOverlay::SelectionOverlay(QSystemTrayIcon* tray, LlmClient* llm,
                                   const ConfigManager& config,
                                   const QPixmap& background, const QRect& screenGeometry,
                                   bool clipboardMode, QWidget* parent)
    : QWidget(parent), m_tray(tray), m_llm(llm), m_ocr(tray, config.sourceLang()),
      m_fullScreenBackground(background), m_clipboardMode(clipboardMode)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_DeleteOnClose);

    setGeometry(screenGeometry);

    setCursor(Qt::BlankCursor);
    setMouseTracking(true);
    show();
    activateWindow();
    setFocus();

    qDebug() << "[OVERLAY] Freeze-frame overlay uruchomiony na:" << screenGeometry
             << "| DPR:" << background.devicePixelRatio();
}

void SelectionOverlay::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    painter.drawPixmap(0, 0, m_fullScreenBackground);

    painter.fillRect(rect(), QColor(0, 0, 0, 120));

    if (m_selecting) {
        QRect sel = selectionRect();

        const qreal dpr = m_fullScreenBackground.devicePixelRatio();
        QRect physSel(qRound(sel.x() * dpr), qRound(sel.y() * dpr),
                      qRound(sel.width() * dpr), qRound(sel.height() * dpr));
        painter.drawPixmap(sel, m_fullScreenBackground, physSel);

        painter.setPen(QPen(Qt::white, 1, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(sel);
    }

    const QPoint p = m_mousePos;
    painter.setPen(QPen(QColor(0, 0, 0, 200), 3));
    painter.drawLine(0, p.y(), width(), p.y());
    painter.drawLine(p.x(), 0, p.x(), height());
    painter.setPen(QPen(Qt::white, 1));
    painter.drawLine(0, p.y(), width(), p.y());
    painter.drawLine(p.x(), 0, p.x(), height());
}

void SelectionOverlay::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_startPos = event->pos();
        m_currentPos = event->pos();
        m_selecting = true;
        update();
    }
}

void SelectionOverlay::mouseMoveEvent(QMouseEvent *event)
{
    m_mousePos = event->pos();
    if (m_selecting)
        m_currentPos = event->pos();
    update();
}

void SelectionOverlay::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_selecting) {
        m_selecting = false;
        m_currentPos = event->pos();
        m_releaseGlobalPos = event->globalPosition().toPoint();
        captureAndSave();
    }
}

void SelectionOverlay::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
        close();
}

QRect SelectionOverlay::selectionRect() const
{
    return QRect(m_startPos, m_currentPos).normalized();
}

void SelectionOverlay::captureAndSave()
{
    QRect sel = selectionRect();
    if (sel.width() < 5 || sel.height() < 5) {
        qDebug() << "[OVERLAY] Zaznaczenie zbyt małe" << sel.size() << "– anulowano.";
        close();
        return;
    }

    const qreal dpr = m_fullScreenBackground.devicePixelRatio();
    QRect physSel(qRound(sel.x() * dpr), qRound(sel.y() * dpr),
                  qRound(sel.width() * dpr), qRound(sel.height() * dpr));
    QPixmap pixmap = m_fullScreenBackground.copy(physSel);

    qDebug() << "[OVERLAY] Wycięto fragment (logiczne):" << sel
             << "| (fizyczne):" << physSel;

    QString path = QApplication::applicationDirPath() + "/snippet.png";
    if (!pixmap.save(path, "PNG"))
        qWarning() << "[OVERLAY] Nie udało się zapisać zrzutu do" << path;
    else
        qDebug() << "[OVERLAY] Zrzut zapisany do" << path;

    QImage image = pixmap.toImage();
    QString ocrText = m_ocr.extractText(image).trimmed();

    if (ocrText.startsWith("##OCR_ERROR##")) {
        QString errorMsg = ocrText.mid(13);
        qCritical() << "[OCR] Błąd inicjalizacji silnika OCR";

        ResultPopup* popup = new ResultPopup(errorMsg, "", m_releaseGlobalPos, "Tesseract",
                                             nullptr, nullptr, nullptr, false, nullptr);
        popup->show();
        close();
        return;
    }

    if (ocrText.isEmpty()) {
        qWarning() << "[OCR] Brak tekstu do przetworzenia.";
        close();
        return;
    }

    if (m_clipboardMode) {
        QClipboard* clipboard = QApplication::clipboard();
        clipboard->setText(ocrText);
        qDebug() << "[CLIPBOARD] Tekst skopiowany do schowka:" << ocrText.left(50) << "...";

        if (m_tray)
            m_tray->showMessage("Alliulingo",
                              Loc::get("manual_window.copied"),
                              QSystemTrayIcon::Information, 2000);
    } else {
        if (m_llm)
            m_llm->translate(ocrText, m_releaseGlobalPos);
        else
            qWarning() << "[OCR] Brak klienta LLM do tłumaczenia.";
    }

    close();
}
