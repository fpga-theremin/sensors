#include "simbatchdialog.h"
#include <QBoxLayout>
#include <QFormLayout>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QStatusBar>
#include <QLabel>

#define END_OF_LIST -1000000

SimBatchDialog::SimBatchDialog(QWidget * parent)
    : QDialog(parent)
{
    _freqVariations = 50;
    _phaseVariations = 50;
    _frequencyStep = 1.00142466;
    _phaseStep = 0.0065441;

    setWindowTitle("Run simulation batch");
    QLayout * layout = new QVBoxLayout();
    QLayout * top = new QHBoxLayout();

    QFormLayout * _simParamsLayout = new QFormLayout();
    _simParamsLayout->setSpacing(10);
    const int variations[] = {10, 20, 30, 40, 50, 100, END_OF_LIST};
    QComboBox * _cbFreqVariations = createIntComboBox(&_freqVariations, variations, 1);
    QComboBox * _cbPhaseVariations = createIntComboBox(&_phaseVariations, variations, 1);
    //QLineEdit * _edFrequency = createDoubleValueEditor(&_simParams.frequency, 100000, 9000000, 2);
    //_edFrequency->setMax
    _simParamsLayout->addRow(new QLabel("Frequency variations"), _cbFreqVariations);

    const double freqStep[] = {1.00025294, 1.00142466, 1.0135345, 1.03837645, END_OF_LIST};
    _simParamsLayout->addRow(new QLabel("Frequency step mult"), createDoubleComboBox(&_frequencyStep, freqStep));

    _simParamsLayout->addRow(new QLabel("Phase variations"), _cbPhaseVariations);

    const double phaseStep[] = {0.000182734, 0.0024453565, 0.0065441, 0.01134385, END_OF_LIST};
    _simParamsLayout->addRow(new QLabel("Phase step"), createDoubleComboBox(&_phaseStep, phaseStep));
    top->addItem(_simParamsLayout);

    QPushButton * btnStart = new QPushButton("Start");
    QPushButton * btnClose = new QPushButton("Close");
    top->addWidget(btnStart);
    top->addWidget(btnClose);
    layout->addItem(top);
    QTextEdit * textEdit = new QTextEdit(this);
    textEdit->setMinimumSize(QSize(600, 400));
    layout->addWidget(textEdit);
    QStatusBar * statusBar = new QStatusBar(this);
    layout->addWidget(statusBar);
    setLayout(layout);
    QLabel * statusLabel = new QLabel("Press Start to execute simulation suite", this);
    statusBar->addWidget(statusLabel);
}

QComboBox * SimBatchDialog::createIntComboBox(int * field, const int * values, int multiplier) {
    QComboBox * cb = new QComboBox();
    int defIndex = 0;
    for (int i = 0; values[i] != END_OF_LIST; i++) {
        int value = values[i];
        if (value*multiplier == *field)
            defIndex = i;
        cb->addItem(QString("%1").arg(value), value);
    }
    cb->setCurrentIndex(defIndex);
    connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged),
        [=](int index){ *field = cb->currentData().toInt() * multiplier;
        //recalculate();
    });
    return cb;
}

QComboBox * SimBatchDialog::createDoubleComboBox(double * field, const double * values) {
    QComboBox * cb = new QComboBox();
    int defIndex = 0;
    for (int i = 0; values[i] > END_OF_LIST; i++) {
        double value = values[i];
        if ((value >= *field - 0.0000000001) && (value <= *field + 0.0000000001))
            defIndex = i;
        cb->addItem(QString("%1").arg(value), value);
    }
    cb->setCurrentIndex(defIndex);
    connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged),
        [=](int index){ *field = cb->currentData().toDouble();
        //recalculate();
    });
    return cb;
}

void SimThread::runSimulation(SimParams * newParams, int freqVariations, double freqStep, int phaseVariations, double phaseStep ) {
    delete newParams;
    SimParams params = *newParams;
}
