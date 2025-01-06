// 主界面
// Editor: Liwh 2024/12/20
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
#include <QGraphicsDropShadowEffect>

#include "mainwid.h"
#include "ui_mainwid.h"
#include "globalhelper.h"
#include "videoctl.h"
#include <QMessageBox>
#include <SDL_ttf.h>

bool isRecording;
const int FULLSCREEN_MOUSE_DETECT_TIME = 500;

MainWid::MainWid(QMainWindow *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWid),
    ShadowWidth(0),
    Menu(this),
    Playlist(this),
    Title(this),
    MoveDrag(false),
    ActFullscreen(this)
{
    isRecording = false;
    ui->setupUi(this);

    setWindowFlags(Qt::FramelessWindowHint| Qt::WindowMinimizeButtonHint);

    ShadowWidth = 10;



    QString qss = GlobalHelper::GetQssStr("://res/qss/mainwid.css");
    setStyleSheet(qss);

    this->setMouseTracking(true);

    isPlaying = false;

    FullScreenPlay = false;

    CtrlBarAnimationTimer.setInterval(2000);
    FullscreenMouseDetectTimer.setInterval(FULLSCREEN_MOUSE_DETECT_TIME);


    loopActionGroup = new QActionGroup(this);
    loopActionGroup->setExclusive(true);
}
MainWid::~MainWid()
{
    delete ui;
}

bool MainWid::Init()
{
    QWidget *em = new QWidget(this);
    ui->PlaylistWid->setTitleBarWidget(em);
    ui->PlaylistWid->setFixedWidth(200);
    ui->PlaylistWid->setWidget(&Playlist);

    QWidget *emTitle = new QWidget(this);
    ui->TitleWid->setTitleBarWidget(emTitle);
    ui->TitleWid->setWidget(&Title);

    if (ConnectSignalSlots() == false)
    {
        return false;
    }

    if (ui->CtrlBarWid->Init() == false || Playlist.Init() == false ||ui->ShowWid->Init() == false || Title.Init() == false)
    {
        return false;
    }


    CtrlbarAnimationShow = new QPropertyAnimation(ui->CtrlBarWid, "geometry");
    CtrlbarAnimationHide = new QPropertyAnimation(ui->CtrlBarWid, "geometry");


    InitMenuActions();


    InitMenu();
    return true;
}
void MainWid::InitMenuActions()
{
    Map_act["OpenFile"] = &MainWid::OpenFile;
    Map_act["OnCloseBtnClicked"] = &MainWid::OnCloseBtnClicked;
    Map_act["OnSaveBmp"] = &MainWid::OnSaveBmp;
    Map_act["OnFullScreenPlay"] = &MainWid::OnFullScreenPlay;
    Map_act["OnSavePeriod"] = &MainWid::OnSavePeriod;
    Map_act["OnLoop"] = &MainWid::OnLoop;
    Map_act["OnList"] = &MainWid::OnList;
    Map_act["OnRandom"] = &MainWid::OnRandom;
}
void MainWid::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
}


void MainWid::enterEvent(QEvent *event)
{
    Q_UNUSED(event);

}
void MainWid::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);

}

