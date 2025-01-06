// PCH.H 公共头
// Editor: Liwh 2024/12/21

#pragma once
#pragma execution_character_set("utf-8")

enum ERROR_CODE
{
    NoError = 0,
    ErrorFileInvalid
};

#include <QString>
#include <QPushButton>
#include <QDebug>
#include <QStringList>

class GlobalHelper
{
public:
    GlobalHelper();
    static void CreateSpeedMenu(QPushButton* speedButton);
    static void CreateClearMenu(QPushButton* clearButton);
    static void CreateSettingMenu(QPushButton* settingButton);

    static QString GetQssStr(QString strQssPath);
    static void SetIcon(QPushButton* btn, int iconSize, QChar icon);
    static void SavePlaylist(QStringList& playList);
    static void GetPlaylist(QStringList& playList);
    static void SavePlayVolume(double& nVolume);
    static void GetPlayVolume(double& nVolume);
};

extern "C"{
#include "libavutil/avstring.h"
#include "libavutil/channel_layout.h"
#include "libavutil/eval.h"
#include "libavutil/mathematics.h"
#include "libavutil/pixdesc.h"
#include "libavutil/imgutils.h"
#include "libavutil/dict.h"
#include "libavutil/fifo.h"
#include "libavutil/parseutils.h"
#include "libavutil/samplefmt.h"
#include "libavutil/time.h"
#include "libavutil/bprint.h"
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavfilter/avfilter.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libavutil/opt.h"
#include "libavcodec/avfft.h"
#include "libswresample/swresample.h"
#include "SDL.h"
#include "SDL_image.h"
#include "SDL_ttf.h"
}
#include <opencv2/opencv.hpp>
#define MAX_SLIDER_VALUE 65535
