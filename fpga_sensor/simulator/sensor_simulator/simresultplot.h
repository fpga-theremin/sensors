#ifndef SIMRESULTPLOT_H
#define SIMRESULTPLOT_H

#include <QWidget>
#include "simutils.h"

class SimResultPlot : public QWidget
{
    Q_OBJECT
protected:
    SimResultsItem _results;
public:
    explicit SimResultPlot(QWidget *parent = nullptr);
    void setSimResults(SimResultsItem * results);
    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;
protected:
    void paintEvent(QPaintEvent *event) override;
signals:

};

#endif // SIMRESULTPLOT_H
