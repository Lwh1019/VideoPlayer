// 控制栏界面
// Editor: Liwh 2024/12/20
#pragma once
#include <QWidget>
#include "ui_ctrlbar.h"
class CtrlBar : public QWidget,public Ui::CtrlBar
{
    Q_OBJECT

public:
    explicit CtrlBar(QWidget *parent = 0);
    void checkSpeedBtnTextChanged();
    void checkClearBtnTextChanged();
    void checkSettingBtnTextChanged();
    void ChangeText();
    ~CtrlBar();
    bool Init();

public:
    void OnVideoTotalSeconds(double nSeconds);
    void OnVideoPlaySeconds(double nSeconds);
    void OnVideopVolume(double dPercent);
    void OnPauseStat(bool bPaused);
    void OnStopFinished();
    void OnSpeed(float speed);

    void OnChangeVideo(QString s);
private:
    void OnPlaySliderValueChanged();
    void OnVolumeSliderValueChanged();
    void OnRecordBtn();

private slots:
    void on_PlayOrPauseBtn_clicked();
    void on_VolumeBtn_clicked();
    void on_SettingBtn_clicked();
    void on_speedBtn_changed();
    void on_clearBtn_changed();
    bool ConnectSignalSlots();
    void On_WordBtn_clicked();

signals:
    void SigShowOrHidePlaylist();	
    void SigPlaySeek(double dPercent);
    void SigPlayVolume(double dPercent);
    void SigPlayOrPause();
    void SigStop();
    void SigForwardPlay();
    void SigBackwardPlay();
    void SigShowMenu();
    void SigShowSetting();
    void SigSpeed(double speed);

    void SigLoadSubtitle(QString word);
    void SigHideSubtitle();

private:
    Ui::CtrlBar *ui;

    double TotalPlaySeconds;
    double LastVolumePercent;
};
