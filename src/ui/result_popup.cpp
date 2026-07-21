#include "ui/result_popup.h"
#include "core/prompt_manager.h"
#include "core/config_manager.h"
#include "core/localization_manager.h"
#include "network/llm_client.h"
#include <windows.h>
#include <dwmapi.h>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPropertyAnimation>
#include <QClipboard>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QMenu>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

QList<ResultPopup*> ResultPopup::s_activePopups;

ResultPopup::ResultPopup(const QString& translatedText,
                         const QString& originalText,
                         const QPoint&  pos,
                         const QString& engineName,
                         PromptManager* promptManager,
                         ConfigManager* config,
                         LlmClient*     llm,
                         bool           isSpecialResult,
                         QWidget*       parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool),
      m_translatedText(translatedText),
      m_originalText(originalText),
      m_engineName(engineName),
      m_promptManager(promptManager),
      m_config(config),
      m_llm(llm),
      m_isSpecialResult(isSpecialResult)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);

    s_activePopups.append(this);

    setupUi(translatedText, engineName);
    adjustPosition(pos);

    setWindowOpacity(0.0);
    show();

    {
        HWND hwnd = reinterpret_cast<HWND>(winId());
        const COLORREF noBorder = 0xFFFFFFFE;
        DwmSetWindowAttribute(hwnd, 34, &noBorder, sizeof(noBorder));
    }

    auto* anim = new QPropertyAnimation(this, "windowOpacity", this);
    anim->setDuration(280);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->start(QAbstractAnimation::DeleteWhenStopped);

    qInfo() << "[UI] ResultPopup shown at" << pos << "| engine:" << engineName;

    if (m_isSpecialResult) {
        qInfo() << "[UI] Otwarto okno wyniku specjalnego (tryb: Moveable/Sticky)";
    }
}

void ResultPopup::closeAll()
{
    if (s_activePopups.isEmpty()) {
        return;
    }

    qInfo() << "[UI] Zamykanie" << s_activePopups.size() << "aktywnych okien ResultPopup";

    QList<ResultPopup*> popupsToClose = s_activePopups;
    for (ResultPopup* popup : popupsToClose) {
        if (popup) {
            popup->close();
        }
    }
}

