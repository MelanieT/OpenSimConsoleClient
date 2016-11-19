#ifndef ADDCONNDIALOG_H
#define ADDCONNDIALOG_H

#include <QDialog>

class QLineEdit;
class QSpinBox;

namespace Ui {
class AddConnDialog;
}

class AddConnDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddConnDialog(QWidget *parent = 0);
    ~AddConnDialog();

    QLineEdit *Name;
    QLineEdit *Host;
    QSpinBox *Port;
    QLineEdit *User;
    QLineEdit *Pass;

public slots:
    void OnCancel();
    void OnOK();

private:
    Ui::AddConnDialog *ui;
};

#endif // ADDCONNDIALOG_H
