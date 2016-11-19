#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>

namespace Ui {
class PreferencesDialog;
}

class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget *parent = 0);
    ~PreferencesDialog();

private slots:
    void on_blackOnWhite_toggled(bool checked);

    void on_useSystemFonts_toggled(bool checked);

public:
    Ui::PreferencesDialog *ui;

signals:
    void changed();
protected slots:
    void OnOK();
    void OnCancel();
};

#endif // PREFERENCESDIALOG_H
