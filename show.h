// 视频显示界面
// Editor: Liwh 2024/12/22
#pragma once

#include <QWidget>
#include <QMimeData>
#include <QDebug>
#include <QTimer>
#include <QDragEnterEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QActionGroup>
#include <QAction>

#include "videoctl.h"
#include "ui_show.h"

class Show : public QWidget,public Ui::Show
{
    Q_OBJECT

public:
    Show(QWidget *parent);
    ~Show();
	bool Init();

    bool eventFilter(QObject* obj, QEvent* event);

protected:
    void dropEvent(QDropEvent *event);

    void dragEnterEvent(QDragEnterEvent *event);
    void resizeEvent(QResizeEvent *event);
    void keyReleaseEvent(QKeyEvent *event);


    void mousePressEvent(QMouseEvent* event);

    void contextMenuEvent(QContextMenuEvent* event) override;

public:
    void OnPlay(QString strFile);
    void OnStopFinished();
    void OnFrameDimensionsChanged(int nFrameWidth, int nFrameHeight);

private:

	void OnDisplayMsg(QString strMsg);


    void OnTimerShowCursorUpdate();

    void OnActionsTriggered(QAction *action);
private:

	bool ConnectSignalSlots();


    void ChangeShow();
signals:
    void SigOpenFile(QString strFileName);///< 增加视频文件
	void SigPlay(QString strFile); ///<播放
								   
	void SigFullScreen();//全屏播放
    void SigPlayOrPause();
    void SigStop();
    void SigShowMenu();

    void SigSeekForward();
    void SigSeekBack();
    void SigAddVolume();
    void SigSubVolume();

public slots:
    void OnResolutionChanged(bool success,QString outputFilePath,double pos);
private:
    Ui::Show *ui;

    int LastFrameWidth;
    int LastFrameHeight;

    QTimer timerShowCursor;

    QMenu Menu;
    QActionGroup ActionGroup;
};
