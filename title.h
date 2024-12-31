// 标题界面：显示名称和视频名
// Editor: Liwh

#pragma once

#include <QWidget>
#include <QMouseEvent>
#include <QMenu>
#include <QActionGroup>
#include <QAction>
#include <QFile>
#include <QPainter>
#include <QtMath>
#include <QDebug>
#include <QAbstractItemView>
#include <QMimeData>
#include <QSizeGrip>
#include <QWindow>
#include <QScreen>
#include <QRect>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonParseError>

#include "ui_title.h"


class Title : public QWidget,public Ui::Title
{
    Q_OBJECT

public:
    explicit Title(QWidget *parent = 0);
    ~Title();
    bool Init();
private:
    void paintEvent(QPaintEvent *event);
    void mouseDoubleClickEvent(QMouseEvent *event);
    void resizeEvent(QResizeEvent *event);

    void ChangeMovieNameShow();
    bool InitUi();

    void OpenFile();
public:

    void OnChangeMaxBtnStyle(bool bIfMax);
    void OnPlay(QString strMovieName);
    void OnStopFinished();

    void OnMenuBtnClicked();
signals:
    void SigCloseBtnClicked();	//< 点击关闭按钮
    void SigMinBtnClicked();	//< 点击最小化按钮
    void SigMaxBtnClicked();	//< 点击最大化按钮
    void SigDoubleClicked();	//< 双击标题栏

    void SigFullScreenBtnClicked(); ///< 点击全屏按钮

    void SigOpenFile(QString strFileName); //打开文件
    void SigShowMenu();
private:
    Ui::Title *ui;

    QString MovieName;

    QMenu Menu;
    QActionGroup ActionGroup;
};