bool MainWid::ConnectSignalSlots()
{
	connect(&Title, &Title::SigCloseBtnClicked, this, &MainWid::OnCloseBtnClicked);
	connect(&Title, &Title::SigMaxBtnClicked, this, &MainWid::OnMaxBtnClicked);
	connect(&Title, &Title::SigMinBtnClicked, this, &MainWid::OnMinBtnClicked);
	connect(&Title, &Title::SigDoubleClicked, this, &MainWid::OnMaxBtnClicked);
    connect(&Title, &Title::SigFullScreenBtnClicked, this, &MainWid::OnFullScreenPlay);
    connect(&Title, &Title::SigOpenFile, &Playlist, &Playlist::OnAddFileAndPlay);
    connect(&Title, &Title::SigShowMenu, this, &MainWid::OnShowMenu);
    

    connect(&Playlist, &Playlist::SigPlay, ui->ShowWid, &Show::SigPlay);

	connect(ui->ShowWid, &Show::SigOpenFile, &Playlist, &Playlist::OnAddFileAndPlay);
    connect(ui->ShowWid, &Show::SigFullScreen, this, &MainWid::OnFullScreenPlay);
    connect(ui->ShowWid, &Show::SigPlayOrPause, VideoCtl::GetInstance(), &VideoCtl::OnPause);
    connect(ui->ShowWid, &Show::SigStop, VideoCtl::GetInstance(), &VideoCtl::OnStop);
    connect(ui->ShowWid, &Show::SigShowMenu, this, &MainWid::OnShowMenu);
    connect(ui->ShowWid, &Show::SigSeekForward, VideoCtl::GetInstance(), &VideoCtl::OnSeekForward);
    connect(ui->ShowWid, &Show::SigSeekBack, VideoCtl::GetInstance(), &VideoCtl::OnSeekBack);
    connect(ui->ShowWid, &Show::SigAddVolume, VideoCtl::GetInstance(), &VideoCtl::OnAddVolume);
    connect(ui->ShowWid, &Show::SigSubVolume, VideoCtl::GetInstance(), &VideoCtl::OnSubVolume);


    connect(ui->CtrlBarWid, &CtrlBar::SigSpeed, VideoCtl::GetInstance(), &VideoCtl::OnSpeed);
    connect(ui->CtrlBarWid, &CtrlBar::SigShowOrHidePlaylist, this, &MainWid::OnShowOrHidePlaylist);
    connect(ui->CtrlBarWid, &CtrlBar::SigPlaySeek, VideoCtl::GetInstance(), &VideoCtl::OnPlaySeek);
    connect(ui->CtrlBarWid, &CtrlBar::SigPlayVolume, VideoCtl::GetInstance(), &VideoCtl::OnPlayVolume);
    connect(ui->CtrlBarWid, &CtrlBar::SigPlayOrPause, VideoCtl::GetInstance(), &VideoCtl::OnPause);
    connect(ui->CtrlBarWid, &CtrlBar::SigStop, VideoCtl::GetInstance(), &VideoCtl::OnStop);
    connect(ui->CtrlBarWid, &CtrlBar::SigBackwardPlay, &Playlist, &Playlist::OnBackwardPlay);
    connect(ui->CtrlBarWid, &CtrlBar::SigForwardPlay, &Playlist, &Playlist::OnForwardPlay);
    connect(ui->CtrlBarWid, &CtrlBar::SigShowMenu, this, &MainWid::OnShowMenu);
    connect(ui->CtrlBarWid, &CtrlBar::SigShowSetting, this, &MainWid::OnShowSettingWid);

    connect(this, &MainWid::SigShowMax, &Title, &Title::OnChangeMaxBtnStyle);
    connect(this, &MainWid::SigSeekForward, VideoCtl::GetInstance(), &VideoCtl::OnSeekForward);
    connect(this, &MainWid::SigSeekBack, VideoCtl::GetInstance(), &VideoCtl::OnSeekBack);
    connect(this, &MainWid::SigAddVolume, VideoCtl::GetInstance(), &VideoCtl::OnAddVolume);
    connect(this, &MainWid::SigSubVolume, VideoCtl::GetInstance(), &VideoCtl::OnSubVolume);
    connect(this, &MainWid::SigOpenFile, &Playlist, &Playlist::OnAddFileAndPlay);
    connect(this, &MainWid::SigVideoLoop, ui->CtrlBarWid, &CtrlBar::OnChangeVideo);

    connect(VideoCtl::GetInstance(), &VideoCtl::SigVideoTotalSeconds, ui->CtrlBarWid, &CtrlBar::OnVideoTotalSeconds);
    connect(VideoCtl::GetInstance(), &VideoCtl::SigVideoPlaySeconds, ui->CtrlBarWid, &CtrlBar::OnVideoPlaySeconds);
    connect(VideoCtl::GetInstance(), &VideoCtl::SigVideoVolume, ui->CtrlBarWid, &CtrlBar::OnVideopVolume);
    connect(VideoCtl::GetInstance(), &VideoCtl::SigPauseStat, ui->CtrlBarWid, &CtrlBar::OnPauseStat, Qt::QueuedConnection);
    connect(VideoCtl::GetInstance(), &VideoCtl::SigStopFinished, ui->ShowWid, &Show::OnStopFinished, Qt::QueuedConnection);
    connect(VideoCtl::GetInstance(), &VideoCtl::SigFrameDimensionsChanged, ui->ShowWid, &Show::OnFrameDimensionsChanged, Qt::QueuedConnection);
    connect(VideoCtl::GetInstance(), &VideoCtl::SigStopFinished, &Title, &Title::OnStopFinished, Qt::DirectConnection);
    connect(VideoCtl::GetInstance(), &VideoCtl::SigStartPlay, &Title, &Title::OnPlay, Qt::DirectConnection);
    connect(VideoCtl::GetInstance(), &VideoCtl::SigList, &Playlist, &Playlist::OnForwardPlay);
    connect(VideoCtl::GetInstance(), &VideoCtl::SigRandom, &Playlist, &Playlist::OnRandomPlay);

    connect(ui->CtrlBarWid, &CtrlBar::SigLoadSubtitle, VideoCtl::GetInstance(), &VideoCtl::LoadSubtitle);

    connect(&CtrlBarAnimationTimer, &QTimer::timeout, this, &MainWid::OnCtrlBarAnimationTimeOut);

    connect(&FullscreenMouseDetectTimer, &QTimer::timeout, this, &MainWid::OnFullscreenMouseDetectTimeOut);


    connect(&ActFullscreen, &QAction::triggered, this, &MainWid::OnFullScreenPlay);
    


	return true;
}


