#include "CustomSlider.h"
#include "globalhelper.h"

CustomSlider::CustomSlider(QWidget *parent)
    : QSlider(parent)
{
    this->setMaximum(MAX_SLIDER_VALUE);
}

CustomSlider::~CustomSlider()
{
}

void CustomSlider::mousePressEvent(QMouseEvent *ev)
{
    QSlider::mousePressEvent(ev);
    double pos = ev->pos().x() / (double)width();
    setValue(pos * (maximum() - minimum()) + minimum());

    emit SigCustomSliderValueChanged();
    IsPressed = true;
}

void CustomSlider::mouseReleaseEvent(QMouseEvent *ev)
{
    QSlider::mouseReleaseEvent(ev);
    IsPressed = false;
}

void CustomSlider::mouseMoveEvent(QMouseEvent *ev)
{
    QSlider::mouseMoveEvent(ev);
    if (IsPressed)
    {
        double pos = ev->pos().x() / (double)width();
        setValue(pos * (maximum() - minimum()) + minimum());
        emit SigCustomSliderValueChanged();
    }
}
