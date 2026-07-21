#include <QApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QIcon>
#include <QStyle>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QProcess>
#include <QUrl>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "core/hotkey_filter.h"
#include "core/config_manager.h"
#include "core/prompt_manager.h"
#include "core/localization_manager.h"
#include "network/llm_client.h"
#include <QPointer>
#include "ui/help_window.h"
#include "ui/prompt_info_window.h"
#include "ui/result_popup.h"

#ifdef NDEBUG
static void customMessageHandler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    if (type != QtCriticalMsg && type != QtFatalMsg)
        return;

    QString level = (type == QtCriticalMsg) ? "ERROR" : "FATAL";

    QString line = QString("[%1] [%2] %3\n")
        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"), level, msg);

    QFile logFile(QCoreApplication::applicationDirPath() + "/alliulingo.log");
    if (logFile.open(QIODevice::Append | QIODevice::Text))
        QTextStream(&logFile) << line;
}
#endif

static QString languageDisplayName(const QString& code)
{
    return Loc::get("lang." + code);
}

class StayOpenMenu : public QMenu
{
public:
    using QMenu::QMenu;

protected:
    void mouseReleaseEvent(QMouseEvent* event) override
    {
        QAction* action = activeAction();
        if (action && action->isEnabled() && action->isCheckable()) {
            action->trigger();
            return;
        }
        QMenu::mouseReleaseEvent(event);
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            QAction* action = activeAction();
            if (action && action->isEnabled() && action->isCheckable()) {
                action->trigger();
                return;
            }
        }
        QMenu::keyPressEvent(event);
    }
};

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"AlliulingoSingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }
#endif

    QApplication app(argc, argv);

#ifdef NDEBUG
    qInstallMessageHandler(customMessageHandler);
