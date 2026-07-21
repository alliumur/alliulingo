#pragma once
#include <QWidget>
#include <QLabel>
#include <QList>

class PromptInfoWindow : public QWidget
{
    Q_OBJECT
public:
    explicit PromptInfoWindow(QWidget *parent = nullptr);

    static void closeAll();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void setupUI();

    static QList<PromptInfoWindow*> s_activeWindows;
};
