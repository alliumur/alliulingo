#ifndef MANUAL_TRANSLATE_WINDOW_H
#define MANUAL_TRANSLATE_WINDOW_H

#include <QWidget>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>

class LlmClient;
class ConfigManager;

class ManualTranslateWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ManualTranslateWindow(LlmClient* llm, ConfigManager& config, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
    void onTranslateClicked();
    void onSourceLangChanged(int index);
    void onTargetLangChanged(int index);

private:
    void setupUI();
    void performTranslation();
    void updateHeaderLabel();

    LlmClient* m_llm;
    ConfigManager& m_config;

    QLabel* m_headerLabel;
    QComboBox* m_sourceLangCombo;
    QComboBox* m_targetLangCombo;
    QTextEdit* m_inputField;
    QTextEdit* m_resultField;
    QPushButton* m_translateButton;
    QPushButton* m_copyButton;
    QPushButton* m_closeButton;

    QString m_lastTranslation;
    QPoint m_dragPosition;
    bool m_isDragging = false;
};

#endif
