#include "addconndialog.h"
#include "ui_addconndialog.h"

AddConnDialog::AddConnDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AddConnDialog)
{
    ui->setupUi(this);

    Name = ui->Name;
    Host = ui->Host;
    Port = ui->Port;
    User = ui->User;
    Pass = ui->Pass;

    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(OnOK()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(OnCancel()));
}

AddConnDialog::~AddConnDialog()
{
    delete ui;
}

void AddConnDialog::OnOK()
{
    done(0);
}

void AddConnDialog::OnCancel()
{
    done(-1);
}
