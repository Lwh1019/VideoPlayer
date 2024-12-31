#pragma once

#include <QSlider>
#include <QMouseEvent>

class CustomSlider : public QSlider
{
    Q_OBJECT

public:
    CustomSlider(QWidget* parent);
    ~CustomSlider();
protected:
    void mousePressEvent(QMouseEvent *ev);
    void mouseReleaseEvent(QMouseEvent *ev);
    void mouseMoveEvent(QMouseEvent *ev);
signals:
    void SigCustomSliderValueChanged();

private:
    bool IsPressed = false;
};
