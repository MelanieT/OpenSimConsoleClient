#ifndef ADDGROUPDIALOG_H
#define ADDGROUPDIALOG_H

#include <QDialog>

class QLineEdit;
class QCheckBox;

namespace Ui {
class AddGroupDialog;
}

class AddGroupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddGroupDialog(QWidget *parent = 0);
    ~AddGroupDialog();

    QLineEdit *Name;
    QLineEdit *Dns;
    QCheckBox *Dynamic;
    QLineEdit *Source;
    QLineEdit *User;
    QLineEdit *Pass;

protected slots:
    void OnOK();
    void OnCancel();
private:
    Ui::AddGroupDialog *ui;
};

#endif // ADDGROUPDIALOG_H
