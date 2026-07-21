#include "ui/manual_translate_window.h"
#include "core/localization_manager.h"
#include "network/llm_client.h"
#include "core/config_manager.h"
#include "network/fallback_translator.h"
#include <windows.h>
#include <dwmapi.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScreen>
#include <QGuiApplication>
#include <QPainter>
#include <QClipboard>
#include <QFontMetrics>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QPropertyAnimation>
#include <QDebug>

static QString languageDisplayName(const QString& code)
{
    return Loc::get("lang." + code);
}

ManualTranslateWindow::ManualTranslateWindow(LlmClient* llm, ConfigManager& config, QWidget *parent)
    : QWidget(parent), m_llm(llm), m_config(config)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);

    setupUI();

    QScreen *screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->availableGeometry();
    adjustSize();
    int x = screenGeometry.right() - width() - 20;
    int y = screenGeometry.bottom() - height() - 20;
    move(x, y);

    setWindowOpacity(0.0);
    show();

    {
        HWND hwnd = reinterpret_cast<HWND>(winId());
        const COLORREF noBorder = 0xFFFFFFFE;
        DwmSetWindowAttribute(hwnd, 34 , &noBorder, sizeof(noBorder));
    }

    auto* anim = new QPropertyAnimation(this, "windowOpacity", this);
    anim->setDuration(280);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->start(QAbstractAnimation::DeleteWhenStopped);

    m_inputField->setFocus();

    qInfo() << "[MANUAL] Okno ręcznego tłumaczenia otwarte";
}

