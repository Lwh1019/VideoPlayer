// 主界面
// Editor: Liwh 2024/12/20
#pragma once
#include <QWidget>
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QMenu>
#include <QAction>
#include <QPropertyAnimation>
#include <QTimer>
#include <QMainWindow>

#include "playlist.h"
#include "title.h"
#include "ui_mainwid.h"

class MainWid : public QMainWindow,public Ui::MainWid
{
    Q_OBJECT

public:
    explicit MainWid(QMainWindow *parent = 0);
    ~MainWid();
    bool Init();
protected:
    void paintEvent(QPaintEvent *event);
    void enterEvent(QEvent *event);
    void leaveEvent(QEvent *event);
    void keyReleaseEvent(QKeyEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void contextMenuEvent(QContextMenuEvent* event);
private:
    bool ConnectSignalSlots();
    void OnCloseBtnClicked();
    void OnMinBtnClicked();
    void OnMaxBtnClicked();
    void OnShowOrHidePlaylist();

    void OnFullScreenPlay();

    void OnCtrlBarAnimationTimeOut();
    void OnFullscreenMouseDetectTimeOut();

    void OnCtrlBarHideTimeOut();
    void OnShowMenu();
    void OnShowAbout();
    void OpenFile();

    void OnSaveBmp();

    void OnShowSettingWid();

    void OnSavePeriod();

    void OnLoop();
    void OnList();
    void OnRandom();

    void InitMenu();
    void InitMenuActions();

    void MenuJsonParser(QJsonObject& json_obj, QMenu* menu);
    QMenu* AddMenuFun(QString menu_title, QMenu* menu);
    void AddActionFun(QString action_title, QMenu* menu, void(MainWid::* slot_addr)());

signals:

    void SigShowMax(bool bIfMax);
    void SigSeekForward();
    void SigSeekBack();
    void SigAddVolume();
    void SigSubVolume();
    void SigPlayOrPause();
    void SigOpenFile(QString strFilename);

    void SigClear(QString file);

    void SigVideoLoop(QString file);
private:
    Ui::MainWid *ui;

    bool isPlaying; // 正在播放


    int ShadowWidth; // 阴影宽度

    bool FullScreenPlay; ///< 全屏播放标志

    QPropertyAnimation *CtrlbarAnimationShow; //全屏时控制面板浮动显示
    QPropertyAnimation *CtrlbarAnimationHide; //全屏时控制面板浮动显示
    QRect CtrlBarAnimationShowRect;//控制面板显示区域
    QRect CtrlBarAnimationHideRect;//控制面板隐藏区域

    QTimer CtrlBarAnimationTimer;
    QTimer FullscreenMouseDetectTimer;//全屏时鼠标位置监测时钟
    bool FullscreenCtrlBarShow;
    QTimer CtrlBarHideTimer;

    Playlist Playlist;
    Title Title;

    bool MoveDrag;//移动窗口标志
    QPoint DragPosition;

    typedef void (MainWid::* MenuAction)();

    QMenu Menu;
    QAction ActFullscreen;

    QActionGroup* loopActionGroup;

    QMap<QString, MenuAction> Map_act;
};
