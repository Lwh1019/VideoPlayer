// 控制栏界面
// Editor: Liwh 2024/12/20
#include <QDebug>
#include <QTime>
#include <QSettings>
#include <QTimer>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtSvgWidgets/QSvgWidget>
 
#include "ctrlbar.h"
#include "ui_ctrlbar.h"
#include "globalhelper.h"
#include <QFileDialog>
#include <videoctl.h>
#include "mainwid.h"

extern bool isRecording;

CtrlBar::CtrlBar(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CtrlBar)
{
    ui->setupUi(this);
    LastVolumePercent = 1.0;


    QTimer* textCheckTimer = new QTimer(this);
    connect(textCheckTimer, &QTimer::timeout, this, &CtrlBar::checkSpeedBtnTextChanged);
    connect(textCheckTimer, &QTimer::timeout, this, &CtrlBar::checkClearBtnTextChanged);
    connect(textCheckTimer, &QTimer::timeout, this, &CtrlBar::checkSettingBtnTextChanged);
    textCheckTimer->start(100);

}
void CtrlBar::checkSpeedBtnTextChanged()
{
    static QString lastText = ui->speedBtn->text();
    QString currentText = ui->speedBtn->text();

    if (currentText != lastText)
    {
        on_speedBtn_changed();
        lastText = currentText;
    }
}
void CtrlBar::checkClearBtnTextChanged()
{
    static QString lastText = ui->clearBtn->text();
    QString currentText = ui->clearBtn->text();

    if (currentText != lastText)
    {
        on_clearBtn_changed();
        lastText = currentText;
    }
}
void CtrlBar::checkSettingBtnTextChanged()
{
    static QString lastText = ui->SettingBtn->text();
    QString currentText = ui->SettingBtn->text();

    if (currentText != lastText)
    {
        on_SettingBtn_clicked();
        lastText = currentText;
    }
}
void CtrlBar::ChangeText()
{
    ui->clearBtn->setText("自动");
    GlobalHelper::SetIcon(ui->WordBtn, 15, QChar(0xe135));
    VideoCtl::GetInstance()->OnSetWordPlay(false);
}
CtrlBar::~CtrlBar()
{
    delete ui;
}

bool CtrlBar::Init()
{
    setStyleSheet(GlobalHelper::GetQssStr("://res/qss/ctrlbar.css"));


    GlobalHelper::SetIcon(ui->PlayOrPauseBtn, 15, QChar(0xf04b));
    GlobalHelper::SetIcon(ui->WordBtn, 15, QChar(0xe135));
    GlobalHelper::SetIcon(ui->VolumeBtn, 12, QChar(0xf6a8));
    GlobalHelper::SetIcon(ui->PlaylistCtrlBtn, 15, QChar(0xf0ae));
    GlobalHelper::SetIcon(ui->ForwardBtn, 15, QChar(0xf051));
    GlobalHelper::SetIcon(ui->BackwardBtn, 15, QChar(0xf048));
    GlobalHelper::SetIcon(ui->RecordBtn, 15, QChar(0xf8d9));
    GlobalHelper::SetIcon(ui->SettingBtn, 15, QChar(0xf2f9));
 
    GlobalHelper::CreateSpeedMenu(ui->speedBtn);
    GlobalHelper::CreateClearMenu(ui->clearBtn);
    GlobalHelper::CreateSettingMenu(ui->SettingBtn);

    ui->PlaylistCtrlBtn->setToolTip("播放列表");
    ui->SettingBtn->setToolTip("设置播放方式");
    ui->VolumeBtn->setToolTip("静音");
    ui->ForwardBtn->setToolTip("下一个");
    ui->BackwardBtn->setToolTip("上一个");
    ui->WordBtn->setToolTip("字幕");
    ui->PlayOrPauseBtn->setToolTip("播放");
    ui->speedBtn->setToolTip("倍速");
    ui->clearBtn->setToolTip("清晰度");
    ui->RecordBtn->setToolTip("录屏");
    
    ConnectSignalSlots();

    double dPercent = -1.0;
    GlobalHelper::GetPlayVolume(dPercent);
    if (dPercent != -1.0)
    {
        emit SigPlayVolume(dPercent);
        OnVideopVolume(dPercent);
    }

    return true;

}