void MainWid::keyReleaseEvent(QKeyEvent *event)
{
    qDebug() << "MainWid::keyPressEvent:" << event->key();
	switch (event->key())
	{
    case Qt::Key_F: 
        OpenFile();
        break;
	case Qt::Key_Return:
        OnFullScreenPlay();
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
    case Qt::Key_S:
        OnSaveBmp();
        break;
    case Qt::Key_Escape:
        OnCloseBtnClicked();
        break;
    case Qt::Key_A:
        OnSavePeriod();
        break;
    case Qt::Key_M:
        OnShowMenu();
        break;
	default:
		break;
	}
}


void MainWid::mousePressEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton)
    {
        if (ui->TitleWid->geometry().contains(event->pos()))
        {
            MoveDrag = true;
            DragPosition = event->globalPos() - this->pos();
        }
    }

    QWidget::mousePressEvent(event);
}

void MainWid::mouseReleaseEvent(QMouseEvent *event)
{
    MoveDrag = false;

    QWidget::mouseReleaseEvent(event);
}

void MainWid::mouseMoveEvent(QMouseEvent *event)
{
    if (MoveDrag)
    {
        move(event->globalPos() - DragPosition);
    }

    QWidget::mouseMoveEvent(event);
}

void MainWid::contextMenuEvent(QContextMenuEvent* event)
{
    Menu.popup(event->globalPos());
}

void MainWid::OnFullScreenPlay()
{
    if (FullScreenPlay == false)
    {
        FullScreenPlay = true;
        ActFullscreen.setChecked(true);

        ui->ShowWid->setWindowFlags(Qt::Window);
        QScreen *pStCurScreen = screen();
        ui->ShowWid->windowHandle()->setScreen(pStCurScreen);
        
        ui->ShowWid->showFullScreen();

        QRect stScreenRect = pStCurScreen->geometry();
        int nCtrlBarHeight = ui->CtrlBarWid->height();
        int nX = ui->ShowWid->x();
        CtrlBarAnimationShowRect = QRect(nX, stScreenRect.height() - nCtrlBarHeight, stScreenRect.width(), nCtrlBarHeight);
        CtrlBarAnimationHideRect = QRect(nX, stScreenRect.height(), stScreenRect.width(), nCtrlBarHeight);

        CtrlbarAnimationShow->setStartValue(CtrlBarAnimationHideRect);
        CtrlbarAnimationShow->setEndValue(CtrlBarAnimationShowRect);
        CtrlbarAnimationShow->setDuration(1000);

        CtrlbarAnimationHide->setStartValue(CtrlBarAnimationShowRect);
        CtrlbarAnimationHide->setEndValue(CtrlBarAnimationHideRect);
        CtrlbarAnimationHide->setDuration(1000);
        
        ui->CtrlBarWid->setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
        ui->CtrlBarWid->windowHandle()->setScreen(pStCurScreen);
        ui->CtrlBarWid->raise();
        ui->CtrlBarWid->setWindowOpacity(0.5);
        ui->CtrlBarWid->showNormal();
        ui->CtrlBarWid->windowHandle()->setScreen(pStCurScreen);
        
        CtrlbarAnimationShow->start();
        FullscreenCtrlBarShow = true;
        FullscreenMouseDetectTimer.start();

        this->setFocus();
    }
    else
    {
        FullScreenPlay = false;
        ActFullscreen.setChecked(false);

        CtrlbarAnimationShow->stop();
        CtrlbarAnimationHide->stop();
        ui->CtrlBarWid->setWindowOpacity(1);
        ui->CtrlBarWid->setWindowFlags(Qt::SubWindow);
        
        ui->ShowWid->setWindowFlags(Qt::SubWindow);

        ui->CtrlBarWid->showNormal();
        ui->ShowWid->showNormal();

        FullscreenMouseDetectTimer.stop();
        this->setFocus();
    }
}

