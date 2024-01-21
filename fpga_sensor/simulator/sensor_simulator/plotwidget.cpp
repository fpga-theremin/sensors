#include "plotwidget.h"
#include <QPainter>
#include <QtMath>
#include <QWheelEvent>

PlotWidget::PlotWidget(SimParams * simParams, SimState * simState, QScrollBar * scrollBar, QWidget *parent) : QWidget(parent)
{
    _simParams = simParams;
    _simState = simState;
    _pixelsPerSample = 32;
    _scrollX = 0;
    _scrollBar = scrollBar;
    connect(_scrollBar, QOverload<int>::of(&QScrollBar::valueChanged),
        [=](int value){
        setScrollPercent(value);
    });
}

void PlotWidget::setPixelsPerSample(int n) {
    _pixelsPerSample = n;
    if (_pixelsPerSample < 3)
        _pixelsPerSample = 3;
    else if (_pixelsPerSample > 48)
        _pixelsPerSample = 48;
    update();
}

void PlotWidget::setScrollPercent(int n) {
    _scrollX = n * 2000 / 100;
    if (_scrollX < 0)
        _scrollX = 0;
    if (_scrollX > 2000)
        _scrollX = 2000;
    update();
}

int PlotWidget::getScrollPercent() {
    int p = _scrollX * 100 / 2000;
    if (p < 0)
        p = 0;
    if (p > 100)
        p = 100;
    return p;
}

QSize PlotWidget::sizeHint() const
{
    return QSize(900, 800);
}

