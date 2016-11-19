#include "preferencesdialog.h"
#include "ui_preferencesdialog.h"

PreferencesDialog::PreferencesDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PreferencesDialog)
{
    ui->setupUi(this);

    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(OnOK()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(OnCancel()));
}

PreferencesDialog::~PreferencesDialog()
{
    delete ui;
}

void PreferencesDialog::on_blackOnWhite_toggled(bool)
{
    emit changed();
}

void PreferencesDialog::on_useSystemFonts_toggled(bool)
{
    emit changed();
}

void PreferencesDialog::OnOK()
{
    done(0);
}

void PreferencesDialog::OnCancel()
{
    done(-1);
}