void ManualTranslateWindow::setupUI()
{
    constexpr int kMargin = 10;
    constexpr int kWidth  = 500;

    setStyleSheet(R"(
        QTextEdit {
            background: rgba(35, 35, 35, 200);
            color: #E0E0E0;
            font-size: 14px;
            border: 1px solid rgba(100, 100, 100, 100);
            border-radius: 4px;
            padding: 8px;
            selection-background-color: rgba(80, 80, 80, 150);
        }
        QLabel#header {
            color: #888888;
            font-size: 11px;
            font-style: italic;
            background: transparent;
            border: none;
        }
        QTextEdit#result {
            background: rgba(25, 25, 25, 200);
            color: #E0E0E0;
            font-size: 14px;
            border: 1px solid rgba(100, 100, 100, 100);
            border-radius: 4px;
            padding: 8px;
        }
        QComboBox {
            background: rgba(60, 60, 60, 200);
            color: #E0E0E0;
            font-size: 12px;
            border: 1px solid rgba(100, 100, 100, 150);
            border-radius: 4px;
            padding: 4px 8px;
        }
        QComboBox::drop-down {
            border: none;
        }
        QComboBox::down-arrow {
            image: none;
            width: 0;
            height: 0;
            border: none;
        }
        QComboBox QAbstractItemView {
            background: rgba(45, 45, 45, 240);
            color: #E0E0E0;
            selection-background-color: rgba(80, 80, 80, 200);
            border: 1px solid rgba(100, 100, 100, 150);
        }
        QPushButton {
            background: rgba(60, 60, 60, 200);
            color: #E0E0E0;
            font-size: 12px;
            border: 1px solid rgba(100, 100, 100, 150);
            border-radius: 4px;
            padding: 6px 16px;
            min-width: 100px;
        }
        QPushButton:hover {
            background: rgba(80, 80, 80, 200);
            border: 1px solid rgba(120, 120, 120, 150);
        }
        QPushButton:pressed {
            background: rgba(40, 40, 40, 200);
        }
        QPushButton:disabled {
            color: #555555;
            border: 1px solid rgba(80, 80, 80, 100);
        }
    )");

    QString engineName = m_config.translatorMode() == 1 ? "MyMemory" : "AI";

    auto* headerLayout = new QHBoxLayout();

    auto* leftSpacer = new QWidget(this);
    leftSpacer->setFixedSize(17, 14);

    m_headerLabel = new QLabel(engineName, this);
    m_headerLabel->setObjectName("header");
    m_headerLabel->setAlignment(Qt::AlignCenter);

    m_closeButton = new QPushButton("\u00D7", this);
    m_closeButton->setFixedSize(17, 14);
    m_closeButton->setFocusPolicy(Qt::NoFocus);
    m_closeButton->setStyleSheet(R"(
        QPushButton {
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
        QPushButton:hover {
            color: #E0E0E0;
            background: transparent;
            border-radius: 2px;
        }
    )");
    connect(m_closeButton, &QPushButton::clicked, this, &ManualTranslateWindow::close);

    headerLayout->addWidget(leftSpacer);
    headerLayout->addStretch();
    headerLayout->addWidget(m_headerLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(m_closeButton, 0, Qt::AlignTop | Qt::AlignRight);

    auto* sepTop = new QWidget(this);
    sepTop->setFixedHeight(1);
    sepTop->setStyleSheet("background: rgba(255,255,255,30);");

    auto* langLayout = new QHBoxLayout();

    auto* sourceLangLabel = new QLabel(Loc::get("manual_window.translate_from"), this);
    sourceLangLabel->setObjectName("header");
    m_sourceLangCombo = new QComboBox(this);

    auto* targetLangLabel = new QLabel(Loc::get("manual_window.translate_to"), this);
    targetLangLabel->setObjectName("header");
    m_targetLangCombo = new QComboBox(this);

    const QStringList availableLangs = m_config.availableOcrLanguages();
    for (const QString& langCode : availableLangs) {
        const QString displayName = languageDisplayName(langCode);
        m_sourceLangCombo->addItem(displayName, langCode);
        m_targetLangCombo->addItem(displayName, langCode);
    }

    int sourceIndex = m_sourceLangCombo->findData(m_config.manualSourceLang());
    if (sourceIndex >= 0) m_sourceLangCombo->setCurrentIndex(sourceIndex);

    int targetIndex = m_targetLangCombo->findData(m_config.manualTargetLang());
    if (targetIndex >= 0) m_targetLangCombo->setCurrentIndex(targetIndex);

    connect(m_sourceLangCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ManualTranslateWindow::onSourceLangChanged);
    connect(m_targetLangCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ManualTranslateWindow::onTargetLangChanged);

    langLayout->addWidget(sourceLangLabel);
    langLayout->addWidget(m_sourceLangCombo, 1);
    langLayout->addSpacing(10);
    langLayout->addWidget(targetLangLabel);
    langLayout->addWidget(m_targetLangCombo, 1);

    m_inputField = new QTextEdit(this);
    m_inputField->setAcceptRichText(false);
    m_inputField->setPlaceholderText(Loc::get("manual_window.input_placeholder"));
    m_inputField->setMinimumHeight(100);
    m_inputField->setMaximumHeight(150);

    m_translateButton = new QPushButton(Loc::get("manual_window.button.translate"), this);
    m_translateButton->setEnabled(false);
    m_translateButton->setFocusPolicy(Qt::NoFocus);
    connect(m_translateButton, &QPushButton::clicked, this, &ManualTranslateWindow::onTranslateClicked);

    connect(m_inputField, &QTextEdit::textChanged, this, [this]() {
        m_translateButton->setEnabled(!m_inputField->toPlainText().trimmed().isEmpty());
    });

    m_resultField = new QTextEdit(this);
    m_resultField->setObjectName("result");
    m_resultField->setReadOnly(true);
    m_resultField->setAcceptRichText(false);
    m_resultField->setPlaceholderText(Loc::get("manual_window.result_placeholder"));
    m_resultField->setMinimumHeight(100);
    m_resultField->setMaximumHeight(150);
    m_resultField->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    m_copyButton = new QPushButton(Loc::get("manual_window.button.copy"), this);
    m_copyButton->setEnabled(false);
    m_copyButton->setFocusPolicy(Qt::NoFocus);
    connect(m_copyButton, &QPushButton::clicked, this, [this]() {
        if (!m_lastTranslation.isEmpty()) {
            QGuiApplication::clipboard()->setText(m_lastTranslation);
            qInfo() << "[MANUAL] Skopiowano tłumaczenie do schowka";
            m_resultField->setStyleSheet("");
            m_resultField->setPlainText(m_lastTranslation + "\n\n" + Loc::get("manual_window.copied"));
        }
    });

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kMargin, kMargin, kMargin, kMargin);
    layout->setSpacing(4);

    layout->addLayout(headerLayout);
    layout->addWidget(sepTop);
    layout->addSpacing(4);
    layout->addLayout(langLayout);
    layout->addSpacing(4);
    layout->addWidget(m_inputField);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_translateButton);
    buttonLayout->addStretch();
    layout->addLayout(buttonLayout);

    layout->addSpacing(8);
    layout->addWidget(m_resultField, 1);

    auto* copyLayout = new QHBoxLayout();
    copyLayout->addStretch();
    copyLayout->addWidget(m_copyButton);
    copyLayout->addStretch();
    layout->addLayout(copyLayout);

    setFixedWidth(kWidth);
    setMinimumHeight(450);
}

