// 核心部分：控制播放视频
// Editor: Liwh 2024/12/23

#pragma once

#include <QObject>
#include <QThread>
#include <QString>
#include <QTimer>
#include <QLabel>
#include <QStandardPaths>

#include "globalhelper.h"
#include "data.h"
#include "sonic.h"

#include <SoundTouch.h>
#include <QProcess>
#include <SDL_ttf.h>
using namespace soundtouch;


#define FFP_PROP_FLOAT_PLAYBACK_RATE      0003       // 设置播放速率
#define FFP_PROP_FLOAT_PLAYBACK_VOLUME    10006


class VideoCtl : public QObject
{
    Q_OBJECT
public:
    static VideoCtl* GetInstance();
    ~VideoCtl();
    bool ReInit();
    bool StartPlay(QString strFileName, QLabel* label, double start);

    void update_sample_display(VideoState* is, short* samples, int samples_size);

    int AudioDecodeFromFrame(VideoState* is);  // 解码并重采样音频帧
    void SetClockAt(Clock* c, double pts, int serial, double time);
    void ClockToSlave(Clock* c, Clock* slave);

    void ChangeResolution(const std::string& resolution);

    double GetCurrentPosition();

    float  ffp_get_property_float(int id, float default_value);
    void   ffp_set_property_float(int id, float value);

    bool TakeScreenshot(QString& filePath);

signals:
    void SigPlayMsg(QString strMsg);
    void SigFrameDimensionsChanged(int nFrameWidth, int nFrameHeight); //<视频宽高发生变化

    void SigVideoTotalSeconds(double nSeconds);
    void SigVideoPlaySeconds(double nSeconds);

    void SigVideoVolume(double dPercent);
    void SigPauseStat(bool bPaused);

    void ResolutionChanged(bool flag, QString Filename, double pos);

    void SigStop();

    void SigStopFinished();
    void SigSpeed(float speed);

    void SigStartPlay(QString strFileName);

    void SigClear();

    void SigCaptureStarted(double startTime);
    void SigCaptureStopped(QString outputFile, bool success);

    void SigList();
    void SigRandom();

public:
    void OnPlaySeek(double dPercent);
    void OnPlayVolume(double dPercent);
    void OnSeekForward();
    void OnSeekBack();
    void OnAddVolume();
    void OnSubVolume();
    void OnPause();
    void OnStop();

    int IsLooping();

    void OnSpeed(double newSpeed);

    void OnSetWordPlay(bool flag);

    bool GetHasWord();

    void LoadingSc();

    void OnSetLoop(int flag);

public:
    explicit VideoCtl(QObject* parent = nullptr);
    void ShowTemporarySubtitle(const QString& text, int duration_ms);
    void ClearSubtitle();
    bool Init();
    bool ConnectSignalSlots();

    int GetVideoFrame(VideoState* is, AVFrame* frame);
    int AudioThread(void* arg); // 音频线程
    int VideoThread(void* arg); // 视频线程
    int SubTitleThread(void* arg); // 字幕解码线程

    int SynchronizeAudio(VideoState* is, int nb_samples);

    int AudioOpen(void* opaque, int64_t wanted_channel_layout, int wanted_nb_channels, int wanted_sample_rate, struct AudioParams* audio_hw_params);
    int StreamOpen(VideoState* is, int stream_index);
    int StreamPackets(AVStream* st, int stream_id, PacketQueue* queue);
    int RealTime(AVFormatContext* s);
    void ReadThread(VideoState* CurStream);
    void LoopThread(VideoState* CurStream);
    VideoState* StreamOpen(const char* filename);

    void ClearSubtitleCache();

    void StreamCycleChannel(VideoState* is, int codec_type);
    void RefreshLoop(VideoState* is, SDL_Event* event);

    void VideoRefresh(void* opaque, double* remaining_time);
    int QueuePicture(VideoState* is, AVFrame* src_frame, double pts, double duration, int64_t pos, int serial);

    //更新音量
    void UpdateVolume(int sign, double step);

    void VideoDisplay(VideoState* is);
    int VideoOpen(VideoState* is);
    void DoExit(VideoState*& is);

    int ReallocTexture(SDL_Texture** texture, Uint32 new_format, int new_width, int new_height, SDL_BlendMode blendmode, int init_texture);
    void CalculateDisplayRect(SDL_Rect* rect, int scr_xleft, int scr_ytop, int scr_width, int scr_height, int pic_width, int pic_height, AVRational pic_sar);
    int UploadTexture(SDL_Texture* tex, AVFrame* frame, struct SwsContext** img_convert_ctx);
    void VideoImageDisplay(VideoState* is);
    void StreamComponentClose(VideoState* is, int stream_index);
    void StreamClose(VideoState* is);
    double GetClock(Clock* c);

    void SetClock(Clock* c, double pts, int serial);
    void SetClockSpeed(Clock* c, double speed);
    void InitClock(Clock* c, int* queue_serial);

    int GetMasterType(VideoState* is);
    double GetMasterClock(VideoState* is);
    void CheckExternalClockSpeed(VideoState* is);
    void StreamSeek(VideoState* is, int64_t pos, int64_t rel);
    void StreamTogglePause(VideoState* is);
    void TogglePause(VideoState* is);
    void ToNextFrame(VideoState* is);
    double TargetDelay(double delay, VideoState* is);
    double VpDuration(VideoState* is, Frame* vp, Frame* nextvp);
    void UpdateVideoPts(VideoState* is, double pts, int64_t pos, int serial);
public:
    void ffp_set_playback_rate(float rate);
    float ffp_get_playback_rate();

    int ffp_get_playback_rate_change();
    void ffp_set_playback_rate_change(int change);

    int64_t get_target_frequency();
    int     get_target_channels();
    int   is_normal_playback_rate();

public slots:
    void onFfmpegFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onWordFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void LoadSubtitle(const QString& subtitleFilePath);
    void HideSubtitle();
    void OnCaptureFinished(int exitCode, QProcess::ExitStatus exitStatus);

public:

    static VideoCtl* m_pInstance; //< 单例指针

    bool m_bInited;	//< 初始化标志
    bool m_bPlayLoop; //刷新循环标志

    VideoState* m_CurStream;

    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_RendererInfo renderer_info = { 0 };
    SDL_AudioDeviceID audio_dev;
    WId play_wid;//播放窗口


    int screen_width;
    int screen_height;
    int startup_volume;

    //播放刷新循环线程
    std::thread m_tPlayLoopThread;

    int m_nFrameW;
    int m_nFrameH;

    soundtouch::SoundTouch soundTouch;


    SDL_mutex* soundTouchMutex;

    std::map<std::string, int> resmap;


    TTF_Font* subtitle_font = nullptr;
    std::string subtitle_font_path = "res//杨任东竹石体-Medium.ttf";
    int subtitle_font_size = 50;

    SDL_Texture* subtitle_texture = nullptr;

    void RenderText(const std::string& text, int x, int y,int id);

    std::map<std::string, SDL_Texture*> subtitle_texture_cache;
    std::mutex subtitle_cache_mutex;

    QProcess* ffmpegProcess;
    QProcess* OnloadWord;
    QProcess* captureProcess;

    float  speed;     // 播放速率
    int  ischange;   // 播放速率改变

    bool wordPlay;

    bool HasWord;

    bool isRecording;
    double recordStartTime;
    double recordEndTime;
    QString captureOutputFile;


    QString currentSubtitle;
    QTimer* subtitleTimer;

    int isloop;
public:
    // 变速相关
    sonicStreamStruct* audio_speed_convert;

private:
    int m_targetWidth;
    int m_targetHeight;
};
