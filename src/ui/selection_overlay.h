#pragma once
#include <QWidget>
#include <QPoint>
#include <QRect>
#include <QPixmap>
#include <QSystemTrayIcon>
#include "ocr/ocr_engine.h"
#include "network/llm_client.h"

class ConfigManager;

class SelectionOverlay : public QWidget
{
    Q_OBJECT
public:
    explicit SelectionOverlay(QSystemTrayIcon*    tray,
                              LlmClient*          llm,
                              const ConfigManager& config,
                              const QPixmap&      background,
                              const QRect&        screenGeometry,
                              bool                clipboardMode = false,
                              QWidget*            parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QSystemTrayIcon* m_tray;
    LlmClient*       m_llm;
    OcrEngine        m_ocr;
    QPixmap          m_fullScreenBackground;
    bool             m_clipboardMode;

    QPoint m_startPos;
    QPoint m_currentPos;
    QPoint m_mousePos;
    QPoint m_releaseGlobalPos;
    bool m_selecting = false;

    QRect selectionRect() const;
    void captureAndSave();
};
