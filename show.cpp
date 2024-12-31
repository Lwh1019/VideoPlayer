// 视频显示界面
// Editor: Liwh 2024/12/22


#include <QDebug>
#include <QMutex>
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


#include "show.h"
#include "ui_show.h"
#include "globalhelper.h"
#include <QMessageBox>
#include "videoctl.h"

#pragma execution_character_set("utf-8")

QMutex show_rect;

double start = 0;

Show::Show(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Show),
    ActionGroup(this),
    Menu(this)
{
    ui->setupUi(this);

    setStyleSheet(GlobalHelper::GetQssStr("://res/qss/show.css"));
    setAcceptDrops(true);

    this->setAttribute(Qt::WA_OpaquePaintEvent);

    ui->label->setUpdatesEnabled(false);

    this->setMouseTracking(true);
    


    LastFrameWidth = 0;
    LastFrameHeight = 0;

    ActionGroup.addAction("全屏");
    ActionGroup.addAction("暂停");
    ActionGroup.addAction("停止");

    Menu.addActions(ActionGroup.actions());

    connect(VideoCtl::GetInstance(), &VideoCtl::ResolutionChanged,this,&Show::OnResolutionChanged);
    
}

Show::~Show()
{
    delete ui;
}

bool Show::Init()
{
    if (ConnectSignalSlots() == false)
    {
        return false;
    }
	return true;
}

void Show::OnFrameDimensionsChanged(int nFrameWidth, int nFrameHeight)
{
    qDebug() << "Show::OnFrameDimensionsChanged" << nFrameWidth << nFrameHeight;
    LastFrameWidth = nFrameWidth;
    LastFrameHeight = nFrameHeight;
    ChangeShow();
}

void Show::ChangeShow()
{
    QMutexLocker locker(&show_rect);

    if (LastFrameWidth == 0 && LastFrameHeight == 0)
    {
        ui->label->setGeometry(0, 0, width(), height());
        return;
    }
    float videoAspectRatio = static_cast<float>(LastFrameWidth) / static_cast<float>(LastFrameHeight);
    int containerWidth = this->width();
    int containerHeight = this->height(); 
    float containerAspectRatio = static_cast<float>(containerWidth) / static_cast<float>(containerHeight);

    int displayWidth, displayHeight, offsetX, offsetY;

    if (videoAspectRatio > containerAspectRatio)
    {
        displayWidth = containerWidth;
        displayHeight = static_cast<int>(containerWidth / videoAspectRatio);
        offsetX = 0;
        offsetY = (containerHeight - displayHeight) / 2; 
    }
    else
    {
        displayHeight = containerHeight;
        displayWidth = static_cast<int>(containerHeight * videoAspectRatio);
        offsetX = (containerWidth - displayWidth) / 2;
        offsetY = 0;
    }

    ui->label->setGeometry(offsetX, offsetY, displayWidth, displayHeight);
}

void Show::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction();
}

void Show::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);

    ChangeShow();
}

void Show::keyReleaseEvent(QKeyEvent *event)
{
    qDebug() << "Show::keyPressEvent:" << event->key();
    switch (event->key())
    {
    case Qt::Key_Return:
        SigFullScreen();
        break;
    case Qt::Key_Left:
        emit SigSeekBack();
        break;
    case Qt::Key_Right:
        qDebug() << "前进5s";
        emit SigSeekForward();
        break;
    case Qt::Key_Up:
        emit SigAddVolume();
        break;
    case Qt::Key_Down:
        emit SigSubVolume();
        break;
    case Qt::Key_Space:
        emit SigPlayOrPause();
        break;

    default:
        QWidget::keyPressEvent(event);
        break;
    }
}
void Show::mousePressEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::RightButton)
    {
        emit SigShowMenu();
    }

    QWidget::mousePressEvent(event);
}

void Show::OnDisplayMsg(QString strMsg)
{
	qDebug() << "Show::OnDisplayMsg " << strMsg;
}

void Show::OnPlay(QString strFile)
{
    LastFrameWidth = 0;
    LastFrameHeight = 0;
    ChangeShow();
    VideoCtl::GetInstance()->StartPlay(strFile, ui->label->winId(),start);
    start = 0.0;
}

void Show::OnStopFinished()
{
    update();
}


void Show::OnTimerShowCursorUpdate()
{
}

void Show::OnActionsTriggered(QAction *action)
{
    QString strAction = action->text();
    if (strAction == "全屏")
    {
        emit SigFullScreen();
    }
    else if (strAction == "停止")
    {
        emit SigStop();
    }
    else if (strAction == "暂停" || strAction == "播放")
    {
        emit SigPlayOrPause();
    }
}

bool Show::ConnectSignalSlots()
{
	QList<bool> listRet;
	bool bRet;

	bRet = connect(this, &Show::SigPlay, this, &Show::OnPlay);
    listRet.append(bRet);

    timerShowCursor.setInterval(2000);
    bRet = connect(&timerShowCursor, &QTimer::timeout, this, &Show::OnTimerShowCursorUpdate);
    listRet.append(bRet);

    connect(&ActionGroup, &QActionGroup::triggered, this, &Show::OnActionsTriggered);

	for (bool bReturn : listRet)
	{
		if (bReturn == false)
		{
			return false;
		}
	}

	return true;
}

void Show::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls();
    if(urls.isEmpty())
    {
        return;
    }

    for(QUrl url: urls)
    {
        QString strFileName = url.toLocalFile();
        qDebug() << strFileName;
        emit SigOpenFile(strFileName);
        break;
    }
}

void Show::OnResolutionChanged(bool success,QString outputFilePath,double pos)
{
    if (success)
    {
        qDebug() << "转换后的视频文件：" << outputFilePath;
        start = pos;
        OnPlay(outputFilePath);
    }
    else {
        QMessageBox::warning(this, "转换失败", "");
    }
}