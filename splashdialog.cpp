#include "splashdialog.h"
#include "ui_splashdialog.h"
#include <QPainter>
#include <QImage>
#include <QTimer>

SplashDialog::SplashDialog(QWidget *parent, Qt::WindowFlags f) :
    QDialog(parent, f),
    isSplash(false),
    ui(new Ui::SplashDialog)
{
    ui->setupUi(this);
}

SplashDialog::~SplashDialog()
{
    delete ui;
}

void SplashDialog::show()
{
    if (isSplash)
    {
        ui->buttonBox->hide();
        QTimer::singleShot(2500, [this]() -> void { close(); deleteLater(); });
    }
    QDialog::show();

}

void SplashDialog::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    QImage img(":/Icons/Splash.png");

    p.drawImage(0, 0, img);

    if (isSplash)
    {
        p.setPen(QPen(QColor(0, 0, 0)));
        p.drawRect(0, 0, width() - 1, height() - 1);
    }
}