bool CtrlBar::ConnectSignalSlots()
{
    QList<bool> listRet;
    bool bRet;

    connect(ui->PlaylistCtrlBtn, &QPushButton::clicked, this, &CtrlBar::SigShowOrHidePlaylist);
    connect(ui->PlaySlider, &CustomSlider::SigCustomSliderValueChanged, this, &CtrlBar::OnPlaySliderValueChanged);
    connect(ui->VolumeSlider, &CustomSlider::SigCustomSliderValueChanged, this, &CtrlBar::OnVolumeSliderValueChanged);
    connect(ui->BackwardBtn, &QPushButton::clicked, this, &CtrlBar::SigBackwardPlay);
    connect(ui->ForwardBtn, &QPushButton::clicked, this, &CtrlBar::SigForwardPlay);
    connect(ui->WordBtn, &QPushButton::clicked, this, &CtrlBar::On_WordBtn_clicked);

    //connect(ui->speedBtn, &QPushButton::, this, &CtrlBar::on_speedBtn_changed);
    connect(VideoCtl::GetInstance(), &VideoCtl::SigClear, this, &CtrlBar::ChangeText);
    connect(ui->RecordBtn, &QPushButton::clicked, this, &CtrlBar::OnRecordBtn);
    
    return true;
}

void CtrlBar::OnVideoTotalSeconds(int nSeconds)
{
    TotalPlaySeconds = nSeconds;

    int thh, tmm, tss;
    thh = nSeconds / 3600;
    tmm = (nSeconds % 3600) / 60;
    tss = (nSeconds % 60);
    QTime TotalTime(thh, tmm, tss);

    ui->VideoTotalTimeTimeEdit->setTime(TotalTime);
}


void CtrlBar::OnVideoPlaySeconds(int nSeconds)
{
    nSeconds = std::min(nSeconds, TotalPlaySeconds);
    int thh, tmm, tss;
    thh = nSeconds / 3600;
    tmm = (nSeconds % 3600) / 60;
    tss = (nSeconds % 60);
    QTime TotalTime(thh, tmm, tss);

    ui->VideoPlayTimeTimeEdit->setTime(TotalTime);

    ui->PlaySlider->setValue(nSeconds * 1.0 / TotalPlaySeconds * MAX_SLIDER_VALUE);
}

void CtrlBar::OnVideopVolume(double dPercent)
{
    ui->VolumeSlider->setValue(dPercent * MAX_SLIDER_VALUE);
    LastVolumePercent = dPercent;
    qDebug() << "LastVolumePercent：" << dPercent << '\n';
    if (LastVolumePercent == 0)
    {
        GlobalHelper::SetIcon(ui->VolumeBtn, 12, QChar(0xf2e2));
    }
    else if(LastVolumePercent==1)
    {
        GlobalHelper::SetIcon(ui->VolumeBtn, 12, QChar(0xf028));
    }
    else
    {
        GlobalHelper::SetIcon(ui->VolumeBtn, 12, QChar(0xf6a8));
    }

    GlobalHelper::SavePlayVolume(dPercent);
}

void CtrlBar::OnPauseStat(bool bPaused)
{
    qDebug() << "CtrlBar::OnPauseStat" << bPaused;
    if (bPaused)
    {
        GlobalHelper::SetIcon(ui->PlayOrPauseBtn, 15, QChar(0xf04b));
        ui->PlayOrPauseBtn->setToolTip("播放");
    }
    else
    {
        GlobalHelper::SetIcon(ui->PlayOrPauseBtn, 15, QChar(0xf04c));
        ui->PlayOrPauseBtn->setToolTip("暂停");
    }
}

void CtrlBar::OnStopFinished()
{
    ui->PlaySlider->setValue(0);
    QTime StopTime(0, 0, 0);
    ui->VideoTotalTimeTimeEdit->setTime(StopTime);
    ui->VideoPlayTimeTimeEdit->setTime(StopTime);
    GlobalHelper::SetIcon(ui->PlayOrPauseBtn, 15, QChar(0xf04b));
    ui->PlayOrPauseBtn->setToolTip("播放");
}

void CtrlBar::OnSpeed(float speed)
{
    ui->speedBtn->setText(QString("倍速:").arg(speed));
}
void CtrlBar::OnChangeVideo(QString s)
{
    if (s == "循环播放")
    {
        GlobalHelper::SetIcon(ui->SettingBtn, 15, QChar(0xf2f9));
    }
    else if (s == "列表播放")
    {
        GlobalHelper::SetIcon(ui->SettingBtn, 15, QChar(0xf883));
    }
    else
    {
        GlobalHelper::SetIcon(ui->SettingBtn, 15, QChar(0xf074));
    }
}
void CtrlBar::OnPlaySliderValueChanged()
{
    double dPercent = ui->PlaySlider->value()*1.0 / ui->PlaySlider->maximum();
    emit SigPlaySeek(dPercent);
    qDebug() << dPercent;
}