QSize PlotWidget::minimumSizeHint() const
{
    return QSize(700, 600);
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
    int sample0 = _scrollX;
    // common marks
    //int yscale = h / (1<<(_simParams->ncoValueBits));
    int y0 = (h)/2;
    painter.drawLine(QPoint(0, y0), QPoint(w, y0));

    // senseMulBase scale
    int mulBits = _simParams->ncoValueBits + _simParams->adcBits - _simParams->mulDropBits;
    double yscalef = (double)(h*0.9) / ((uint64_t)1<<(mulBits-1));
    int dx = xscale/2-1;
    if (dx < 2)
        dx = 2;
    if (dx > 10)
        dx = 10;
    // sense*base1 (cos)
    {
        int yy = -(int)round(_simState->avgMulBase1 * yscalef) + y0;
        painter.fillRect( 0, yy, w, 1, brushMul1);
    }
    for (int i = 0; i < xsamples; i++) {
        int x = sample0 + i;
        int yy = (int)round(_simState->senseMulBase1[x] * yscalef);
        painter.fillRect( i*xscale, y0, dx, -yy, brushMul1);
    }
    // sense*base2 (sin)
    {
        int yy = -(int)round(_simState->avgMulBase2 * yscalef) + y0;
        painter.fillRect( 0, yy, w, 1, brushMul2);
    }
    for (int i = 0; i < xsamples; i++) {
        int x = sample0 + i;
        int yy = (int)round(_simState->senseMulBase2[x] * yscalef);
        painter.fillRect( i*xscale+xscale - dx, y0, dx, -yy, brushMul2);
    }

    // sense signal sign change (half-period) marks
    for (int i = 0; i < xsamples; i++) {
        int x = sample0 + i;
        if (_simState->periodIndex[x+1] != _simState->periodIndex[x]) {
            // draw period mark
            painter.setPen(pen1);
            painter.drawLine(QPoint(i*xscale + xscale, 0), QPoint(i*xscale + xscale, h));

            // draw sum values
            QString s;
            int fh = painter.fontMetrics().height();
            painter.setPen(pen);
            int nextPeriod = _simState->periodIndex[x+1];
            double angle = _simState->phaseForPeriods(nextPeriod, 1);
            double err = _simParams->phaseError(angle);
            int exactBits = SimParams::exactBits(err);
            s = QString(" 1h: %1 (%2) %3 bits")
                    .arg(angle, 0, 'f', 6)
                    .arg(err, 0, 'f', 6)
                    .arg(exactBits)
                    ;
            painter.drawText(QPoint(i * xscale + xscale + 5, fh*2 + 4), s);

            // bottom: avg for 1 period (2 halfperiods)
            angle = _simState->phaseForPeriods(nextPeriod, 2);
            err = _simParams->phaseError(angle);
            exactBits = SimParams::exactBits(err);
            s = QString(" 1p: %1 (%2) %3 bits %4ns")
                    .arg(angle, 0, 'f', 6)
                    .arg(err, 0, 'f', 6)
                    .arg(exactBits)
                    .arg(_simParams->phaseErrorToNanoSeconds(err), 0, 'f', 3)
                    ;
            painter.drawText(QPoint(i * xscale + xscale + 5, h - fh*1 - 4), s);

            // bottom: avg for 2 periods (4 halfperiods)
            angle = _simState->phaseForPeriods(nextPeriod, 4);
            err = _simParams->phaseError(angle);
            exactBits = SimParams::exactBits(err);
            s = QString(" 2p: %1 (%2) %3 bits %4ns")
                    .arg(angle, 0, 'f', 6)
                    .arg(err, 0, 'f', 6)
                    .arg(exactBits)
                    .arg(_simParams->phaseErrorToNanoSeconds(err), 0, 'f', 3)
                    ;
            painter.drawText(QPoint(i * xscale + xscale + 5, h - fh*2 - 4), s);

            // bottom: avg for 4 periods (8 halfperiods)
            angle = _simState->phaseForPeriods(nextPeriod, 8);
            err = _simParams->phaseError(angle);
            exactBits = SimParams::exactBits(err);
            s = QString(" 4p: %1 (%2) %3 bits %4ns")
                    .arg(angle, 0, 'f', 6)
                    .arg(err, 0, 'f', 6)
                    .arg(exactBits)
                    .arg(_simParams->phaseErrorToNanoSeconds(err), 0, 'f', 3)
                    ;
            painter.drawText(QPoint(i * xscale + xscale + 5, h - fh*3 - 4), s);

            // bottom: avg for 8 periods (16 halfperiods)
            angle = _simState->phaseForPeriods(nextPeriod, 16);
            err = _simParams->phaseError(angle);
            exactBits = SimParams::exactBits(err);
            s = QString(" 8p: %1 (%2) %3 bits %4ns")
                    .arg(angle, 0, 'f', 6)
                    .arg(err, 0, 'f', 6)
                    .arg(exactBits)
                    .arg(_simParams->phaseErrorToNanoSeconds(err), 0, 'f', 3)
                    ;
            painter.drawText(QPoint(i * xscale + xscale + 5, h - fh*4 - 4), s);
        }
    }

    Array<QPoint> exactPoints;
    // y of center point of ADC*sin  (base1)
    exactPoints.clear();
    for (int i = 0; i <= xsamples; i++) {
        int x = sample0 + i;
        // base1 is sin, base2 is cos
        // y = adc*cos + adc*sin
        int64_t centerBase1 = (_simState->senseMulBase2[x] + _simState->senseMulBase1[x]) / 2;
        int yy1 = y0 - (int)round(centerBase1 * yscalef);
        QPoint pt = QPoint(i*xscale, yy1);
        exactPoints.add(pt);
    }
    painter.drawPolyline(exactPoints.data(), exactPoints.length());

    // x of center point of ADC*cos (base2)
    exactPoints.clear();
    for (int i = 0; i <= xsamples; i++) {
        int x = sample0 + i;
        // base1 is sin, base2 is cos
        // x = adc*cos - adc*sin
        int64_t centerBase2 = (_simState->senseMulBase2[x] - _simState->senseMulBase1[x]) / 2;
        int yy2 = y0 - (int)round(centerBase2 * yscalef);
        QPoint pt = QPoint(i*xscale, yy2);
        exactPoints.add(pt);
    }
    painter.drawPolyline(exactPoints.data(), exactPoints.length());


    // NCO Y scale
    yscalef = (h * 0.95) / (double)(1<<(_simParams->ncoValueBits));

    // base1 (cos)
    painter.setPen(pen2);
    for (int i = 0; i < xsamples; i++) {
        int x = sample0 + i;
        int y = y0 - (int)(_simState->base1[x] * yscalef);
        int nexty = y0 - (int)(_simState->base1[x+1] * yscalef);
        painter.drawLine(QPoint(i*xscale, y), QPoint(i*xscale + xscale, y));
        painter.drawLine(QPoint(i*xscale + xscale, y), QPoint(i*xscale + xscale, nexty));

    }
    // base2 (sin)
    painter.setPen(pen3);
    for (int i = 0; i < xsamples; i++) {
        int x = sample0 + i;
        int y = y0 - (int)(_simState->base2[x] * yscalef);
        int nexty = y0 - (int)(_simState->base2[x+1] * yscalef);
        painter.drawLine(QPoint(i*xscale, y), QPoint(i*xscale + xscale, y));
        painter.drawLine(QPoint(i*xscale + xscale, y), QPoint(i*xscale + xscale, nexty));

    }

    // Sense Y scale
    yscalef = 0.95 * h / (1<<(_simParams->adcBits));

    // senseExact
    painter.setPen(pen4);
    exactPoints.clear();
    for (int i = 0; i <= xsamples; i++) {
        int x = sample0 + i;
        double sense = _simState->senseExact[x];
        int y = y0 - (int)(sense * yscalef);// + yscale/2;
        QPoint pt = QPoint(i*xscale, y);
        exactPoints.add(pt);
    }
    painter.drawPolyline(exactPoints.data(), exactPoints.length());


    // sense with noise quantized
    painter.setPen(pen5);
    for (int i = 0; i < xsamples; i++) {
        int x = sample0 + i;
        int y = y0 - (int)(_simState->sense[x] * yscalef);
        int nexty = y0 - (int)(_simState->sense[x+1] * yscalef);
        painter.drawLine(QPoint(i*xscale, y), QPoint(i*xscale + xscale, y));
        painter.drawLine(QPoint(i*xscale + xscale, y), QPoint(i*xscale + xscale, nexty));

    }

}

