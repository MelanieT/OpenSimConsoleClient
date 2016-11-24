#ifndef SPLASHDIALOG_H
#define SPLASHDIALOG_H

#include <QDialog>

namespace Ui {
class SplashDialog;
}

class SplashDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SplashDialog(QWidget *parent = 0, Qt::WindowFlags = 0);
    ~SplashDialog();

    bool isSplash;

    void show();
protected:
    void paintEvent(QPaintEvent *event);
private:
    Ui::SplashDialog *ui;
};

#endif // SPLASHDIALOG_H