void ResultPopup::setupUi(const QString& text, const QString& engineName)
{
    constexpr int kPopupWidth = 400;
    constexpr int kMargin     = 15;
    constexpr int kMaxContent = 350;

    const int innerWidth = kPopupWidth - kMargin - kMargin;

    setStyleSheet(R"(
        QLabel#content {
            color: #FFFFFF;
            font-size: 13px;
            background: transparent;
            border: none;
        }
        QLabel#header {
            color: #888888;
            font-size: 11px;
            font-style: italic;
            background: transparent;
            border: none;
        }
        QLabel#footer {
            color: #888888;
            font-size: 10px;
            background: transparent;
            border: none;
        }
        QPushButton#menuButton,
        QPushButton#closeButton {
            background: transparent;
            color: #888888;
            font-size: 18px;
            font-weight: bold;
            border: none;
            padding: 0px 0px 2px 0px;
            margin: 0px;
            min-width: 17px;
            max-width: 17px;
            min-height: 14px;
            max-height: 14px;
            text-align: center;
        }
        QPushButton#menuButton:hover,
        QPushButton#closeButton:hover {
            color: #E0E0E0;
            background: transparent;
            border-radius: 2px;
        }
        QScrollArea {
            background: transparent;
            border: none;
        }
        QScrollBar:vertical {
            background: rgba(255, 255, 255, 20);
            width: 6px;
            border-radius: 3px;
            margin: 0px;
        }
        QScrollBar::handle:vertical {
            background: rgba(255, 255, 255, 80);
            border-radius: 3px;
            min-height: 20px;
        }
        QScrollBar::add-line:vertical,
        QScrollBar::sub-line:vertical {
            height: 0px;
        }
    )");

    m_headerBar = new QWidget(this);
    auto* headerLayout = new QHBoxLayout(m_headerBar);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(0);

    m_leftSpacer = new QWidget(m_headerBar);
    m_leftSpacer->setFixedSize(17, 14);

    m_headerLabel = new QLabel(engineName.isEmpty() ? Loc::get("result_popup.header.translation") : engineName, m_headerBar);
    m_headerLabel->setObjectName("header");
    m_headerLabel->setAlignment(Qt::AlignCenter);

    headerLayout->addWidget(m_leftSpacer);
    headerLayout->addStretch();
    headerLayout->addWidget(m_headerLabel);
    headerLayout->addStretch();

    if (m_isSpecialResult) {
        m_closeButton = new QPushButton("\u00d7", m_headerBar);
        m_closeButton->setObjectName("closeButton");
        m_closeButton->setFixedSize(17, 14);
        m_closeButton->setFocusPolicy(Qt::NoFocus);
        connect(m_closeButton, &QPushButton::clicked, this, &ResultPopup::close);
        headerLayout->addWidget(m_closeButton, 0, Qt::AlignTop | Qt::AlignRight);
    } else {
        m_menuButton = new QPushButton("\u2261", m_headerBar);
        m_menuButton->setObjectName("menuButton");
        m_menuButton->setFixedSize(17, 14);
        m_menuButton->setFocusPolicy(Qt::NoFocus);
        m_menuButton->installEventFilter(this);
        connect(m_menuButton, &QPushButton::clicked, this, &ResultPopup::showContextMenu);
        headerLayout->addWidget(m_menuButton, 0, Qt::AlignTop | Qt::AlignRight);
    }

    if (m_isSpecialResult) {
        m_headerBar->installEventFilter(this);
        m_leftSpacer->installEventFilter(this);
        m_headerLabel->installEventFilter(this);
    }

    m_sepTop = new QWidget(this);
    m_sepTop->setFixedHeight(1);
    m_sepTop->setStyleSheet("background: rgba(255,255,255,30);");

    if (m_isSpecialResult) {
        m_sepTop->installEventFilter(this);
    }

    m_label = new QLabel(text);
    m_label->setObjectName("content");
    m_label->setWordWrap(true);
    m_label->setTextInteractionFlags(Qt::NoTextInteraction);
    m_label->setContextMenuPolicy(Qt::NoContextMenu);
    m_label->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    if (text.startsWith("(")) {
        m_label->setStyleSheet("QLabel#content { color: #888888; font-size: 13px; background: transparent; border: none; }");
    }

    QFont contentFont = m_label->font();
    contentFont.setPixelSize(13);
    m_label->setFont(contentFont);

    int labelH = m_label->heightForWidth(innerWidth - 8);
    if (labelH < 1) {
        m_label->adjustSize();
        labelH = m_label->sizeHint().height();
    }
    const int contentH = qBound(20, labelH + 8, kMaxContent);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidget(m_label);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setFixedSize(innerWidth, contentH);
    m_scrollArea->setAutoFillBackground(false);
    m_scrollArea->viewport()->setAutoFillBackground(false);

    m_sepBot = new QWidget(this);
    m_sepBot->setFixedHeight(1);
    m_sepBot->setStyleSheet("background: rgba(255,255,255,30);");

    m_footerLabel = new QLabel(
        m_isSpecialResult ? Loc::get("result_popup.footer.special") : Loc::get("result_popup.footer"),
        this);
    m_footerLabel->setObjectName("footer");
    m_footerLabel->setAlignment(Qt::AlignCenter);
    m_footerLabel->setWordWrap(true);

    if (m_isSpecialResult) {
        m_sepBot->installEventFilter(this);
        m_footerLabel->installEventFilter(this);
    }

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kMargin, kMargin, kMargin, kMargin);
    layout->setSpacing(6);
    layout->addWidget(m_headerBar);
    layout->addWidget(m_sepTop);
    layout->addWidget(m_scrollArea);
    layout->addWidget(m_sepBot);
    layout->addWidget(m_footerLabel);

    setFixedWidth(kPopupWidth);
}