void ManualTranslateWindow::paintEvent(QPaintEvent* event)
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

void ManualTranslateWindow::keyPressEvent(QKeyEvent *event)
{
    QWidget::keyPressEvent(event);
}

void ManualTranslateWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QWidget* widget = childAt(event->pos());
        if (widget && (widget == m_closeButton || widget == m_translateButton || widget == m_copyButton)) {
            m_isDragging = false;
            event->ignore();
            return;
        }
        m_isDragging = true;
        m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void ManualTranslateWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isDragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragPosition);
        event->accept();
    }
}

void ManualTranslateWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
    }
    QWidget::mouseReleaseEvent(event);
}

void ManualTranslateWindow::onSourceLangChanged(int index)
{
    const QString langCode = m_sourceLangCombo->itemData(index).toString();
    m_config.setManualSourceLang(langCode);
    updateHeaderLabel();
    qInfo() << "[MANUAL] Zmieniono język źródłowy na:" << langCode;
}

void ManualTranslateWindow::onTargetLangChanged(int index)
{
    const QString langCode = m_targetLangCombo->itemData(index).toString();
    m_config.setManualTargetLang(langCode);
    updateHeaderLabel();
    qInfo() << "[MANUAL] Zmieniono język docelowy na:" << langCode;
}

void ManualTranslateWindow::updateHeaderLabel()
{
    QString engineName = m_config.translatorMode() == 1 ? "MyMemory" : "AI";
    m_headerLabel->setText(engineName);
}

void ManualTranslateWindow::onTranslateClicked()
{
    const QString inputText = m_inputField->toPlainText().trimmed();

    if (inputText.isEmpty()) {
        m_resultField->setStyleSheet("QTextEdit#result { color: #888888; }");
        m_resultField->setPlainText(Loc::get("error.response"));
        qWarning() << "[MANUAL] Próba tłumaczenia pustego tekstu";
        return;
    }

    qInfo() << "[MANUAL] Rozpoczynam tłumaczenie tekstu:" << inputText.left(50);

    m_resultField->setStyleSheet("QTextEdit#result { color: #888888; }");
    m_resultField->setPlainText(Loc::get("manual_window.translating"));
    m_translateButton->setEnabled(false);
    m_copyButton->setEnabled(false);

    performTranslation();
}