void CtrlBar::OnVolumeSliderValueChanged()
{
    double dPercent = ui->VolumeSlider->value()*1.0 / ui->VolumeSlider->maximum();
    
    emit SigPlayVolume(dPercent);

    OnVideopVolume(dPercent);
}

void CtrlBar::on_PlayOrPauseBtn_clicked()
{
    emit SigPlayOrPause();
}

void CtrlBar::on_VolumeBtn_clicked()
{
    if (ui->VolumeBtn->text() == QChar(0xf6a8)|| ui->VolumeBtn->text() == QChar(0xf028))
    {
        GlobalHelper::SetIcon(ui->VolumeBtn, 12, QChar(0xf2e2));
        ui->VolumeSlider->setValue(0);
        emit SigPlayVolume(0);
    }
    else
    {
        GlobalHelper::SetIcon(ui->VolumeBtn, 12, QChar(0xf028));
        ui->VolumeSlider->setValue(LastVolumePercent * MAX_SLIDER_VALUE);
        emit SigPlayVolume(LastVolumePercent);
    }

}
void CtrlBar::On_WordBtn_clicked()
{
    if (ui->WordBtn->text() == QChar(0xe135))
    {
        VideoCtl::GetInstance()->OnSetWordPlay(true);
        int flag = VideoCtl::GetInstance()->GetHasWord();
        if (flag)
        {
            GlobalHelper::SetIcon(ui->WordBtn, 15, QChar(0xf20a));
        }
        else
        {
            QString subtitleFilePath = QFileDialog::getOpenFileName(this, "选择字幕文件", "", "字幕文件 (*.srt *.ass *.sub *.ssa)");
            if (!subtitleFilePath.isEmpty())
            {
                emit SigLoadSubtitle(subtitleFilePath);
                //GlobalHelper::SetIcon(ui->WordBtn, 15, QChar(0xf20a));
            }
        }
    }
    else
    {
        VideoCtl::GetInstance()->OnSetWordPlay(false);
        emit SigHideSubtitle();
        GlobalHelper::SetIcon(ui->WordBtn, 15, QChar(0xe135));
    }
}
void CtrlBar::OnRecordBtn()
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
void CtrlBar::on_SettingBtn_clicked()
{
    if (ui->SettingBtn->text() == QChar(0xf2f9))
    {
        GlobalHelper::SetIcon(ui->SettingBtn, 15, QChar(0xf2f9));
        VideoCtl::GetInstance()->OnSetLoop(1);
    }
    else if (ui->SettingBtn->text() == QChar(0xf883))
    {
        GlobalHelper::SetIcon(ui->SettingBtn, 15, QChar(0xf883));
        VideoCtl::GetInstance()->OnSetLoop(2);
    }
    else
    {
        VideoCtl::GetInstance()->OnSetLoop(3);
        GlobalHelper::SetIcon(ui->SettingBtn, 15, QChar(0xf074));
    }
}
void CtrlBar::on_speedBtn_changed()
{
    if (ui->speedBtn->text() == "倍速")
    {
        emit SigSpeed(1);
    }
    else if (ui->speedBtn->text() == "0.75x")
    {
        emit SigSpeed(0.75);
    }
    else if (ui->speedBtn->text() == "1.25x")
    {
        emit SigSpeed(1.25);
    }
    else if (ui->speedBtn->text() == "1.5x")
    {
        emit SigSpeed(1.5);
    }
    else if(ui->speedBtn->text()=="0.5x")
    {
        emit SigSpeed(0.5);
    }
    else  emit SigSpeed(2);

}
void CtrlBar::on_clearBtn_changed()
{
    if (ui->clearBtn->text() == "自动")
    {
        qDebug() << "选择了自动" << '\n';
        VideoCtl::GetInstance()->ChangeResolution("自动");
    }
    else if (ui->clearBtn->text() == "360P 流畅")
    {
        qDebug() << "选择了360P" << '\n';
        VideoCtl::GetInstance()->ChangeResolution("360p");
    }
    else if (ui->clearBtn->text() == "480P 清晰")
    {
        qDebug() << "选择了480P" << '\n';
        VideoCtl::GetInstance()->ChangeResolution("480p");
    }
    else if (ui->clearBtn->text() == "720P 高清")
    {
        qDebug() << "选择了720P" << '\n';
        VideoCtl::GetInstance()->ChangeResolution("720p");
    }
    else if (ui->clearBtn->text() == "1080P 高清")
    {
        qDebug() << "选择了1080P" << '\n';
        VideoCtl::GetInstance()->ChangeResolution("1080p");
    }
    else
    {
        qDebug() << "选择了1080P 60帧" << '\n';
        VideoCtl::GetInstance()->ChangeResolution("1080p_60");
    }
}