void ResultPopup::adjustPosition(const QPoint& pos)
{
    adjustSize();

    QScreen* screen = QApplication::screenAt(pos);
    if (!screen)
        screen = QApplication::primaryScreen();

    const QRect avail = screen->availableGeometry();

    int x = pos.x() + 12;
    int y = pos.y() + 12;

    if (x + width()  > avail.right())  x = avail.right()  - width()  - 10;
    if (y + height() > avail.bottom()) y = avail.bottom() - height() - 10;
    if (x < avail.left())  x = avail.left()  + 10;
    if (y < avail.top())   y = avail.top()   + 10;

    move(x, y);
}

void ResultPopup::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.setCompositionMode(QPainter::CompositionMode_Clear);
    painter.fillRect(rect(), Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    painter.setBrush(QColor(45, 45, 45, 220));
    painter.setPen(QPen(QColor(100, 100, 100, 150), 1));
    painter.drawRoundedRect(rect(), 12, 12);
}

void ResultPopup::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        QWidget* widget = childAt(event->pos());
        if (widget == m_menuButton || widget == m_closeButton) {
            event->ignore();
            return;
        }

        if (m_isSpecialResult) {
            beginDrag(event->globalPosition().toPoint());
            event->accept();
            return;
        }

        if (m_menuVisible || m_isAnalyzing) {
            return;
        }
        close();
    } else if (event->button() == Qt::RightButton) {
        const bool altPressed = QGuiApplication::keyboardModifiers() & Qt::AltModifier;
        copyResultToClipboard(altPressed);
    }
}

void ResultPopup::copyResultToClipboard(bool copyOriginal)
{
    if (copyOriginal) {
        QApplication::clipboard()->setText(m_originalText);
        qInfo() << "[UI] Skopiowano tekst oryginalny do schowka";
    } else {
        QApplication::clipboard()->setText(m_translatedText);
        qInfo() << "[UI] Skopiowano tłumaczenie do schowka";
    }
}

void ResultPopup::beginDrag(const QPoint& globalPos)
{
    m_isDragging = true;
    m_dragPosition = globalPos - frameGeometry().topLeft();
}

void ResultPopup::continueDrag(const QPoint& globalPos)
{
    if (m_isDragging) {
        move(globalPos - m_dragPosition);
    }
}

void ResultPopup::endDrag()
{
    m_isDragging = false;
}

void ResultPopup::mouseMoveEvent(QMouseEvent* event)
{
    if (m_isSpecialResult && (event->buttons() & Qt::LeftButton)) {
        continueDrag(event->globalPosition().toPoint());
        event->accept();
    }
}

void ResultPopup::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        endDrag();
    }
    QWidget::mouseReleaseEvent(event);
}

void ResultPopup::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape)
        close();
    else
        QWidget::keyPressEvent(event);
}

void ResultPopup::closeEvent(QCloseEvent* event)
{
    s_activePopups.removeOne(this);
    qDebug() << "[UI] ResultPopup zamknięty. Pozostało aktywnych:" << s_activePopups.size();
    QWidget::closeEvent(event);
}

