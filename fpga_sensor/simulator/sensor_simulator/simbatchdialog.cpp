#include "simbatchdialog.h"
#include <QBoxLayout>
#include <QTextEdit>
#include <QLineEdit>

SimBatchDialog::SimBatchDialog(QWidget * parent)
    : QDialog(parent)
{
    setWindowTitle("Run simulation batch");
    QLayout * layout = new QVBoxLayout();
    layout->addWidget(new QTextEdit(this));
    setLayout(layout);
}