void MainWid::OnCtrlBarAnimationTimeOut()
{
    QApplication::setOverrideCursor(Qt::BlankCursor);
}

void MainWid::OnFullscreenMouseDetectTimeOut()
{
    if (FullScreenPlay)
    {
        if (CtrlBarAnimationShowRect.contains(cursor().pos()))
        {

            if (ui->CtrlBarWid->geometry().contains(cursor().pos()))
            {
                FullscreenCtrlBarShow = true;
            }
            else
            {
                ui->CtrlBarWid->raise();
                
                CtrlbarAnimationShow->start();
                CtrlbarAnimationHide->stop();
                CtrlBarHideTimer.stop();
            }
        }
        else  
        {
            if (FullscreenCtrlBarShow)
            {
                FullscreenCtrlBarShow = false;
                CtrlBarHideTimer.singleShot(2000, this, &MainWid::OnCtrlBarHideTimeOut);
            }

        }

    }
}

void MainWid::OnCtrlBarHideTimeOut()
{
    if (FullScreenPlay)
    {
        CtrlbarAnimationHide->start();
    }
}

void MainWid::OnShowMenu()
{
    qDebug() << "MainWid::OnShowMenu triggered at position:" << QCursor::pos();
    QPoint globalPos = QCursor::pos();
    QScreen* currentScreen = QGuiApplication::screenAt(globalPos);
    if (currentScreen)
    {
        QRect screenGeometry = currentScreen->geometry();
        QPoint adjustedPos = globalPos;
        if (globalPos.x() + Menu.sizeHint().width() > screenGeometry.right())
            adjustedPos.setX(screenGeometry.right() - Menu.sizeHint().width());
        if (globalPos.y() + Menu.sizeHint().height() > screenGeometry.bottom())
            adjustedPos.setY(screenGeometry.bottom() - Menu.sizeHint().height());
        Menu.popup(adjustedPos);
    }
    else
    {
        Menu.popup(globalPos);
    }
}

void MainWid::OnShowAbout()
{

}

