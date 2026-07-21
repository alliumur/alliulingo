#pragma once
#include <QWidget>
#include <QLabel>
#include <QList>
#include <QPushButton>
#include <QNetworkAccessManager>

class QPropertyAnimation;
class QScrollArea;
class PromptManager;
class ConfigManager;
class LlmClient;

class ResultPopup : public QWidget
{
    Q_OBJECT
public:
    explicit ResultPopup(const QString& translatedText,
                         const QString& originalText,
                         const QPoint&  pos,
                         const QString& engineName,
                         PromptManager* promptManager,
                         ConfigManager* config,
                         LlmClient*     llm,
                         bool           isSpecialResult = false,
                         QWidget*       parent = nullptr);

    static void closeAll();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    QLabel* m_label = nullptr;
    QScrollArea* m_scrollArea = nullptr;
    QPushButton* m_menuButton = nullptr;
    QPushButton* m_closeButton = nullptr;
    QWidget* m_headerBar = nullptr;
    QWidget* m_leftSpacer = nullptr;
    QLabel* m_headerLabel = nullptr;
    QWidget* m_sepTop = nullptr;
    QWidget* m_sepBot = nullptr;
    QLabel* m_footerLabel = nullptr;
    QString m_translatedText;
    QString m_originalText;
    QString m_engineName;
    bool m_menuVisible = false;
    QTimer* m_menuResetTimer = nullptr;
    bool m_isAnalyzing = false;
    bool m_isSpecialResult = false;
    bool m_isDragging = false;
    QPoint m_dragPosition;

    PromptManager* m_promptManager = nullptr;
    ConfigManager* m_config = nullptr;
    LlmClient* m_llm = nullptr;
    QNetworkAccessManager m_nam;

    void setupUi(const QString& text, const QString& engineName);
    void adjustPosition(const QPoint& pos);
    void showContextMenu();
    void executeSpecialPrompt(const QString& promptName);
    void updateContentSize();
    void copyResultToClipboard(bool copyOriginal);
    void beginDrag(const QPoint& globalPos);
    void continueDrag(const QPoint& globalPos);
    void endDrag();

    static QList<ResultPopup*> s_activePopups;
};