void ResultPopup::executeSpecialPrompt(const QString& promptName)
{
    if (!m_promptManager || !m_config) {
        qWarning() << "[UI] Brak wymaganych komponentów do wykonania specjalnego promptu";
        return;
    }

    qInfo() << "[UI] Wykonywanie specjalnego promptu:" << promptName;

    const QString fileName = "#" + promptName + ".txt";
    const PromptData promptData = m_promptManager->getPromptData(fileName);

    if (promptData.skillContent.isEmpty()) {
        qWarning() << "[UI] Nie można wczytać specjalnego promptu:" << fileName;
        return;
    }

    m_isAnalyzing = true;
    m_label->setText(Loc::get("result_popup.analyzing"));

    QString systemPrompt = promptData.skillContent;
    systemPrompt.replace("[sLang]", m_config->sourceLang());
    systemPrompt.replace("[tLang]", m_config->targetLang());

    qInfo() << "[UI] System prompt dla specjalnego promptu:" << systemPrompt.left(100);

    QJsonArray messages;
    messages.append(QJsonObject{{ "role", "system" }, { "content", systemPrompt }});

    QString userContent = m_originalText;
    if (!promptData.userContentPrefix.isEmpty()) {
        userContent = promptData.userContentPrefix + " " + m_originalText;
        qInfo() << "[UI] Dodano userContentPrefix do user content (specjalny prompt)";
    }

    messages.append(QJsonObject{{ "role", "user"   }, { "content", userContent }});

    QJsonObject body;
    body["messages"]    = messages;
    body["model"]       = m_config->model();
    body["temperature"] = m_config->temperature();

    const QByteArray jsonData = QJsonDocument(body).toJson(QJsonDocument::Compact);

    QNetworkRequest request(QUrl(m_config->endpoint()));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    const QString apiKey = !promptData.apiKey.isEmpty() ? promptData.apiKey : m_config->apiKey();
    if (!apiKey.isEmpty()) {
        const QString authHeader = "Bearer " + apiKey;
        request.setRawHeader("Authorization", authHeader.toUtf8());
    }

    qInfo() << "[UI] Wysyłanie zapytania do LM Studio z specjalnym promptem";

    QNetworkReply* reply = m_nam.post(request, jsonData);

    connect(reply, &QNetworkReply::finished, this, [this, reply, promptName]() {
        reply->deleteLater();

        m_isAnalyzing = false;

        QString translation;
        if (reply->error() != QNetworkReply::NoError) {
            qCritical() << "[UI] Błąd LM Studio dla specjalnego promptu:" << reply->errorString();
            translation = QString("[ERROR] %1").arg(reply->errorString());
        } else {
            const QByteArray data = reply->readAll();
            const QJsonDocument doc = QJsonDocument::fromJson(data);

            if (doc.isObject()) {
                const auto choices = doc.object()["choices"].toArray();
                if (!choices.isEmpty())
                    translation = choices[0].toObject()["message"]
                                      .toObject()["content"]
                                      .toString()
                                      .trimmed();
            }

            if (translation.isEmpty()) {
                qWarning() << "[UI] Pusta odpowiedź z LM Studio dla specjalnego promptu";
                translation = Loc::get("result_popup.no_response");
            } else {
                qInfo() << "[UI] Odpowiedź ze specjalnego promptu:" << translation.left(100);
            }
        }

        const QPoint specialPopupPos = pos() - QPoint(12, 12);
        auto* specialPopup = new ResultPopup(translation, m_originalText, specialPopupPos, m_engineName,
                                             m_promptManager, m_config, m_llm, true);
        Q_UNUSED(specialPopup);

        close();
    });
}

