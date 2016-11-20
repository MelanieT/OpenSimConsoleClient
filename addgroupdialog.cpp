#include "addgroupdialog.h"
#include "ui_addgroupdialog.h"

AddGroupDialog::AddGroupDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AddGroupDialog)
{
    ui->setupUi(this);

    Name = ui->Name;
    Dns = ui->Dns;
    Dynamic = ui->dynamic;
    Source = ui->source;
    User = ui->user;
    Pass = ui->pass;

    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(OnOK()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(OnCancel()));
}

AddGroupDialog::~AddGroupDialog()
{
    delete ui;
}

void AddGroupDialog::OnOK()
{
    done(0);
}

void AddGroupDialog::OnCancel()
{
    done(-1);
}