void PlotWidget::zoomIn() {
    setPixelsPerSample(getPixelsPerSample() + 1);
}

void PlotWidget::zoomOut() {
    setPixelsPerSample(getPixelsPerSample() - 1);
}

void PlotWidget::wheelEvent(QWheelEvent * event) {
    //int x = event->position().toPoint().x();
    //int y = event->position().toPoint().y();
    Qt::MouseButtons mouseFlags = event->buttons();
    Qt::KeyboardModifiers keyFlags = event->modifiers();
    QPoint angleDelta = event->angleDelta();
    //Qt::ScrollPhase phase = event->phase();
    //qDebug("wheelEvent(x=%d, y=%d, mouse=%x, keymodifiers=%x delta=(%d %d) phase=%d)", x, y, mouseFlags, keyFlags, angleDelta.x(), angleDelta.y(), phase);


    // vscroll
    if (angleDelta.y() != 0 && angleDelta.x() == 0 && mouseFlags == 0 && keyFlags == 0) {
        setScrollPercent(getScrollPercent() + (angleDelta.y() > 0 ? 1 : -1));
        _scrollBar->setSliderPosition(getScrollPercent());
        event->accept();
        return;
    }
    // touch pad hscroll gesture
    if (angleDelta.y() == 0 && angleDelta.x() != 0 && mouseFlags == 0 && keyFlags == 0) {
        setScrollPercent(getScrollPercent() + (angleDelta.x() > 0 ? -1 : 1));
        _scrollBar->setSliderPosition(getScrollPercent());
        event->accept();
        return;
    }
    // mouse Shift + wheel  (shcroll)
    if (angleDelta.x() == 0 && angleDelta.y() != 0 && mouseFlags == 0 && (keyFlags & Qt::ControlModifier) ==  0
            && (keyFlags & Qt::ShiftModifier) !=  0) {
        setScrollPercent(getScrollPercent() + angleDelta.y());
        _scrollBar->setSliderPosition(getScrollPercent());
        event->accept();
        return;
    }
    // hscale touch pad hscroll + Ctrl
    if (angleDelta.y() == 0 && angleDelta.x() != 0 && mouseFlags == 0 && (keyFlags & Qt::ControlModifier) !=  0
            && (keyFlags & Qt::ShiftModifier) ==  0) {
        setPixelsPerSample(getPixelsPerSample() + angleDelta.x());
        //return angleDelta.x();
        event->accept();
        return;
    }
    // hscale mouse or touch pad vscroll + Shift + Ctrl
    if (angleDelta.y() != 0 && angleDelta.x() == 0 && mouseFlags == 0 && (keyFlags & Qt::ControlModifier) !=  0
             && (keyFlags & Qt::ShiftModifier) !=  0) {
        setPixelsPerSample(getPixelsPerSample() + angleDelta.y());
        //return angleDelta.y();
        event->accept();
        return;
    }
    // yscale: mouse wheel or touch pad vscroll + Ctrl
    if (angleDelta.x() == 0 && angleDelta.y() != 0 && mouseFlags == 0 && (keyFlags & Qt::ControlModifier) !=  0
            && (keyFlags & Qt::ShiftModifier) ==  0) {
        setPixelsPerSample(getPixelsPerSample() + angleDelta.y());
        //return angleDelta.y();
        event->accept();
        return;
    }
}