void MainWid::OpenFile()
{
    QString strFileName = QFileDialog::getOpenFileName(this, "打开文件", QDir::homePath(),"音视频文件(*.wav *.ogg *.mp3 *.mkv *.rmvb *.mp4 *.avi *.flv *.wmv *.3gp *.mov *.yuv)");
    emit SigOpenFile(strFileName);
}
void MainWid::OnSaveBmp()
{
    QString savePath;
    bool success = VideoCtl::GetInstance()->TakeScreenshot(savePath);
    if (success)
    {
        QMessageBox::information(this, "截图成功", QString("截图已保存到：%1").arg(savePath));
    }
    else
    {
        QMessageBox::warning(this, "截图失败", "无法保存截图。");
    }
}
void MainWid::OnShowSettingWid()
{

}
void MainWid::InitMenu()
{
    QString menu_json_file_name = ":/res/menu.json";
    QByteArray ba_json;
    QFile json_file(menu_json_file_name);
    if (json_file.open(QIODevice::ReadOnly))
    {
        ba_json = json_file.readAll();
        json_file.close();
    }

    QJsonDocument json_doc = QJsonDocument::fromJson(ba_json);

    if (json_doc.isObject())
    {
        QJsonObject json_obj = json_doc.object();
        MenuJsonParser(json_obj, &Menu);
    }
    Menu.setStyleSheet("QMenu { background-color: #2C2C2C; color: white; border: 1px solid #5C5C5C; }"
        "QMenu::item { padding: 5px 20px; }"
        "QMenu::item:selected { background-color: #4C9AFF; }");
}
void MainWid::MenuJsonParser(QJsonObject& json_obj, QMenu* menu)
{
    QJsonObject::iterator it = json_obj.begin();
    QJsonObject::iterator end = json_obj.end();
    while (it != end)
    {
        QString key = it.key();
        auto value = it.value();
        if (value.isObject())
        {
            QMenu* sub_menu = menu->addMenu(key);
            QJsonObject obj = value.toObject();
            MenuJsonParser(obj, sub_menu);
        }
        else
        {
            QString value_str = value.toString();
            qDebug() << value_str << "\n";
            QStringList value_info = value_str.split("/");

            if (value_info.size() == 2)
            {
                QString fun_str = value_info[0];
                QString hot_key = value_info[1];
                if (!hot_key.isEmpty())
                {
                    key += "\t" + hot_key;
                }
                QAction* action = menu->addAction(key);

                if (!hot_key.isEmpty())
                {
                    action->setShortcut(QKeySequence(hot_key));
                    action->setShortcutContext(Qt::ApplicationShortcut);
                }

                if (Map_act.contains(fun_str))
                {
                    qDebug() << "设置了菜单" << fun_str << '\n';
                    MenuAction pFunc = Map_act.value(fun_str);
                    connect(action, &QAction::triggered, this, pFunc);
                    if (fun_str == "OnLoop" || fun_str == "OnList")
                    {
                        action->setCheckable(true);
                        loopActionGroup->addAction(action);
                        if (fun_str == "OnLoop")
                            action->setChecked(VideoCtl::GetInstance()->IsLooping());
                        else if (fun_str == "OnList")
                            action->setChecked(!VideoCtl::GetInstance()->IsLooping());
                    }
                }
                else
                {
                    qWarning() << "MenuJsonParser: 未找到对应函数名:" << fun_str;
                }
            }
            else
            {
                QString fun_str = value_info[0];
                QAction* action = menu->addAction(key);
                if (Map_act.contains(fun_str))
                {
                    qDebug() << "设置了菜单" << fun_str << '\n';
                    MenuAction pFunc = Map_act.value(fun_str);
                    connect(action, &QAction::triggered, this, pFunc);
                }
                else
                {
                    qWarning() << "MenuJsonParser: 未找到对应函数名:" << fun_str;
                }
            }
        }

        it++;
    }
}

QMenu* MainWid::AddMenuFun(QString menu_title, QMenu* menu)
{
    QMenu* menu_t = new QMenu(this);
    menu_t->setTitle(menu_title);
    menu->addMenu(menu_t);
    return menu_t;
}

void MainWid::AddActionFun(QString action_title, QMenu* menu, void(MainWid::* slot_addr)())
{
    QAction* action = new QAction(this);;
    action->setText(action_title);
    menu->addAction(action);
    connect(action, &QAction::triggered, this, slot_addr);
}

void MainWid::OnCloseBtnClicked()
{
    this->close();
}

void MainWid::OnMinBtnClicked()
{
    this->showMinimized();
}

void MainWid::OnMaxBtnClicked()
{
    if (isMaximized())
    {
        showNormal();
        emit SigShowMax(false);
    }
    else
    {
        showMaximized();
        emit SigShowMax(true);
    }
}

void MainWid::OnShowOrHidePlaylist()
{
     if (ui->PlaylistWid->isHidden())
     {
         ui->PlaylistWid->show();
     }
     else
     {
         ui->PlaylistWid->hide();
     }
     this->repaint();
}
void MainWid::OnSavePeriod()
{
    if (!isRecording)
    {
        VideoCtl::GetInstance()->LoadingSc();
        qDebug() << "正在录屏中" << '\n';
        isRecording = true;
    }
    else
    {
        VideoCtl::GetInstance()->LoadingSc();
        qDebug() << "录屏完成" << '\n';
        isRecording = false;
    }
}

void MainWid::OnLoop()
{
    VideoCtl::GetInstance()->OnSetLoop(1);
    emit SigVideoLoop("循环播放");
}
void MainWid::OnList()
{
    VideoCtl::GetInstance()->OnSetLoop(2);
    emit SigVideoLoop("列表播放");
}

void MainWid::OnRandom()
{
    VideoCtl::GetInstance()->OnSetLoop(3);
    emit SigVideoLoop("随机播放");
}