void ResultPopup::showContextMenu()
{
    static const char* kMenuStyle = R"(
        QMenu {
            background: rgba(45, 45, 45, 240);
            color: #E0E0E0;
            border: 1px solid rgba(100, 100, 100, 150);
        }
        QMenu::item {
            padding: 4px 20px;
        }
        QMenu::item:selected {
            background: rgba(80, 80, 80, 200);
        }
    )";

    if (!m_promptManager || !m_config) {
        m_menuVisible = true;
        if (m_menuResetTimer) {
            m_menuResetTimer->stop();
            m_menuResetTimer->deleteLater();
            m_menuResetTimer = nullptr;
        }
        QMenu* menu = new QMenu(this);
        menu->setStyleSheet(kMenuStyle);
        QAction* a = menu->addAction(Loc::get("result_popup.basic_mode_info"));
        a->setEnabled(false);
        connect(menu, &QMenu::aboutToHide, this, [this]() {
            if (m_menuResetTimer) {
                m_menuResetTimer->stop();
                m_menuResetTimer->deleteLater();
            }
            m_menuResetTimer = new QTimer(this);
            m_menuResetTimer->setSingleShot(true);
            connect(m_menuResetTimer, &QTimer::timeout, this, [this]() {
                m_menuVisible = false;
                m_menuResetTimer->deleteLater();
                m_menuResetTimer = nullptr;
            });
            m_menuResetTimer->start(100);
        });
        menu->exec(m_menuButton->mapToGlobal(QPoint(0, m_menuButton->height())));
        menu->deleteLater();
        return;
    }

    m_menuVisible = true;

    if (m_menuResetTimer) {
        m_menuResetTimer->stop();
        m_menuResetTimer->deleteLater();
        m_menuResetTimer = nullptr;
    }

    QMenu* menu = new QMenu(this);
    menu->setStyleSheet(kMenuStyle);

    const bool isAiMode = (m_config->translatorMode() == 0);

    if (!isAiMode) {
        QAction* basicModeAction = menu->addAction(Loc::get("result_popup.basic_mode_info"));
        basicModeAction->setEnabled(false);
    } else {
        const QStringList specialPrompts = m_promptManager->specialPrompts();

        if (specialPrompts.isEmpty()) {
            QAction* noPromptsAction = menu->addAction(Loc::get("result_popup.no_special_prompts"));
            noPromptsAction->setEnabled(false);
        } else {
            // QAction* header = menu->addAction(Loc::get("result_popup.tooltip.special_prompts"));
            // header->setEnabled(false);
            // menu->addSeparator();
            for (const QString& promptName : specialPrompts) {
                QAction* action = menu->addAction(promptName);
                connect(action, &QAction::triggered, this, [this, promptName]() {
                    executeSpecialPrompt(promptName);
                });
            }
        }
    }

    connect(menu, &QMenu::aboutToHide, this, [this]() {
        if (m_menuResetTimer) {
            m_menuResetTimer->stop();
            m_menuResetTimer->deleteLater();
        }

        m_menuResetTimer = new QTimer(this);
        m_menuResetTimer->setSingleShot(true);
        connect(m_menuResetTimer, &QTimer::timeout, this, [this]() {
            m_menuVisible = false;
            m_menuResetTimer->deleteLater();
            m_menuResetTimer = nullptr;
        });
        m_menuResetTimer->start(100);
    });

    QPoint pos = m_menuButton->mapToGlobal(QPoint(0, m_menuButton->height()));
    menu->exec(pos);
    menu->deleteLater();
}

bool ResultPopup::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_menuButton && event->type() == QEvent::ToolTip)
        return true;

    const bool isDraggableChrome = m_isSpecialResult &&
        (obj == m_headerBar || obj == m_leftSpacer || obj == m_headerLabel ||
         obj == m_sepTop    || obj == m_sepBot      || obj == m_footerLabel);

    if (isDraggableChrome) {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                beginDrag(mouseEvent->globalPosition().toPoint());
                return true;
            }
            if (mouseEvent->button() == Qt::RightButton) {
                const bool altPressed = QGuiApplication::keyboardModifiers() & Qt::AltModifier;
                copyResultToClipboard(altPressed);
                return true;
            }
            break;
        }
        case QEvent::MouseMove: {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (m_isDragging && (mouseEvent->buttons() & Qt::LeftButton)) {
                continueDrag(mouseEvent->globalPosition().toPoint());
                return true;
            }
            break;
        }
        case QEvent::MouseButtonRelease: {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                endDrag();
                return true;
            }
            break;
        }
        default:
            break;
        }
    }

    return QWidget::eventFilter(obj, event);
}

void ResultPopup::updateContentSize()
{
    if (!m_label || !m_scrollArea) {
        return;
    }

    constexpr int kPopupWidth = 400;
    constexpr int kMargin     = 15;
    constexpr int kMaxContent = 350;

    const int innerWidth = kPopupWidth - kMargin - kMargin;

    int labelH = m_label->heightForWidth(innerWidth - 8);
    if (labelH < 1) {
        m_label->adjustSize();
        labelH = m_label->sizeHint().height();
    }
    const int contentH = qBound(20, labelH + 8, kMaxContent);

    m_scrollArea->setFixedSize(innerWidth, contentH);

    adjustSize();
}
