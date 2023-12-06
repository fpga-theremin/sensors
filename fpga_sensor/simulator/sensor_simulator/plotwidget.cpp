#include "plotwidget.h"
#include <QPainter>
#include <QtMath>

PlotWidget::PlotWidget(SimParams * simParams, SimState * simState, QWidget *parent) : QWidget(parent)
{
    _simParams = simParams;
    _simState = simState;
    _pixelsPerSample = 12;
}

QSize PlotWidget::sizeHint() const
{
    return QSize(1400, 1000);
}

QSize PlotWidget::minimumSizeHint() const
{
    return QSize(600, 400);
}

void PlotWidget::paintEvent(QPaintEvent * /* event */)
{
    QPainter painter(this);

    int penWidth = 2;
    Qt::PenStyle style = Qt::PenStyle(Qt::SolidLine);
    Qt::PenCapStyle cap = Qt::PenCapStyle(Qt::RoundCap);
    Qt::PenJoinStyle join = Qt::PenJoinStyle(Qt::RoundJoin);
    QPen pen(QColor(128, 128, 128), penWidth, style, cap, join);

    // half-period bounds
    QPen pen1(QColor(192, 240, 192), penWidth, style, cap, join);

    //QPen penMul1(QColor(64, 64, 255, 64), penWidth, style, cap, join);
    //QPen penMul2(QColor(255, 0, 128, 64), penWidth, style, cap, join);
    QBrush brushMul1(QColor(64, 64, 192, 96));
    QBrush brushMul2(QColor(192, 0, 128, 96));

    QPen pen2(QColor(0, 0, 220), penWidth, style, cap, join);
    QPen pen3(QColor(192, 0, 50), penWidth, style, cap, join);

    QPen pen4(QColor(128, 192, 64), penWidth, style, cap, join);
    QPen pen5(QColor(48, 128, 32), penWidth, style, cap, join);
    painter.setPen(pen);

    QBrush brush(QColor(250, 255, 250));
    painter.setBrush(brush);

    int w = width();
    int h = height();
    painter.fillRect(QRect(0, 0, w, h), brush);
    int xscale = _pixelsPerSample;
    int xsamples = (w + xscale - 1) / xscale;
    int sample0 = 0;
    // common marks
    int yscale = h / (1<<(_simParams->ncoValueBits));
    int y0 = (h)/2;
    painter.drawLine(QPoint(0, y0), QPoint(w, y0));

    // senseMulBase scale
    int mulBits = _simParams->ncoValueBits + _simParams->adcBits;
    double yscalef = (double)(h*0.9) / (1<<(mulBits-1));
    // sense*base1 (cos)
    {
        int yy = -(int)round(_simState->avgMulBase1 * yscalef) + y0;
        painter.fillRect( 0, yy, w, 1, brushMul1);
    }
    for (int i = 0; i < xsamples; i++) {
        int x = sample0 + i;
        int yy = (int)round(_simState->senseMulBase1[x] * yscalef);
        painter.fillRect( x*xscale, y0, xscale/2-1, -yy, brushMul1);
    }
    // sense*base2 (sin)
    {
        int yy = -(int)round(_simState->avgMulBase2 * yscalef) + y0;
        painter.fillRect( 0, yy, w, 1, brushMul2);
    }
    for (int i = 0; i < xsamples; i++) {
        int x = sample0 + i;
        int yy = (int)round(_simState->senseMulBase2[x] * yscalef);
        painter.fillRect( x*xscale+xscale/2+1, y0, xscale/2-1, -yy, brushMul2);
    }

    // sense signal sign change (half-period) marks
    for (int i = 0; i < xsamples; i++) {
        int x = sample0 + i;
        if (_simState->periodIndex[i+1] != _simState->periodIndex[i]) {
            // draw period mark
            painter.setPen(pen1);
            painter.drawLine(QPoint(x*xscale + xscale, 0), QPoint(x*xscale + xscale, h));

            // draw sum values
            QString s;
            int fh = painter.fontMetrics().height();
            painter.setPen(pen);
            int nextPeriod = _simState->periodIndex[i+1];
            int sum1 = _simState->periodSumBase1[nextPeriod];
            int sum2 = _simState->periodSumBase2[nextPeriod];
            double angle = - atan2(sum2, sum1) / M_PI / 2;
            double err = angle - _simParams->sensePhaseShift;
            s = QString(" %1 / %2")
                    .arg(sum1)
                    .arg(sum2);
            painter.drawText(QPoint(x * xscale + xscale + 5, fh + 4), s);
            s = QString(" %1 (%2)")
                    .arg(angle, 0, 'f', 6)
                    .arg(err, 0, 'f', 6);
            painter.drawText(QPoint(x * xscale + xscale + 5, fh*2 + 4), s);

            // bottom: avg for 2 halfperiods
            sum1 = _simState->periodSumBase1[nextPeriod] + _simState->periodSumBase1[nextPeriod+1];
            sum2 = _simState->periodSumBase2[nextPeriod] + _simState->periodSumBase2[nextPeriod+1];
            angle = - atan2(sum2, sum1) / M_PI / 2;
            err = angle - _simParams->sensePhaseShift;
            s = QString(" %1 (%2)")
                    .arg(angle, 0, 'f', 6)
                    .arg(err, 0, 'f', 6);
            painter.drawText(QPoint(x * xscale + xscale + 5, h - fh*1 - 4), s);

            // 2 more periods
            sum1 += _simState->periodSumBase1[nextPeriod+2] + _simState->periodSumBase1[nextPeriod+3];
            sum2 += _simState->periodSumBase2[nextPeriod+2] + _simState->periodSumBase2[nextPeriod+3];
            angle = - atan2(sum2, sum1) / M_PI / 2;
            err = angle - _simParams->sensePhaseShift;
            s = QString(" %1 (%2)")
                    .arg(angle, 0, 'f', 6)
                    .arg(err, 0, 'f', 6);
            painter.drawText(QPoint(x * xscale + xscale + 5, h - fh*2 - 4), s);
        }
    }

    // NCO Y scale
    yscalef = (h * 0.95) / (double)(1<<(_simParams->ncoValueBits));

    // base1 (cos)
    painter.setPen(pen2);
    for (int i = 0; i < xsamples; i++) {
        int x = sample0 + i;
        int y = y0 - (int)(_simState->base1[x] * yscalef);
        int nexty = y0 - (int)(_simState->base1[x+1] * yscalef);
        painter.drawLine(QPoint(x*xscale, y), QPoint(x*xscale + xscale, y));
        painter.drawLine(QPoint(x*xscale + xscale, y), QPoint(x*xscale + xscale, nexty));

    }
    // base2 (sin)
    painter.setPen(pen3);
    for (int i = 0; i < xsamples; i++) {
        int x = sample0 + i;
        int y = y0 - (int)(_simState->base2[x] * yscalef);
        int nexty = y0 - (int)(_simState->base2[x+1] * yscalef);
        painter.drawLine(QPoint(x*xscale, y), QPoint(x*xscale + xscale, y));
        painter.drawLine(QPoint(x*xscale + xscale, y), QPoint(x*xscale + xscale, nexty));

    }

    // Sense Y scale
    yscalef = 0.95 * h / (1<<(_simParams->adcBits));

    // senseExact
    painter.setPen(pen4);
    QPoint exactPoints[SP_SIM_MAX_SAMPLES];
    int pointCount = 0;
    for (int i = 0; i <= xsamples; i++) {
        int x = sample0 + i;
        double sense = _simState->senseExact[x];
        int y = y0 - (int)(sense * yscalef);// + yscale/2;
        QPoint pt = QPoint(x*xscale, y);
        exactPoints[pointCount++] = pt;

    }
    painter.drawPolyline(exactPoints, pointCount);

    // sense with noise quantized
    painter.setPen(pen5);
    for (int i = 0; i < xsamples; i++) {
        int x = sample0 + i;
        int y = y0 - (int)(_simState->sense[x] * yscalef);
        int nexty = y0 - (int)(_simState->sense[x+1] * yscalef);
        painter.drawLine(QPoint(x*xscale, y), QPoint(x*xscale + xscale, y));
        painter.drawLine(QPoint(x*xscale + xscale, y), QPoint(x*xscale + xscale, nexty));

    }

}