#endif

    Q_INIT_RESOURCE(resources);

    app.setQuitOnLastWindowClosed(false);
    app.setWindowIcon(QIcon(":/tray"));

    QSystemTrayIcon tray(QIcon(":/tray"));

    const QString stringsPath = QCoreApplication::applicationDirPath() + "/strings.json";
    LocalizationManager::instance().loadFromFile(stringsPath);

    const QString cfgPath = QCoreApplication::applicationDirPath() + "/alliulingo.cfg";
    ConfigManager config(cfgPath);

    PromptManager promptManager;

    if (!config.activePromptFile().isEmpty()) {
        const PromptData promptData = promptManager.getPromptData(config.activePromptFile());
        if (!promptData.skillContent.isEmpty()) {
            config.setSkill(promptData.skillContent);
            config.setUserContentPrefix(promptData.userContentPrefix);
            config.setApiKey(promptData.apiKey);

            if (!promptData.endpoint.isEmpty()) {
                config.setEndpoint(promptData.endpoint);
            }
            if (!promptData.model.isEmpty()) {
                config.setModel(promptData.model);
            }
            if (promptData.temperature >= 0.0) {
                config.setTemperature(promptData.temperature);
            }

            qInfo() << "[PROMPT] Wczytano aktywny prompt:" << config.activePromptFile();
        }
    }

    auto updateTrayTooltip = [&tray, &config]() {
        QString modeLine = config.engineName();
        if (config.translatorMode() == 0 && !config.activePromptFile().isEmpty()) {
            QString promptName = config.activePromptFile();
            if (promptName.endsWith(".txt", Qt::CaseInsensitive))
                promptName.chop(4);
            modeLine += "/" + promptName;
        }
        tray.setToolTip(QString("Alliulingo\nShift+Alt+X: %1 → %2\n%3")
                            .arg(languageDisplayName(config.sourceLang()),
                                 languageDisplayName(config.targetLang()),
                                 modeLine));
    };
    updateTrayTooltip();

    StayOpenMenu menu;

    QPointer<HelpWindow> helpWindow;
    QAction *helpAction = menu.addAction(Loc::get("menu.shortcuts"));
    QObject::connect(helpAction, &QAction::triggered, [&]() {
        ResultPopup::closeAll();
        PromptInfoWindow::closeAll();

        if (helpWindow) {
            helpWindow->raise();
            helpWindow->activateWindow();
            return;
        }
        helpWindow = new HelpWindow();
        helpWindow->show();
    });

    menu.addSeparator();

    StayOpenMenu *sourceLangMenu  = new StayOpenMenu(Loc::get("menu.translate_from"), &menu);
    menu.addMenu(sourceLangMenu);
    QActionGroup *sourceGroup     = new QActionGroup(&menu);
    sourceGroup->setExclusive(true);

    const QStringList availableLangs = config.availableOcrLanguages();
    for (const QString& langCode : availableLangs) {
        QAction *act = sourceLangMenu->addAction(languageDisplayName(langCode));
        act->setCheckable(true);
        act->setData(langCode);
        sourceGroup->addAction(act);

        if (langCode == config.sourceLang())
            act->setChecked(true);

        QObject::connect(act, &QAction::triggered, [&config, &updateTrayTooltip, langCode]() {
            config.setSourceLang(langCode);
            updateTrayTooltip();
            qInfo() << "[LANG] Zmieniono język źródłowy na:" << langCode;
        });
    }

    StayOpenMenu *targetLangMenu = new StayOpenMenu(Loc::get("menu.translate_to"), &menu);
    menu.addMenu(targetLangMenu);
    QActionGroup *targetGroup    = new QActionGroup(&menu);
    targetGroup->setExclusive(true);

    for (const QString& langCode : availableLangs) {
        QAction *act = targetLangMenu->addAction(languageDisplayName(langCode));
        act->setCheckable(true);
        act->setData(langCode);
        targetGroup->addAction(act);

        if (langCode == config.targetLang())
            act->setChecked(true);

        QObject::connect(act, &QAction::triggered, [&config, &updateTrayTooltip, langCode]() {
            config.setTargetLang(langCode);
            updateTrayTooltip();
            qInfo() << "[LANG] Zmieniono język docelowy na:" << langCode;
        });
    }

    menu.addSeparator();

    StayOpenMenu *promptMenu  = nullptr;

    StayOpenMenu *engineMenu  = new StayOpenMenu(Loc::get("menu.translation_method"), &menu);
    menu.addMenu(engineMenu);
    QActionGroup *engineGroup = new QActionGroup(&menu);
    engineGroup->setExclusive(true);

    QAction *actOnline = engineMenu->addAction(Loc::get("menu.translation_method.basic"));
    actOnline->setCheckable(true);

    QAction *actLmStudio = engineMenu->addAction(Loc::get("menu.translation_method.advanced"));
    actLmStudio->setCheckable(true);

    engineGroup->addAction(actOnline);
    engineGroup->addAction(actLmStudio);

    (config.translatorMode() == 1 ? actOnline : actLmStudio)->setChecked(true);

    QObject::connect(actOnline, &QAction::triggered, [&]() {
        config.setTranslatorMode(1);
        promptMenu->menuAction()->setEnabled(false);
        updateTrayTooltip();
        qInfo() << "[SYSTEM] Zmieniono metodę tłumaczenia na: Podstawowa (MyMemory)";
    });
    QObject::connect(actLmStudio, &QAction::triggered, [&]() {
        config.setTranslatorMode(0);
        promptMenu->menuAction()->setEnabled(true);
        updateTrayTooltip();
        qInfo() << "[SYSTEM] Zmieniono metodę tłumaczenia na: Zaawansowana (AI)";
    });

    promptMenu  = new StayOpenMenu(Loc::get("menu.ai_settings"), &menu);
    menu.addMenu(promptMenu);
    promptMenu->menuAction()->setEnabled(config.translatorMode() == 0);
    QActionGroup *promptGroup = new QActionGroup(&menu);
    promptGroup->setExclusive(true);

    const QString kMenuBase = R"(
        QMenu {
            background: rgba(45, 45, 45, 240);
            color: #E0E0E0;
            border: 1px solid rgba(100, 100, 100, 150);
        }
        QMenu::item:selected {
            background: rgba(80, 80, 80, 200);
        }
        QMenu::item:disabled {
            color: #606060;
        }
        QMenu::separator {
            height: 1px;
            background: rgba(255, 255, 255, 30);
            margin: 3px 8px;
        }
    )";
    const QString kMenuStylePlain = kMenuBase + R"(
        QMenu::item { padding: 4px 20px; }
    )";
    const QString kMenuStyleCheckable = kMenuBase + R"(
        QMenu::item { padding: 4px 20px 4px 4px; }
        QMenu::indicator { width: 14px; height: 14px; left: 3px; }
    )";
    menu.setStyleSheet(kMenuStylePlain);
    for (QMenu* m : { sourceLangMenu, targetLangMenu, engineMenu, promptMenu })
        m->setStyleSheet(kMenuStyleCheckable);

    QAction* promptAnchorSeparator = nullptr;

    auto refreshPromptMenu = [&]() {
        promptManager.clearCache();

        QAction* separator = promptAnchorSeparator;

        QList<QAction*> actions = promptMenu->actions();
        for (QAction* act : actions) {
            if (act == separator) break;
            promptMenu->removeAction(act);
            promptGroup->removeAction(act);
            delete act;
        }

        const QStringList availablePrompts = promptManager.availablePrompts();
        QStringList regularPrompts;
        for (const QString& promptFile : availablePrompts) {
            if (!promptFile.startsWith("#")) {
                regularPrompts << promptFile;
            }
        }

        QString defaultPrompt;
        QStringList userPrompts;
        for (const QString& promptFile : regularPrompts) {
            if (promptFile.compare("Default.txt", Qt::CaseInsensitive) == 0)
                defaultPrompt = promptFile;
            else
                userPrompts << promptFile;
        }
        userPrompts.sort(Qt::CaseInsensitive);

        if (regularPrompts.isEmpty()) {
            QAction *noPromptsAction = new QAction(Loc::get("menu.ai_settings.no_prompts"), promptMenu);
            noPromptsAction->setEnabled(false);
            promptMenu->insertAction(separator, noPromptsAction);
        } else {
            auto addPromptAction = [&](const QString& promptFile) {
                QString displayName = promptFile;
                if (displayName.endsWith(".txt", Qt::CaseInsensitive))
                    displayName.chop(4);

                QAction *act = new QAction(displayName, promptMenu);
                act->setCheckable(true);
                act->setData(promptFile);
                promptGroup->addAction(act);
                promptMenu->insertAction(separator, act);

                if (promptFile == config.activePromptFile())
                    act->setChecked(true);

                QObject::connect(act, &QAction::triggered, [&config, &promptManager, &updateTrayTooltip, promptFile, displayName]() {
                    const PromptData promptData = promptManager.getPromptData(promptFile);
                    if (promptData.skillContent.isEmpty()) {
                        qWarning() << "[PROMPT] Nie można wczytać pliku:" << promptFile;
                        return;
                    }

                    config.setActivePromptFile(promptFile);
                    config.setSkill(promptData.skillContent);
                    config.setUserContentPrefix(promptData.userContentPrefix);
                    config.setApiKey(promptData.apiKey);

                    if (!promptData.endpoint.isEmpty()) {
                        config.setEndpoint(promptData.endpoint);
                    }
                    if (!promptData.model.isEmpty()) {
                        config.setModel(promptData.model);
                    }
                    if (promptData.temperature >= 0.0) {
                        config.setTemperature(promptData.temperature);
                    }

                    updateTrayTooltip();
                    qInfo() << "[PROMPT] Aktywowano ustawienia tłumacza AI:" << displayName;
                });
            };

            if (!defaultPrompt.isEmpty())
                addPromptAction(defaultPrompt);

            if (!defaultPrompt.isEmpty() && !userPrompts.isEmpty())
                promptMenu->insertSeparator(separator);

            for (const QString& promptFile : userPrompts)
                addPromptAction(promptFile);
        }

        QList<QAction*> allActions = promptMenu->actions();

        QAction* openFolder = nullptr;
        QAction* editActive = nullptr;

        for (QAction* act : allActions) {
            if (act->isSeparator()) {
                continue;
            } else if (act->text() == Loc::get("menu.ai_settings.open_folder")) {
                openFolder = act;
            } else if (act->text() == Loc::get("menu.ai_settings.edit_active")) {
                editActive = act;
            }
        }

        if (editActive) {
            promptMenu->removeAction(editActive);
            delete editActive;
        }

        QAction *newEditActive = new QAction(Loc::get("menu.ai_settings.edit_active"), promptMenu);
        newEditActive->setEnabled(!config.activePromptFile().isEmpty());

        QObject::connect(newEditActive, &QAction::triggered, [&config, &promptManager]() {
            const QString activeFile = config.activePromptFile();
            if (activeFile.isEmpty()) {
                qWarning() << "[PROMPT] Brak aktywnego promptu do edycji";
                return;
            }

            const QString fullPath = promptManager.promptsDirectory() + "/" + activeFile;
            qInfo() << "[PROMPT] Otwieranie w Notatniku:" << fullPath;
            QProcess::startDetached("notepad.exe", QStringList() << fullPath);
        });

        if (openFolder) {
            promptMenu->insertAction(openFolder, newEditActive);
        } else {
            promptMenu->addAction(newEditActive);
        }

        qDebug() << "[PROMPT] Odświeżono menu - znaleziono" << availablePrompts.size() << "promptów";
    };

    refreshPromptMenu();

    QObject::connect(promptMenu, &QMenu::aboutToShow, refreshPromptMenu);

    promptAnchorSeparator = promptMenu->addSeparator();

    QAction *openPromptsFolder = promptMenu->addAction(Loc::get("menu.ai_settings.open_folder"));
    QObject::connect(openPromptsFolder, &QAction::triggered, [&promptManager]() {
        const QString promptsPath = promptManager.promptsDirectory();
        qInfo() << "[PROMPT] Otwieranie folderu:" << promptsPath;
        QDesktopServices::openUrl(QUrl::fromLocalFile(promptsPath));
    });

    promptMenu->addSeparator();

    QPointer<PromptInfoWindow> promptInfoWindow;
    QAction *infoAction = promptMenu->addAction(Loc::get("menu.ai_settings.help"));
    QObject::connect(infoAction, &QAction::triggered, [&promptInfoWindow]() {
        if (promptInfoWindow) {
            promptInfoWindow->raise();
            promptInfoWindow->activateWindow();
            return;
        }
        promptInfoWindow = new PromptInfoWindow();
        promptInfoWindow->show();
    });

    menu.addSeparator();
    QAction *exitAction = menu.addAction(Loc::get("menu.exit"));
    QObject::connect(exitAction, &QAction::triggered, &app, &QApplication::quit);

    tray.setContextMenu(&menu);
    tray.show();

    LlmClient llm(config, &tray, &promptManager, &app);

    HotkeyFilter filter(&tray, &llm, config);
    filter.setHelpWindow(&helpWindow);
    app.installNativeEventFilter(&filter);

    QObject::connect(&tray, &QSystemTrayIcon::activated, [&filter](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) {
            filter.openManualWindow(true);
        }
    });

    return app.exec();
}