void ManualTranslateWindow::performTranslation()
{
    const QString inputText = m_inputField->toPlainText().trimmed();

    if (m_config.manualSourceLang() == m_config.manualTargetLang()) {
        qInfo() << "[MANUAL] Języki źródłowy i docelowy są takie same, pomijam tłumaczenie";
        m_lastTranslation = inputText;
        m_resultField->setStyleSheet("");
        m_resultField->setPlainText(inputText);
        m_translateButton->setEnabled(true);
        m_copyButton->setEnabled(true);
        return;
    }

    if (m_config.translatorMode() == 1) {
        qInfo() << "[MANUAL] Używam trybu Online (MyMemory) z FallbackTranslator";

        FallbackTranslator* translator = new FallbackTranslator(nullptr, this);

        translator->translateWithCallback(
            inputText,
            m_config.manualSourceLang(),
            m_config.manualTargetLang(),
            [this](const QString& translation) {
                m_translateButton->setEnabled(true);

                if (translation.contains(Loc::get("error.network").left(6)) ||
                    translation.contains(Loc::get("error.no_translation").left(6)) ||
                    translation.contains("(pusty") || translation.contains("[ERROR]")) {
                    m_resultField->setStyleSheet("QTextEdit#result { color: #888888; }");
                } else {
                    m_resultField->setStyleSheet("");
                    qInfo() << "[MANUAL] Tłumaczenie otrzymane (długość:" << translation.length() << ")";
                }

                m_lastTranslation = translation;
                m_resultField->setPlainText(translation);
                m_copyButton->setEnabled(true);
            }
        );

        return;
    }

    qInfo() << "[MANUAL] Używam trybu AI (LM Studio)";

    QString systemPrompt;
    if (!m_config.skill().isEmpty()) {
        systemPrompt = m_config.processSkillTemplate(m_config.manualSourceLang(), m_config.manualTargetLang());
    } else {
        systemPrompt = Loc::get("fallback.system_prompt")
                           .arg(m_config.manualSourceLang(), m_config.manualTargetLang());
    }

    qInfo() << "[MANUAL] System prompt:" << systemPrompt;

    QJsonArray messages;
    messages.append(QJsonObject{{ "role", "system" }, { "content", systemPrompt }});

    QString userContent = inputText;
    if (!m_config.userContentPrefix().isEmpty()) {
        userContent = m_config.userContentPrefix() + " " + inputText;
        qInfo() << "[MANUAL] Dodano userContentPrefix do user content";
    }

    messages.append(QJsonObject{{ "role", "user"   }, { "content", userContent }});

    QJsonObject body;
    body["messages"]    = messages;
    body["model"]       = m_config.model();
    body["temperature"] = m_config.temperature();

    const QByteArray jsonData = QJsonDocument(body).toJson(QJsonDocument::Compact);

    QNetworkRequest request(QUrl(m_config.endpoint()));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    if (!m_config.apiKey().isEmpty()) {
        const QString authHeader = "Bearer " + m_config.apiKey();
        request.setRawHeader("Authorization", authHeader.toUtf8());
    }

    auto* nam = new QNetworkAccessManager(this);
    QNetworkReply* reply = nam->post(request, jsonData);

    connect(reply, &QNetworkReply::finished, this, [this, reply, nam, inputText]() {
        reply->deleteLater();
        nam->deleteLater();

        m_translateButton->setEnabled(true);

        if (reply->error() != QNetworkReply::NoError) {
            qCritical() << "[MANUAL] Błąd LM Studio:" << reply->errorString();
            const QString errorMsg = Loc::get("network.error.llm_unavailable");
            m_resultField->setStyleSheet("QTextEdit#result { color: #888888; }");
            m_resultField->setPlainText(errorMsg);
            return;
        }

        const QByteArray data = reply->readAll();
        const QJsonDocument doc = QJsonDocument::fromJson(data);

        QString translation;
        if (doc.isObject()) {
            const auto choices = doc.object()["choices"].toArray();
            if (!choices.isEmpty())
                translation = choices[0].toObject()["message"]
                                  .toObject()["content"]
                                  .toString()
                                  .trimmed();
        }

        if (translation.isEmpty()) {
            translation = Loc::get("error.response");
            qWarning() << "[MANUAL] Pusta odpowiedź z LM Studio";
            m_resultField->setStyleSheet("QTextEdit#result { color: #888888; }");
        } else {
            qInfo() << "[MANUAL] Tłumaczenie otrzymane:" << translation.left(50);
            m_resultField->setStyleSheet("");
        }

        m_lastTranslation = translation;
        m_resultField->setPlainText(translation);
        m_copyButton->setEnabled(true);
    });
}
