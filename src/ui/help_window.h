#ifndef HELP_WINDOW_H
#define HELP_WINDOW_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>

class HelpWindow : public QWidget
{
    Q_OBJECT

public:
    explicit HelpWindow(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    void setupUI();
};

#endif
