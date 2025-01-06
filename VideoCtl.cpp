// 核心部分：控制播放视频
// Editor: Liwh 2024/12/23


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
#include <QRandomGenerator>

#include <SDL_ttf.h>
#include "Playlist.h"
#include <thread>
#include "videoctl.h"
#include <QProcess>
#include <QMessageBox>

#pragma execution_character_set("utf-8")

extern QMutex show_rect;

QString Filename;
WId widlay;
QString outFile;


static int framedrop = -1;  //丢帧策略的标志位
static int infinite_buffer = -1;
static int64_t audio_callback_time;

#define FF_QUIT_EVENT    (SDL_USEREVENT + 2)

// 重新分配一个 SDL 纹理（SDL_Texture），确保纹理的格式、尺寸和其他参数满足需要
int VideoCtl::ReallocTexture(SDL_Texture** texture, Uint32 new_format, int new_width, int new_height, SDL_BlendMode blendmode, int init_texture)
{
    Uint32 format;
    int access, w, h;
    if (SDL_QueryTexture(*texture, &format, &access, &w, &h) < 0 || new_width != w || new_height != h || new_format != format) {
        void* pixels;
        int pitch;
        SDL_DestroyTexture(*texture);
        if (!(*texture = SDL_CreateTexture(renderer, new_format, SDL_TEXTUREACCESS_STREAMING, new_width, new_height)))
            return -1;
        if (SDL_SetTextureBlendMode(*texture, blendmode) < 0)
            return -1;
        if (init_texture) {
            if (SDL_LockTexture(*texture, NULL, &pixels, &pitch) < 0)
                return -1;
            memset(pixels, 0, pitch * new_height);
            SDL_UnlockTexture(*texture);
        }
    }
    return 0;
}

// 根据视频帧的宽高比和屏幕大小，计算视频在显示区域中的适配矩形
void VideoCtl::CalculateDisplayRect(SDL_Rect* rect,
    int scr_xleft, int scr_ytop, int scr_width, int scr_height,
    int pic_width, int pic_height, AVRational pic_sar)
{
    float aspect_ratio;
    int width, height, x, y;

    if (pic_sar.num == 0)
        aspect_ratio = 0;
    else
        aspect_ratio = av_q2d(pic_sar);

    if (aspect_ratio <= 0.0)
        aspect_ratio = 1.0;
    aspect_ratio *= (float)pic_width / (float)pic_height;

    int targetWidth = m_targetWidth > 0 ? m_targetWidth : scr_width;
    int targetHeight = m_targetHeight > 0 ? m_targetHeight : scr_height;

    height = targetHeight;
    width = lrint(height * aspect_ratio) & ~1;
    if (width > targetWidth)
    {
        width = targetWidth;
        height = lrint(width / aspect_ratio) & ~1;
    }
    x = (targetWidth - width) / 2;
    y = (targetHeight - height) / 2;
    rect->x = scr_xleft + x;
    rect->y = scr_ytop + y;
    rect->w = FFMAX(width, 1);
    rect->h = FFMAX(height, 1);
}

// 将解码后的视频帧数据上传到 SDL 的纹理
int VideoCtl::UploadTexture(SDL_Texture* tex, AVFrame* frame, struct SwsContext** img_convert_ctx)
{
    int ret = 0;
    AVFrame* processed_frame = nullptr;

    bool need_processing = (m_targetWidth != 0 && m_targetHeight != 0);

    if (need_processing)
    {
        *img_convert_ctx = sws_getCachedContext(*img_convert_ctx,
            frame->width, frame->height, (AVPixelFormat)frame->format,
            m_targetWidth > 0 ? m_targetWidth : frame->width,
            m_targetHeight > 0 ? m_targetHeight : frame->height,
            AV_PIX_FMT_BGRA, SWS_BICUBIC, NULL, NULL, NULL);

        if (!*img_convert_ctx) {
            av_log(NULL, AV_LOG_FATAL, "Cannot initialize the conversion context\n");
            return -1;
        }

        processed_frame = av_frame_alloc();
        if (!processed_frame) {
            av_log(NULL, AV_LOG_ERROR, "Could not allocate processed_frame\n");
            return -1;
        }
        processed_frame->format = AV_PIX_FMT_BGRA;
        processed_frame->width = m_targetWidth > 0 ? m_targetWidth : frame->width;
        processed_frame->height = m_targetHeight > 0 ? m_targetHeight : frame->height;

        ret = av_frame_get_buffer(processed_frame, 32);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Could not allocate the video frame data\n");
            av_frame_free(&processed_frame);
            return -1;
        }

        // 执行缩放
        ret = sws_scale(*img_convert_ctx, (const uint8_t* const*)frame->data, frame->linesize,
            0, frame->height, processed_frame->data, processed_frame->linesize);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "sws_scale failed\n");
            av_frame_free(&processed_frame);
            return -1;
        }
        // 将处理后的帧上传到 SDL 纹理
        ret = SDL_UpdateTexture(tex, NULL, processed_frame->data[0], processed_frame->linesize[0]);

        av_frame_free(&processed_frame);
    }
    else {
        // 不需要处理，直接上传纹理
        switch (frame->format)
        {
        case AV_PIX_FMT_YUV420P:
            if (frame->linesize[0] < 0 || frame->linesize[1] < 0 || frame->linesize[2] < 0) {
                av_log(NULL, AV_LOG_ERROR, "Negative linesize is not supported for YUV.\n");
                return -1;
            }
            ret = SDL_UpdateYUVTexture(tex, NULL, frame->data[0], frame->linesize[0],
                frame->data[1], frame->linesize[1],
                frame->data[2], frame->linesize[2]);
            break;
        case AV_PIX_FMT_BGRA:
            if (frame->linesize[0] < 0)
            {
                ret = SDL_UpdateTexture(tex, NULL, frame->data[0] + frame->linesize[0] * (frame->height - 1), -frame->linesize[0]);
            }
            else {
                ret = SDL_UpdateTexture(tex, NULL, frame->data[0], frame->linesize[0]);
            }
            break;
        default:
            *img_convert_ctx = sws_getCachedContext(*img_convert_ctx,
                frame->width, frame->height, (AVPixelFormat)frame->format, frame->width, frame->height,
                AV_PIX_FMT_BGRA, SWS_BICUBIC, NULL, NULL, NULL);
            if (*img_convert_ctx != NULL)
            {
                uint8_t* pixels[4];
                int pitch[4];
                if (!SDL_LockTexture(tex, NULL, (void**)pixels, pitch))
                {
                    sws_scale(*img_convert_ctx, (const uint8_t* const*)frame->data, frame->linesize,
                        0, frame->height, pixels, pitch);
                    SDL_UnlockTexture(tex);
                }
            }
            else
            {
                av_log(NULL, AV_LOG_FATAL, "Cannot initialize the conversion context\n");
                ret = -1;
            }
            break;
        }
    }

    return ret;
}

//显示视频画面
// 将视频帧和字幕帧渲染到窗口中
void VideoCtl::VideoImageDisplay(VideoState* is)
{
    //qDebug() << "调用了VideoImageDisplay" << '\n';
    Frame* vp;
    Frame* sp = NULL;
    SDL_Rect rect;
    double current_time;

    // 从视频帧队列 pictq 中提取最新的帧 vp
    vp = frame_queue_peek_last(&is->pictq);

    // 计算视频显示矩形
    CalculateDisplayRect(&rect, is->xleft, is->ytop, is->width, is->height, vp->width, vp->height, vp->sar);

    //qDebug() << is->xleft << "  " << is->ytop << " " << is->width << " " << is->height << "  " << vp->width << "  " << vp->height << '\n';

    // 视频纹理处理
    if (!vp->uploaded &&is->no_video==false)
    {
        int sdl_pix_fmt = vp->frame->format == AV_PIX_FMT_YUV420P ? SDL_PIXELFORMAT_YV12 : SDL_PIXELFORMAT_ARGB8888;
        if (ReallocTexture(&is->vid_texture, sdl_pix_fmt, vp->frame->width, vp->frame->height, SDL_BLENDMODE_NONE, 0) < 0)
            return;

        // 上传视频帧到纹理
        if (UploadTexture(is->vid_texture, vp->frame, &is->img_convert_ctx) < 0)
            return;
        vp->uploaded = 1;
        vp->flip_v = vp->frame->linesize[0] < 0;

        // 通知宽高变化
        if (m_nFrameW != vp->frame->width || m_nFrameH != vp->frame->height)
        {
            m_nFrameW = vp->frame->width;
            m_nFrameH = vp->frame->height;
            emit SigFrameDimensionsChanged(m_nFrameW, m_nFrameH);
        }
    }

    // 渲染视频
    if (is->no_video)
    {
        //qDebug() << "将渲染本地图像" << '\n';
        if (is->default_tex)
        {
            int tex_width, tex_height;
            if (SDL_QueryTexture(is->default_tex, NULL, NULL, &tex_width, &tex_height) != 0)
            {
                qDebug() << "查询纹理失败：" << SDL_GetError();
                return;
            }

            SDL_Rect dstRect;
            dstRect.w = tex_width;
            dstRect.h = tex_height;
            dstRect.x = (is->width - tex_width) / 2;
            dstRect.y = (is->height - tex_height) / 2;

            if (dstRect.x < 0) { dstRect.x = 0; }
            if (dstRect.y < 0) { dstRect.y = 0; }
            if (dstRect.w > is->width) { dstRect.w = is->width; }
            if (dstRect.h > is->height) { dstRect.h = is->height; }


            if (SDL_RenderCopyEx(renderer, is->default_tex, NULL, &dstRect, 0, NULL, SDL_FLIP_NONE) != 0)
            {
                qDebug() << "渲染纹理失败：" << SDL_GetError();
            }
        }
        else
        {
            qDebug() << "default_tex 未创建，无法渲染图片。";
        }
    }
    else
    {
        //qDebug() << "将渲染图像" << '\n';
        SDL_RenderCopyEx(renderer, is->vid_texture, NULL, &rect, 0, NULL, (SDL_RendererFlip)(vp->flip_v ? SDL_FLIP_VERTICAL : 0));
    }
    // 获取当前时间
    current_time = GetMasterClock(is);

    // 处理字幕
    if (is->subtitle_st && frame_queue_nb_remaining(&is->subpq) > 0)
    {
        sp = frame_queue_peek(&is->subpq);
        double subtitle_start_time = sp->pts + ((float)sp->sub.start_display_time / 1000.0);
        double subtitle_end_time = sp->pts + ((float)sp->sub.end_display_time / 1000.0);

        if (current_time >= subtitle_start_time && current_time <= subtitle_end_time)
        {
            // 设置当前字幕
            is->current_subtitle.text = sp->subtitle_text;
            is->current_subtitle.end_time = subtitle_end_time;
            frame_queue_next(&is->subpq); // 移动到下一个字幕帧
        }
    }

    // 渲染当前字幕
    if (!is->current_subtitle.text.empty())
    {
        if (current_time > is->current_subtitle.end_time)
        {
            // 字幕显示时间结束，清除当前字幕
            is->current_subtitle.text.clear();
        }
        else
        {
            // 渲染字幕
            int text_width = 0;
            int text_height = 0;
            TTF_SizeUTF8(subtitle_font, is->current_subtitle.text.c_str(), &text_width, &text_height);

            // 动态计算字幕位置（屏幕底部中央）
            int text_x = (is->width - text_width) / 2;
            int text_y = is->height - text_height - 50; // 距离底部 50 像素

            // 渲染字幕文本
            if(wordPlay) RenderText(is->current_subtitle.text, text_x, text_y,1);
        }
    }

    // 显示渲染内容
    SDL_RenderPresent(renderer);
}
//关闭流对应的解码器等
// 关闭一个指定的媒体流（音频、视频或字幕），并释放与该流相关的资源
void VideoCtl::StreamComponentClose(VideoState* is, int stream_index)
{
    ClearSubtitleCache();
    AVFormatContext* ic = is->ic;
    AVCodecParameters* codecpar;

    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return;
    codecpar = ic->streams[stream_index]->codecpar;
 

    switch (codecpar->codec_type)
    {
    case AVMEDIA_TYPE_AUDIO: // 音频流关闭
        if (audio_dev) {
            SDL_PauseAudioDevice(audio_dev, 1);
        }
        decoder_abort(&is->auddec, &is->sampq);
        SDL_CloseAudio();
        decoder_destroy(&is->auddec);
        swr_free(&is->swr_ctx);
        av_freep(&is->audio_buf1);
        is->audio_buf1_size = 0;
        is->audio_buf = NULL;

        if (is->rdft)
        {
            av_rdft_end(is->rdft);
            av_freep(&is->rdft_data);
            is->rdft = NULL;
            is->rdft_bits = 0;
        }
        break;
    case AVMEDIA_TYPE_VIDEO: // 视频流关闭
        decoder_abort(&is->viddec, &is->pictq);
        decoder_destroy(&is->viddec);
        break;
    case AVMEDIA_TYPE_SUBTITLE: // 字幕流关闭
        decoder_abort(&is->subdec, &is->subpq);
        decoder_destroy(&is->subdec);
        break;
    default:
        break;
    }

    // 流丢弃和状态重置
    ic->streams[stream_index]->discard = AVDISCARD_ALL;
    switch (codecpar->codec_type)
    {
    case AVMEDIA_TYPE_AUDIO:
        is->audio_st = NULL;
        is->audio_stream = -1;
        break;
    case AVMEDIA_TYPE_VIDEO:
        is->video_st = NULL;
        is->video_stream = -1;
        break;
    case AVMEDIA_TYPE_SUBTITLE:
        is->subtitle_st = NULL;
        is->subtitle_stream = -1;
        break;
    default:
        break;
    }
}

//关闭流
void VideoCtl::StreamClose(VideoState* is)
{
    isRecording = false;
    recordStartTime = 0;
    recordEndTime = 0;
    ClearSubtitleCache();
    is->abort_request = 1;
    is->read_tid.join();

    if (is->audio_stream >= 0)
        StreamComponentClose(is, is->audio_stream);
    if (is->video_stream >= 0)
        StreamComponentClose(is, is->video_stream);
    if (is->subtitle_stream >= 0)
        StreamComponentClose(is, is->subtitle_stream);

    avformat_close_input(&is->ic);

    packet_queue_destroy(&is->videoq);
    packet_queue_destroy(&is->audioq);
    packet_queue_destroy(&is->subtitleq);

    frame_queue_destory(&is->pictq);
    frame_queue_destory(&is->sampq);
    frame_queue_destory(&is->subpq);
    SDL_DestroyCond(is->continue_read_thread);
    sws_freeContext(is->img_convert_ctx);
    sws_freeContext(is->sub_convert_ctx);
    av_free(is->filename);

    if (is->vid_texture)
        SDL_DestroyTexture(is->vid_texture);
    if (is->sub_texture)
        SDL_DestroyTexture(is->sub_texture);

    if (is->default_tex) {
        SDL_DestroyTexture(is->default_tex);
        is->default_tex = nullptr;
    }
    av_free(is);
}

// 用于获取时钟的当前时间值，主要用于音视频同步。通过检查时钟的状态（是否暂停）和计算时间的漂移量，它返回时钟的当前值
double VideoCtl::GetClock(Clock* c)
{
    if (*c->queue_serial != c->serial)
        return NAN;
    if (c->paused)
    {
        return c->pts;
    }
    else
    {
        double time = av_gettime_relative() / 1000000.0;
        return c->pts_drift + time - (time - c->last_updated) * (1.0 - c->speed);
    }
}

// 用来更新某个时钟(Clock)的关键属性
void VideoCtl::SetClockAt(Clock* c, double pts, int serial, double time)
{
    c->pts = pts;   // 设置当前时钟的 PTS
    c->last_updated = time;  // 记录“上一次更新时间”
    c->pts_drift = c->pts - time; // 计算并保存 'pts_drift'：PTS 与系统时间的差值
    c->serial = serial;  // 设置该时钟的串行号/序列号
}

// 设置指定时钟（Clock）的时间戳（pts）和序列号（serial），并将当前系统时间作为基准记录下来
void VideoCtl::SetClock(Clock* c, double pts, int serial)
{
    // 获取系统的当前时间戳 微秒时间除以 1,000,000，得到秒数表示
    double time = av_gettime_relative() / 1000000.0;
    SetClockAt(c, pts, serial, time);
}

// 用于设置时钟的播放速度（speed）。通过更新时钟的当前值和序列号，并设置新的速度参数
void VideoCtl::SetClockSpeed(Clock* c, double speed)
{
    SetClock(c, GetClock(c), c->serial);
    c->speed = speed;
}

// 初始化一个时钟（Clock）对象，为播放时钟设置初始状态
void VideoCtl::InitClock(Clock* c, int* queue_serial)
{
    c->speed = 1.0;
    c->paused = 0;
    c->queue_serial = queue_serial;
    SetClock(c, NAN, -1);
}

// 让时钟c同步到时钟slave
void VideoCtl::ClockToSlave(Clock* c, Clock* slave)
{
    double clock = GetClock(c);
    double slave_clock = GetClock(slave);
    if (!std::isnan(slave_clock) && (std::isnan(clock) || fabs(clock - slave_clock) > AV_NOSYNC_THRESHOLD)) SetClock(c, slave_clock, slave->serial);
}
float VideoCtl::ffp_get_property_float(int id, float default_value)
{
    return 0;
}
void VideoCtl::ffp_set_property_float(int id, float value)
{
    switch (id) {
    case FFP_PROP_FLOAT_PLAYBACK_RATE:
        ffp_set_playback_rate(value);
        break;
    default:
        return;
    }
}
bool VideoCtl::TakeScreenshot(QString& filePath)
{

    QMutexLocker locker(&show_rect);

    if (!renderer)
    {
        qDebug() << "Renderer not initialized.";
        return false;
    }
    int width, height;
    if (SDL_GetRendererOutputSize(renderer, &width, &height) != 0)
    {
        qDebug() << "SDL_GetRendererOutputSize failed:" << SDL_GetError();
        return false;
    }
    std::vector<Uint32> pixels(width * height);
    if (SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888, pixels.data(), width * sizeof(Uint32)) != 0)
    {
        qDebug() << "SDL_RenderReadPixels failed:" << SDL_GetError();
        return false;
    }
    QImage image(reinterpret_cast<const uchar*>(pixels.data()), width, height, QImage::Format_ARGB32);
    if (image.isNull())
    {
        qDebug() << "Failed to create QImage from pixel data.";
        return false;
    }
    QImage flipped = image;

    // 获取当前目录
    QString currentDir = QDir::currentPath();
    // 拼接截图文件夹路径
    QString screenshotDir = currentDir + "/Screenshot";

    // 检查文件夹是否存在，如果不存在则创建
    QDir dir;
    if (!dir.exists(screenshotDir))
    {
        if (!dir.mkpath(screenshotDir))
        {
            qDebug() << "Failed to create Screenshot directory.";
            return false;
        }
    }


    QString fileName = QString("screenshot_%1.png").arg(QRandomGenerator::global()->bounded(1000000, 9999999));
    QString fullPath = screenshotDir + "/" + fileName;


    if (!flipped.save(fullPath))
    {
        qDebug() << "Failed to save screenshot to" << fullPath;
        return false;
    }

    qDebug() << "Screenshot saved to:" << fullPath;
    filePath = fullPath; 
    return true;
}
void VideoCtl::OnSpeed(double newSpeed)
{
    const double MIN_SPEED = 0.0;
    const double MAX_SPEED = 2.0;

    qDebug() << "改了倍速为:" << newSpeed << '\n';
    speed = qBound(MIN_SPEED, newSpeed, MAX_SPEED);
    ischange = 1;

    ffp_set_playback_rate(speed);

    emit SigSpeed(speed);
}
// 决定视频、音频或外部时钟的同步方式
int VideoCtl::GetMasterType(VideoState* is)
{
    if (is->av_sync_type == AV_SYNC_VIDEO_MASTER) // 视频作为主时钟进行同步
    {
        if (is->video_st)
            return AV_SYNC_VIDEO_MASTER;
        else
            return AV_SYNC_AUDIO_MASTER;
    }
    else if (is->av_sync_type == AV_SYNC_AUDIO_MASTER) // 音频作为主时钟进行同步
    {
        if (is->audio_st)
            return AV_SYNC_AUDIO_MASTER;
        else
            return AV_SYNC_EXTERNAL_CLOCK;
    }
    else
    {
        return AV_SYNC_EXTERNAL_CLOCK; // 外部时钟作为主时钟进行同步
    }
}

// 用于获取播放器的主时钟值。根据播放器的同步设置（音频主时钟、视频主时钟或外部时钟），它选择相应的时钟并返回当前时刻
double VideoCtl::GetMasterClock(VideoState* is)
{
    double val;

    switch (GetMasterType(is)) {
    case AV_SYNC_VIDEO_MASTER:
        val = GetClock(&is->vidclk);
        break;
    case AV_SYNC_AUDIO_MASTER:
        val = GetClock(&is->audclk);
        break;
    default:
        val = GetClock(&is->extclk);
        break;
    }
    return val;
}

// 用于调整外部时钟的播放速度，确保视频和音频流的同步
// 在播放过程中根据视频帧和音频帧的队列长度动态调整时钟速度
void VideoCtl::CheckExternalClockSpeed(VideoState* is)
{

    if (is->video_stream >= 0 && is->videoq.nb_packets <= EXTERNAL_CLOCK_MIN_FRAMES ||
        is->audio_stream >= 0 && is->audioq.nb_packets <= EXTERNAL_CLOCK_MIN_FRAMES)
    {
        SetClockSpeed(&is->extclk, FFMAX(EXTERNAL_CLOCK_SPEED_MIN, is->extclk.speed - EXTERNAL_CLOCK_SPEED_STEP));
    }
    else if ((is->video_stream < 0 || is->videoq.nb_packets > EXTERNAL_CLOCK_MAX_FRAMES) &&
        (is->audio_stream < 0 || is->audioq.nb_packets > EXTERNAL_CLOCK_MAX_FRAMES))
    {
        SetClockSpeed(&is->extclk, FFMIN(EXTERNAL_CLOCK_SPEED_MAX, is->extclk.speed + EXTERNAL_CLOCK_SPEED_STEP));
    }
    else
    {
        double speed = is->extclk.speed;
        if (speed != 1.0)
            SetClockSpeed(&is->extclk, speed + EXTERNAL_CLOCK_SPEED_STEP * (1.0 - speed) / fabs(1.0 - speed));
    }
}

// 用于请求在视频流中进行“跳跃”操作，也就是在播放中间位置进行快进、快退等操作。它通过设置相关的标志和参数来通知播放器进行寻址操作
void VideoCtl::StreamSeek(VideoState* is, int64_t pos, int64_t rel)
{
    if (!is->seek_req)
    {
        is->seek_pos = pos;
        is->seek_rel = rel;
        is->seek_flags &= ~AVSEEK_FLAG_BYTE;
        is->seek_req = 1;
        SDL_CondSignal(is->continue_read_thread);
    }
}

// 切换视频流的暂停和播放状态。它不仅会控制视频播放的暂停与恢复，还会同步音频和外部时钟的暂停状态
void VideoCtl::StreamTogglePause(VideoState* is)
{
    // 如果 is->paused 为 true，表示当前视频处于暂停状态。此时，函数将切换到播放状态，恢复视频和音频的播放
    if (is->paused)
    {
        is->frame_timer += av_gettime_relative() / 1000000.0 - is->vidclk.last_updated;
        if (is->read_pause_return != AVERROR(ENOSYS)) {
            is->vidclk.paused = 0;
        }
        SetClock(&is->vidclk, GetClock(&is->vidclk), is->vidclk.serial); //更新视频时钟
    }
    SetClock(&is->extclk, GetClock(&is->extclk), is->extclk.serial);// 更新外部时钟

    // 切换暂停状态
    is->paused = is->audclk.paused = is->vidclk.paused = is->extclk.paused = !is->paused;
}

// 切换状态
void VideoCtl::TogglePause(VideoState* is)
{
    StreamTogglePause(is);

    // 禁用单步播放模式，确保视频流恢复为连续播放状态
    is->step = 0;
}


// 在暂停视频的情况下，切换到下一帧视频并进入单步播放模式
void VideoCtl::ToNextFrame(VideoState* is)
{
    if (is->paused)
        StreamTogglePause(is);
    is->step = 1;
}

// 用于计算目标显示帧的延迟时间（delay），确保视频与主同步源（如音频或外部时钟）保持同步
double VideoCtl::TargetDelay(double delay, VideoState* is)
{
    double sync_threshold, diff = 0;
    if (GetMasterType(is) != AV_SYNC_VIDEO_MASTER) {

        diff = GetClock(&is->vidclk) - GetMasterClock(is);

        // 设置同步阈值，用于判断是否需要跳帧或重复帧
        sync_threshold = FFMAX(AV_SYNC_THRESHOLD_MIN, FFMIN(AV_SYNC_THRESHOLD_MAX, delay));
        if (!std::isnan(diff) && fabs(diff) < is->max_frame_duration)
        {
            if (diff <= -sync_threshold)
                delay = FFMAX(0, delay + diff);
            else if (diff >= sync_threshold && delay > AV_SYNC_FRAMEDUP_THRESHOLD)
                delay = delay + diff;
            else if (diff >= sync_threshold)
                delay = 2 * delay;
        }
    }

    av_log(NULL, AV_LOG_TRACE, "video: delay=%0.3f A-V=%f\n", delay, -diff);
    return delay;
}

// 用于计算两个视频帧之间的持续时间（duration）
double VideoCtl::VpDuration(VideoState* is, Frame* vp, Frame* nextvp)
{
    if (vp->serial == nextvp->serial) {
        double duration = nextvp->pts - vp->pts;
        if (std::isnan(duration) || duration <= 0 || duration > is->max_frame_duration)
            return vp->duration / speed;
        else
            return duration / speed;
    }
    else {
        return 0.0;
    }
}

// 用于更新视频时钟（vidclk）的当前时间戳（PTS），并同步外部时钟（extclk）到视频时钟（vidclk）
void VideoCtl::UpdateVideoPts(VideoState* is, double pts, int64_t pos, int serial)
{
    SetClock(&is->vidclk, pts, serial);
    ClockToSlave(&is->extclk, &is->vidclk);
}
void VideoCtl::ffp_set_playback_rate(float rate)
{
    speed = rate;
    ischange = 1;
}
float VideoCtl::ffp_get_playback_rate()
{
    return speed;
}

int VideoCtl::ffp_get_playback_rate_change()
{
    return ischange;
}

void VideoCtl::ffp_set_playback_rate_change(int change)
{
    ischange = change;
}
int64_t VideoCtl::get_target_frequency()
{
    if (m_CurStream)
    {
        return m_CurStream->audio_tgt.freq;
    }
    return 44100;       // 默认
}
int VideoCtl::get_target_channels()
{
    if (m_CurStream)
    {
        return m_CurStream->audio_tgt.channels;     //目前变速只支持1/2通道
    }
    return 2;
}
int VideoCtl::is_normal_playback_rate()
{
    if (speed > 0.99 && speed < 1.01)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}
void VideoCtl::LoadSubtitle(const QString& subtitleFilePath)
{
    if (!m_CurStream)
    {
        qDebug() << "No video stream loaded. Cannot load subtitles.";
        QMessageBox::warning(nullptr, "警告", "没有加载的视频流，无法加载字幕。");
        return;
    }

    // 检查当前视频是否已有字幕流
    if (m_CurStream->subtitle_stream >= 0)
    {
        qDebug() << "Current video already has a subtitle stream. Skipping external subtitle loading.";
        QMessageBox::information(nullptr, "信息", "当前视频已有字幕流，无需加载外部字幕。");
        return;
    }

    // 构建输出文件路径
    QFileInfo inputFileInfo(Filename);
    QString inputDir = inputFileInfo.absolutePath();
    QString baseName = inputFileInfo.completeBaseName();
    QString newFileName = baseName + "_with_word.mp4"; // 新文件名
    QString outputFilePath = inputDir + "/" + newFileName;

    // 检查输出文件是否已存在
    if (QFile::exists(outputFilePath))
    {
        qDebug() << "Output file already exists. Using the existing file:" << outputFilePath;
        Filename = outputFilePath;
        emit SigStartPlay(Filename);
        emit ResolutionChanged(true, Filename, 0.0);
        return;
    }

    // 验证字幕文件是否存在
    QFileInfo subtitleFileInfo(subtitleFilePath);
    if (!subtitleFileInfo.exists())
    {
        qDebug() << "Subtitle file does not exist:" << subtitleFilePath;
        QMessageBox::critical(nullptr, "错误", "选定的字幕文件不存在。");
        return;
    }

    // 构建 FFmpeg 命令行参数
    QString ffmpegPath = "ffmpeg"; // 如果 FFmpeg 不在 PATH 中，请提供完整路径
    QStringList arguments;
    arguments << "-y" // 覆盖输出文件
        << "-i" << Filename // 输入视频
        << "-i" << subtitleFilePath // 输入字幕
        << "-c" << "copy" // 复制视频和音频流
        << "-c:s" << "mov_text" // 设置字幕流编码器
        << "-metadata:s:s:0" << "language=zho" // 设置字幕语言为中文
        << outputFilePath; // 输出文件

    qDebug() << "Starting FFmpeg with arguments:" << arguments;

    // 检查 FFmpeg 是否正在运行
    if (OnloadWord->state() != QProcess::NotRunning)
    {
        qDebug() << "FFmpeg is already running.";
        QMessageBox::warning(nullptr, "警告", "FFmpeg 进程已在运行，请等待完成后再试。");
        return;
    }

    // 启动 FFmpeg 进程
    OnloadWord->start(ffmpegPath, arguments);

    if (!OnloadWord->waitForStarted())
    {
        qDebug() << "Failed to start FFmpeg process:" << OnloadWord->errorString();
        QMessageBox::critical(nullptr, "错误", "无法启动 FFmpeg 进程。请确保 FFmpeg 已正确安装并在系统 PATH 中。");
        return;
    }

    qDebug() << "FFmpeg process started successfully.";
}
void VideoCtl::HideSubtitle()
{

}
// 负责从帧队列中提取帧并进行播放，同时处理视频同步、字幕同步和丢帧逻辑
void VideoCtl::VideoRefresh(void* opaque, double* remaining_time)
{
    VideoState* is = (VideoState*)opaque;
    double time;

    Frame* sp, * sp2;

    double rdftspeed = 0.02;

    // 检查外部时钟速度
    if (!is->paused && GetMasterType(is) == AV_SYNC_EXTERNAL_CLOCK && is->realtime)
        CheckExternalClockSpeed(is);

    if (is->video_st)
    {
    retry:
        // 视频帧队列检查
        if (frame_queue_nb_remaining(&is->pictq) == 0)
        {
        }
        else {
            double last_duration, duration, delay;
            Frame* vp, * lastvp;

            // 提取当前和上一帧
            lastvp = frame_queue_peek_last(&is->pictq);
            vp = frame_queue_peek(&is->pictq);

            if (vp->serial != is->videoq.serial)
            {
                frame_queue_next(&is->pictq);
                goto retry;
            }

            // 更新帧计时器 如果上一帧和当前帧属于不同的解码序列（serial 不同），重置帧计时器
            if (lastvp->serial != vp->serial)
                is->frame_timer = av_gettime_relative() / 1000000.0;

            if (is->paused)
                goto display;

            // 计算帧显示延迟
            last_duration = VpDuration(is, lastvp, vp); // 帧间持续时间
            delay = TargetDelay(last_duration, is); // 目标延迟

            //  判断是否需要等待
            // 如果当前时间未达到下一帧显示时间，更新剩余时间并跳到显示逻辑
            time = av_gettime_relative() / 1000000.0;
            if (time < is->frame_timer + delay)
            {
                *remaining_time = FFMIN(is->frame_timer + delay - time, *remaining_time);
                goto display;
            }


            // 更新帧计时器并丢弃过期帧
            is->frame_timer += delay;
            if (delay > 0 && time - is->frame_timer > AV_SYNC_THRESHOLD_MAX) is->frame_timer = time;

            SDL_LockMutex(is->pictq.mutex);
            if (!std::isnan(vp->pts)) UpdateVideoPts(is, vp->pts, vp->pos, vp->serial);
            SDL_UnlockMutex(is->pictq.mutex);


            if (frame_queue_nb_remaining(&is->pictq) > 1)
            {
                Frame* nextvp = frame_queue_peek_next(&is->pictq);
                duration = VpDuration(is, vp, nextvp);
                if (!is->step && (framedrop > 0 || (framedrop && GetMasterType(is) != AV_SYNC_VIDEO_MASTER)) && time > is->frame_timer + duration) {
                    is->frame_drops_late++;
                    frame_queue_next(&is->pictq);
                    goto retry;
                }
            }

            // 字幕同步逻辑
            if (is->subtitle_st)
            {
                // 遍历字幕队列，检查当前字幕是否需要显示或隐藏
                // 如果字幕超时（当前视频时间超出字幕显示结束时间），从队列中移除
                while (frame_queue_nb_remaining(&is->subpq) > 0)
                {
                    sp = frame_queue_peek(&is->subpq);

                    if (frame_queue_nb_remaining(&is->subpq) > 1)
                        sp2 = frame_queue_peek_next(&is->subpq);
                    else
                        sp2 = NULL;
                    if (sp->serial != is->subtitleq.serial
                        || (is->vidclk.pts > (sp->pts + ((float)sp->sub.end_display_time / 1000)))
                        || (sp2 && is->vidclk.pts > (sp2->pts + ((float)sp2->sub.start_display_time / 1000))))
                    {
                        if (sp->uploaded)
                        {
                            int i;
                            for (i = 0; i < sp->sub.num_rects; i++)
                            {
                                AVSubtitleRect* sub_rect = sp->sub.rects[i];
                                uint8_t* pixels;
                                int pitch, j;

                                if (!SDL_LockTexture(is->sub_texture, (SDL_Rect*)sub_rect, (void**)&pixels, &pitch))
                                {
                                    for (j = 0; j < sub_rect->h; j++, pixels += pitch)
                                        memset(pixels, 0, sub_rect->w << 2);
                                    SDL_UnlockTexture(is->sub_texture);
                                }
                            }
                        }
                        frame_queue_next(&is->subpq);
                    }
                    else
                    {
                        break;
                    }
                }
            }

            frame_queue_next(&is->pictq);
            is->force_refresh = 1;

            if (is->step && !is->paused)
                StreamTogglePause(is);
        }
    display:
        // 帧显示逻辑
        // 如果设置了 force_refresh 并且当前帧已被标记为显示，调用 VideoDisplay 进行实际的帧显示。
        if (is->force_refresh && is->pictq.rindex_shown)
            VideoDisplay(is);
    }

    if (is->no_video)
    {
        is->force_refresh = 1;
        VideoDisplay(is);
    }

    is->force_refresh = 0;

    // 发出播放时间信号
    emit SigVideoPlaySeconds(GetMasterClock(is));
}

// 将一个解码后的视频帧加入到视频帧队列（pictq）中，供后续的显示刷新使用
int VideoCtl::QueuePicture(VideoState* is, AVFrame* src_frame, double pts, double duration, int64_t pos, int serial)
{
    Frame* vp;

    if (!(vp = frame_queue_peek_writable(&is->pictq))) return -1;

    vp->sar = src_frame->sample_aspect_ratio;
    vp->uploaded = 0;

    vp->width = src_frame->width;
    vp->height = src_frame->height;
    vp->format = src_frame->format;

    vp->pts = pts;
    vp->duration = duration;
    vp->pos = pos;
    vp->serial = serial;

    av_frame_move_ref(vp->frame, src_frame);
    frame_queue_push(&is->pictq);
    return 0;
}

//从视频队列中获取数据，并解码数据，得到可显示的视频帧
int VideoCtl::GetVideoFrame(VideoState* is, AVFrame* frame)
{
    // 从视频解码器取出一帧已解码的视频数据（AVFrame），并根据主时钟和丢帧策略判断这帧是否已经迟到太多而需要丢帧。

    int flag;  // 标记是否成功获得解码帧（为 1 表示成功，0 表示还没有拿到帧或者丢帧)

    if ((flag = decoder_decode_frame(&is->viddec, frame, NULL)) < 0) return -1;

    if (flag)
    {
        double dpts = NAN;  // PTS
        //计算帧的显示时间戳
        if (frame->pts != AV_NOPTS_VALUE) dpts = av_q2d(is->video_st->time_base) * frame->pts;

        //推断帧的显示宽高比
        frame->sample_aspect_ratio = av_guess_sample_aspect_ratio(is->ic, is->video_st, frame);

        // 提前丢帧 防卡顿 实际用处不大
        if (framedrop > 0 || (framedrop && GetMasterType(is) != AV_SYNC_VIDEO_MASTER))
        {
            if (frame->pts != AV_NOPTS_VALUE)
            {
                double diff = dpts - GetMasterClock(is);
                if (!isnan(diff) && fabs(diff) < AV_NOSYNC_THRESHOLD && diff - is->frame_last_filter_delay < 0 && is->viddec.pkt_serial == is->vidclk.serial && is->videoq.nb_packets)
                {
                    is->frame_drops_early++; av_frame_unref(frame);
                    flag = 0;
                }
            }
        }
    }

    return flag;
}

// 音频解码线程： 音频解码并将解码后的帧压入音频帧队列
int VideoCtl::AudioThread(void* arg)
{
    qDebug() << "调用了音频解码线程";
    VideoState* is = (VideoState*)arg;   // 播放器状态
    AVFrame* frame = av_frame_alloc();// 用来接收音频解码后的帧数据
    Frame* af;  // 稍后要往音频队列里放的 Frame

    int flag = 0;   // 解码器是否产出了一帧
    AVRational tb;  // 时间基，后面用于计算 pts
    int ret = 0;   // 用于保存解码/循环返回码

    if (!frame) return AVERROR(ENOMEM);

    do
    {
        // 解码一帧
        if ((flag = decoder_decode_frame(&is->auddec, frame, NULL)) < 0) goto end;

        if (flag)
        {
            // 写入音频帧队列sampq
            tb = { 1, frame->sample_rate };

            //// 获取“可写”队列元素，如果队列满了，就会阻塞或失败
            if (!(af = frame_queue_peek_writable(&is->sampq)))
                goto end;

            // 计算音频帧的 pts（秒）
            af->pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);

            // 记录相关信息
            af->pos = frame->pkt_pos;
            af->serial = is->auddec.pkt_serial;
            af->duration = av_q2d({ frame->nb_samples, frame->sample_rate });

            // 将 AVFrame 转移到自定义 Frame->frame 里
            av_frame_move_ref(af->frame, frame);

            // 通知队列，这个帧写完了，可以被读取
            frame_queue_push(&is->sampq);

        }
    } while (ret >= 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF);
end:

    av_frame_free(&frame);
    return ret;
}

//视频解码线程,从视频解码器中不断获取解码后的视频帧，然后将其放入显示队列
int VideoCtl::VideoThread(void* arg)
{
    qDebug() << "调用了视频解码线程";
    VideoState* is = (VideoState*)arg;
    AVFrame* frame = av_frame_alloc();
    double pts; // PTS
    double duration; // 时长
    int ret;
    AVRational tb = is->video_st->time_base;  // 当前视频流的时间基
    AVRational frame_rate = av_guess_frame_rate(is->ic, is->video_st, NULL);

    if (!frame)
    {
        return AVERROR(ENOMEM);
    }

    //循环从队列中获取视频帧
    for (;;)
    {
        // 从解码器或视频队列中获取解码后的帧
        ret = GetVideoFrame(is, frame);
        if (ret < 0) goto end;
        if (!ret) continue; // 如果没拿到帧（或帧被丢弃），继续下一次循环

        // 计算一帧的预估持续时间 (duration)
        // 如果能成功获取 frame_rate (fps)，则 duration = 1 / fps

        duration = (frame_rate.num && frame_rate.den ? av_q2d({ frame_rate.den, frame_rate.num }) : 0);

        // 计算帧的播放时间戳 pts（单位：秒）
        // 若 frame->pts 无效，则置为 NAN
        pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);

        // 将解码好的帧放入显示队列
        // queue_picture() 内部会将该帧及其参数(pts、duration等)
        // 存到播放器的渲染队列
        ret = QueuePicture(is, frame, pts, duration, frame->pkt_pos, is->viddec.pkt_serial);
        av_frame_unref(frame);

        if (ret < 0)
            goto end;
    }
end:
    av_frame_free(&frame);
    return 0;
}

// 字幕解码线程,从字幕解码器中获取解码后的字幕数据，然后将这些数据包装成Frame并放入字幕队列
int VideoCtl::SubTitleThread(void* arg)
{
    qDebug() << "调用了字幕解码线程";
    // 从字幕解码器中获取解码后的字幕数据
    VideoState* is = (VideoState*)arg;   // 播放器状态
    Frame* sp;  // 字幕帧的指针，用于存储解码后的字幕数据
    int flag;   // 标记是否成功解码字幕
    double pts;  // 字幕帧的显示时间戳

    for (;;)
    {
        if (!(sp = frame_queue_peek_writable(&is->subpq)))  // 从字幕队列获取一个可写帧
            return 0;

        //  从字幕解码器中解码一帧字幕
        if ((flag = decoder_decode_frame(&is->subdec, NULL, &sp->sub)) < 0) break;

        pts = 0;

        if (flag)
        {
            qDebug() << "解码出字幕帧，PTS:" << sp->sub.pts;

            if (sp->sub.pts != AV_NOPTS_VALUE)
                pts = sp->sub.pts / (double)AV_TIME_BASE;
            sp->pts = pts;
            sp->serial = is->subdec.pkt_serial;
            sp->width = is->subdec.avctx->width;
            sp->height = is->subdec.avctx->height;
            sp->uploaded = 0;
            sp->subtitle_text.clear();
            for (int i = 0; i < sp->sub.num_rects; i++)
            {
                AVSubtitleRect* sub_rect = sp->sub.rects[i];
                if (sub_rect->type == SUBTITLE_TEXT || sub_rect->type == SUBTITLE_ASS)
                {
                    std::string text = "";
                    if (sub_rect->text)
                        text += std::string(sub_rect->text);
                    if (sub_rect->ass)
                        text += std::string(sub_rect->ass);

                    // 检查是否包含 "Dialogue:" 前缀
                    size_t dialogue_pos = text.find("Dialogue:");
                    if (dialogue_pos != std::string::npos)
                    {
                        // ASS 格式的字幕行通常包含多个逗号，实际字幕文本位于第 10 个逗号之后
                        int comma_count = 0;
                        size_t pos = 0;
                        while (comma_count < 9 && (pos = text.find(',', pos)) != std::string::npos)
                        {
                            pos++;
                            comma_count++;
                        }
                        if (comma_count == 9 && pos < text.size())
                        {
                            std::string dialogue = text.substr(pos);
                            sp->subtitle_text += dialogue + "\n";
                        }
                        else
                        {
                            // 如果格式不符合预期，保留原始文本作为回退
                            sp->subtitle_text += text + "\n";
                        }
                    }
                    else
                    {
                        // 对于不包含 "Dialogue:" 的字幕行，直接使用原始文本
                        sp->subtitle_text += text + "\n";
                    }
                }
            }

            qDebug() << "提取字幕文本：" << QString::fromStdString(sp->subtitle_text);

            frame_queue_push(&is->subpq);
        }
        else
        {
            qDebug() << "不支持的字幕格式或解码失败";
            avsubtitle_free(&sp->sub);
        }
    }
    return 0;
}

// 在音频播放过程中对音频帧的采样数进行调整，以实现音频与主时钟的同步,保证音视频同步
int VideoCtl::SynchronizeAudio(VideoState* is, int nb_samples)
{
    int wanted_nb_samples = nb_samples;  // 期望的采样数与当前音频帧的采样数一致

    //判断是否需要同步
    // 如果播放器的主时钟不是音频时钟,则需要考虑同步校正
    if (GetMasterType(is) != AV_SYNC_AUDIO_MASTER)
    {
        double diff, avg_diff;
        int min_nb_samples, max_nb_samples;

        // get_clock 获取音频时钟当前时间
        // get_master_clock 获得主时钟当前时间
        diff = GetClock(&is->audclk) - GetMasterClock(is);  // 表示音频时钟与主时钟的时间差

        // 判断是否需要校正
        if (!std::isnan(diff) && fabs(diff) < AV_NOSYNC_THRESHOLD)
        {
            // 累积的时间差，包含当前帧的 diff 和历史的加权平均时间差
            // audio_diff_avg_coef 时间差累积的衰减系数，用于平滑变化，避免剧烈波动

            is->audio_diff_cum = diff + is->audio_diff_avg_coef * is->audio_diff_cum;

            // audio_diff_avg_count 记录累积的帧数
            if (is->audio_diff_avg_count < AUDIO_DIFF_AVG_NB)
            {
                is->audio_diff_avg_count++;
            }
            else
            {
                avg_diff = is->audio_diff_cum * (1.0 - is->audio_diff_avg_coef);

                // 校正采样数
                if (fabs(avg_diff) >= is->audio_diff_threshold)
                {
                    wanted_nb_samples = nb_samples + (int)(diff * is->audio_src.freq);
                    min_nb_samples = ((nb_samples * (100 - SAMPLE_CORRECTION_PERCENT_MAX) / 100));
                    max_nb_samples = ((nb_samples * (100 + SAMPLE_CORRECTION_PERCENT_MAX) / 100));
                    wanted_nb_samples = av_clip(wanted_nb_samples, min_nb_samples, max_nb_samples);
                }
            }
        }
        else
        {
            //  超大时间差的处理:直接重置时间差的累积统计
            is->audio_diff_avg_count = 0;
            is->audio_diff_cum = 0;
        }
    }

    return wanted_nb_samples; // 返回调整后的采样数
}

// 解码并重采样音频帧
int VideoCtl::AudioDecodeFromFrame(VideoState* is)
{
    int data_size, resampled_data_size;
    int64_t dec_channel_layout;
    av_unused double audio_clock0;
    int wanted_nb_samples;
    Frame* af;

    // 检查是否暂停
    if (is->paused)  return -1;

    //从音频帧队列中取可读帧
    do
    {
        if (!(af = frame_queue_peek_readable(&is->sampq))) return -1;
        frame_queue_next(&is->sampq);
    } while (af->serial != is->audioq.serial);

    //计算当前帧的原始大小
    data_size = av_samples_get_buffer_size(NULL, av_frame_get_channels(af->frame),
        af->frame->nb_samples,
        (AVSampleFormat)af->frame->format, 1);


    dec_channel_layout =
        (af->frame->channel_layout && av_frame_get_channels(af->frame) == av_get_channel_layout_nb_channels(af->frame->channel_layout)) ?
        af->frame->channel_layout : av_get_default_channel_layout(av_frame_get_channels(af->frame));
    // 音频同步
    wanted_nb_samples = SynchronizeAudio(is, af->frame->nb_samples);


    //当音频参数发生变化时，需要重新初始化或创建swr_ctx,否则可以复用已有的重采样上下文
    if (af->frame->format != is->audio_src.fmt ||dec_channel_layout != is->audio_src.channel_layout ||af->frame->sample_rate != is->audio_src.freq ||(wanted_nb_samples != af->frame->nb_samples && !is->swr_ctx))
    {
        swr_free(&is->swr_ctx);
        is->swr_ctx = swr_alloc_set_opts(NULL,
            is->audio_tgt.channel_layout, is->audio_tgt.fmt, is->audio_tgt.freq,
            dec_channel_layout, (AVSampleFormat)af->frame->format, af->frame->sample_rate,
            0, NULL);
        if (!is->swr_ctx || swr_init(is->swr_ctx) < 0) {
            av_log(NULL, AV_LOG_ERROR,
                "Cannot create sample rate converter for conversion of %d Hz %s %d channels to %d Hz %s %d channels!\n",
                af->frame->sample_rate, av_get_sample_fmt_name((AVSampleFormat)af->frame->format), av_frame_get_channels(af->frame),
                is->audio_tgt.freq, av_get_sample_fmt_name(is->audio_tgt.fmt), is->audio_tgt.channels);
            swr_free(&is->swr_ctx);
            return -1;
        }
        is->audio_src.channel_layout = dec_channel_layout;
        is->audio_src.channels = av_frame_get_channels(af->frame);
        is->audio_src.freq = af->frame->sample_rate;
        is->audio_src.fmt = (AVSampleFormat)af->frame->format;
    }

    if (is->swr_ctx)
    {
        // 使用 swr_convert 做重采样
        const uint8_t** in = (const uint8_t**)af->frame->extended_data;
        uint8_t** out = &is->audio_buf1;
        int out_count = (int64_t)wanted_nb_samples * is->audio_tgt.freq / af->frame->sample_rate + 256;
        int out_size = av_samples_get_buffer_size(NULL, is->audio_tgt.channels, out_count, is->audio_tgt.fmt, 0);
        int len2;
        if (out_size < 0)
        {
            return -1;
        }
        if (wanted_nb_samples != af->frame->nb_samples)
        {
            if (swr_set_compensation(is->swr_ctx, (wanted_nb_samples - af->frame->nb_samples) * is->audio_tgt.freq / af->frame->sample_rate,wanted_nb_samples * is->audio_tgt.freq / af->frame->sample_rate) < 0)
            {
                return -1;
            }
        }
        av_fast_malloc(&is->audio_buf1, &is->audio_buf1_size, out_size);
        if (!is->audio_buf1) return AVERROR(ENOMEM);
        len2 = swr_convert(is->swr_ctx, out, out_count, in, af->frame->nb_samples);
        if (len2 < 0)
        {
            return -1;
        }
        if (len2 == out_count)
        {
            if (swr_init(is->swr_ctx) < 0) swr_free(&is->swr_ctx);
        }
        is->audio_buf = is->audio_buf1;
        resampled_data_size = len2 * is->audio_tgt.channels * av_get_bytes_per_sample(is->audio_tgt.fmt);
    }
    else
    {
        // 无需重采样，直接使用原帧数据
        is->audio_buf = af->frame->data[0];
        resampled_data_size = data_size;
    }

    // 更新音频时钟
    audio_clock0 = is->audio_clock;
    if (!isnan(af->pts)) is->audio_clock = af->pts + (double)af->frame->nb_samples / af->frame->sample_rate;
    else
        is->audio_clock = NAN;
    is->audio_clock_serial = af->serial;

    // 返回最终可播放的音频数据大小
    return resampled_data_size;
}


// 负责将解码后的音频数据拷贝到音频设备中，支持播放控制和音频与视频的同步
void sdl_audio_callback(void* opaque, Uint8* stream, int len)
{
    VideoState* is = (VideoState*)opaque;
    int audio_size, len1;

    VideoCtl* pVideoCtl = VideoCtl::GetInstance();

    audio_callback_time = av_gettime_relative();
    while (len > 0)
    {
        // 如果音频缓冲区中的数据已全部使用完，调用 AudioDecodeFromFrame 获取新的解码音频数据。
        if (is->audio_buf_index >= is->audio_buf_size)
        {
            audio_size = pVideoCtl->AudioDecodeFromFrame(is);
            if (audio_size < 0)
            {
                is->audio_buf = NULL;
                is->audio_buf_size = SDL_AUDIO_MIN_BUFFER_SIZE / is->audio_tgt.frame_size * is->audio_tgt.frame_size;
            }
            else
            {
                is->audio_buf_size = audio_size;
            }
            is->audio_buf_index = 0;

            if (pVideoCtl->ffp_get_playback_rate_change())
            {
                pVideoCtl->ffp_set_playback_rate_change(0);
                // 初始化
                if (pVideoCtl->audio_speed_convert)
                {
                    // 先释放
                    sonicDestroyStream(pVideoCtl->audio_speed_convert);

                }
                // 再创建
                pVideoCtl->audio_speed_convert = sonicCreateStream(pVideoCtl->get_target_frequency(),
                    pVideoCtl->get_target_channels());

                if (pVideoCtl->audio_speed_convert)
                {
                    sonicSetSpeed(pVideoCtl->audio_speed_convert, pVideoCtl->ffp_get_playback_rate());
                    sonicSetPitch(pVideoCtl->audio_speed_convert, 1.0);
                    sonicSetRate(pVideoCtl->audio_speed_convert, 1.0);
                }
            }
            if (!pVideoCtl->is_normal_playback_rate() && is->audio_buf)
            {
                // 不是正常播放则需要修改
                // 需要修改  is->audio_buf_index is->audio_buf_size is->audio_buf
                int actual_out_samples = is->audio_buf_size /
                    (is->audio_tgt.channels * av_get_bytes_per_sample(is->audio_tgt.fmt));
                // 计算处理后的点数
                int out_ret = 0;
                int out_size = 0;
                int num_samples = 0;
                int sonic_samples = 0;
                if (is->audio_tgt.fmt == AV_SAMPLE_FMT_FLT)
                {
                    out_ret = sonicWriteFloatToStream(pVideoCtl->audio_speed_convert,
                        (float*)is->audio_buf,
                        actual_out_samples);
                }
                else  if (is->audio_tgt.fmt == AV_SAMPLE_FMT_S16)
                {
                    out_ret = sonicWriteShortToStream(pVideoCtl->audio_speed_convert,
                        (short*)is->audio_buf,
                        actual_out_samples);
                }
                else
                {
                    av_log(NULL, AV_LOG_ERROR, "sonic unspport ......\n");
                }
                num_samples = sonicSamplesAvailable(pVideoCtl->audio_speed_convert);
                // 2通道  目前只支持2通道的
                out_size = (num_samples)*av_get_bytes_per_sample(is->audio_tgt.fmt) * is->audio_tgt.channels;

                av_fast_malloc(&is->audio_buf1, &is->audio_buf1_size, out_size);
                if (out_ret)
                {
                    // 从流中读取处理好的数据
                    if (is->audio_tgt.fmt == AV_SAMPLE_FMT_FLT)
                    {
                        sonic_samples = sonicReadFloatFromStream(pVideoCtl->audio_speed_convert,
                            (float*)is->audio_buf1,
                            num_samples);
                    }
                    else  if (is->audio_tgt.fmt == AV_SAMPLE_FMT_S16)
                    {
                        sonic_samples = sonicReadShortFromStream(pVideoCtl->audio_speed_convert,
                            (short*)is->audio_buf1,
                            num_samples);
                    }
                    else
                    {
                        qDebug() << "sonic unspport ......";
                    }
                    is->audio_buf = is->audio_buf1;
                    is->audio_buf_size = sonic_samples * is->audio_tgt.channels * av_get_bytes_per_sample(is->audio_tgt.fmt);
                    is->audio_buf_index = 0;
                }
            }
        }
        if (is->audio_buf_size == 0)
            continue;
        // 计算本次填充长度
        len1 = is->audio_buf_size - is->audio_buf_index;
        if (len1 > len)
            len1 = len;


        if (is->audio_buf && is->audio_volume == SDL_MIX_MAXVOLUME)
            memcpy(stream, (uint8_t*)is->audio_buf + is->audio_buf_index, len1);
        else
        {
            memset(stream, 0, len1);
            if (is->audio_buf)
                SDL_MixAudio(stream, (uint8_t*)is->audio_buf + is->audio_buf_index, len1, is->audio_volume);
        }

        //更新缓冲区状态
        len -= len1;
        stream += len1;
        is->audio_buf_index += len1;
    }

    // 更新音频写入缓冲区大小
    //is->audio_write_buf_size = is->audio_buf_size - is->audio_buf_index;

    //更新音频时钟并同步外部时钟
    if (!std::isnan(is->audio_clock))
    {
        pVideoCtl->SetClockAt(&is->audclk, is->audio_clock - (double)(2 * is->audio_hw_buf_size + is->audio_write_buf_size) / is->audio_tgt.bytes_per_sec, is->audio_clock_serial, audio_callback_time / 1000000.0);
        pVideoCtl->ClockToSlave(&is->extclk, &is->audclk);
    }
}

// 初始化音频设备（通过SDL），并根据播放器的需求配置音频设备的参数（如采样率、通道布局等）
int VideoCtl::AudioOpen(void* opaque, int64_t wanted_channel_layout, int wanted_nb_channels, int wanted_sample_rate,struct AudioParams* audio_hw_params)
{

    SDL_AudioSpec wanted_spec, spec;
    const char* env;

    static const int next_nb_channels[] = { 0, 0, 1, 6, 2, 6, 4, 6 };
    static const int next_sample_rates[] = { 0, 44100, 48000, 96000, 192000 };

    int next_sample_rate_idx = FF_ARRAY_ELEMS(next_sample_rates) - 1;

    env = SDL_getenv("SDL_AUDIO_CHANNELS");

    if (env)
    {
        wanted_nb_channels = atoi(env);
        wanted_channel_layout = av_get_default_channel_layout(wanted_nb_channels);
    }
    if (!wanted_channel_layout || wanted_nb_channels != av_get_channel_layout_nb_channels(wanted_channel_layout)) {
        wanted_channel_layout = av_get_default_channel_layout(wanted_nb_channels);
        wanted_channel_layout &= ~AV_CH_LAYOUT_STEREO_DOWNMIX;
    }
    wanted_nb_channels = av_get_channel_layout_nb_channels(wanted_channel_layout);
    wanted_spec.channels = wanted_nb_channels;
    wanted_spec.freq = wanted_sample_rate;
    if (wanted_spec.freq <= 0 || wanted_spec.channels <= 0)
    {
        return -1;
    }

    while (next_sample_rate_idx && next_sample_rates[next_sample_rate_idx] >= wanted_spec.freq) next_sample_rate_idx--;

    // 设置期望的音频格式
    wanted_spec.format = AUDIO_S16SYS;  // 16 位系统字节序音频
    wanted_spec.silence = 0; // 静音值，填充为0
    // 缓冲区大小（采样数），根据音频频率和回调次数计算，确保缓冲区足够大
    wanted_spec.samples = FFMAX(SDL_AUDIO_MIN_BUFFER_SIZE, 2 << av_log2(wanted_spec.freq / SDL_AUDIO_MAX_CALLBACKS_PER_SEC));
    // SDL 音频回调函数，用于填充音频数据
    wanted_spec.callback = sdl_audio_callback;
    // 用户数据，传递给回调函数
    wanted_spec.userdata = opaque;

    // 打开音频设备
    while (SDL_OpenAudio(&wanted_spec, &spec) < 0)
    {
        av_log(NULL, AV_LOG_WARNING, "SDL_OpenAudio (%d channels, %d Hz): %s\n",
            wanted_spec.channels, wanted_spec.freq, SDL_GetError());
        wanted_spec.channels = next_nb_channels[FFMIN(7, wanted_spec.channels)];
        if (!wanted_spec.channels) {
            wanted_spec.freq = next_sample_rates[next_sample_rate_idx--];
            wanted_spec.channels = wanted_nb_channels;
            if (!wanted_spec.freq) {
                av_log(NULL, AV_LOG_ERROR,
                    "No more combinations to try, audio open failed\n");
                return -1;
            }
        }
        wanted_channel_layout = av_get_default_channel_layout(wanted_spec.channels);
    }

    if (spec.channels != wanted_spec.channels)
    {
        wanted_channel_layout = av_get_default_channel_layout(spec.channels);
        if (!wanted_channel_layout) {
            av_log(NULL, AV_LOG_ERROR,
                "SDL advised channel count %d is not supported!\n", spec.channels);
            return -1;
        }
    }

    switch (spec.format)
    {
    case AUDIO_U8:
        audio_hw_params->fmt = AV_SAMPLE_FMT_U8;
        break;
    case AUDIO_S16LSB:
    case AUDIO_S16MSB:
        audio_hw_params->fmt = AV_SAMPLE_FMT_S16;
        break;
    case AUDIO_S32LSB:
    case AUDIO_S32MSB:
        audio_hw_params->fmt = AV_SAMPLE_FMT_S32;
        break;
    case AUDIO_F32LSB:
    case AUDIO_F32MSB:
        audio_hw_params->fmt = AV_SAMPLE_FMT_FLT;
        break;
    default:
        audio_hw_params->fmt = AV_SAMPLE_FMT_U8;
        break;
    }

    audio_hw_params->freq = spec.freq;
    audio_hw_params->channel_layout = wanted_channel_layout;
    audio_hw_params->channels = spec.channels;
    audio_hw_params->frame_size = av_samples_get_buffer_size(NULL, audio_hw_params->channels, 1, audio_hw_params->fmt, 1);
    audio_hw_params->bytes_per_sec = av_samples_get_buffer_size(NULL, audio_hw_params->channels, audio_hw_params->freq, audio_hw_params->fmt, 1);
    if (audio_hw_params->bytes_per_sec <= 0 || audio_hw_params->frame_size <= 0)
    {
        av_log(NULL, AV_LOG_ERROR, "av_samples_get_buffer_size failed\n");
        return -1;
    }
    return spec.size;
}


// 打开指定的媒体流（音频、视频或字幕），为解码器配置必要的参数并初始化解码线程，同时为音频和视频流设置解码器上下文和相关资源
int VideoCtl::StreamOpen(VideoState* is, int stream_index)
{
    AVFormatContext* ic = is->ic;
    AVCodecContext* avctx;
    AVCodec* codec;

    const char* forced_codec_name = NULL;
    AVDictionary* opts = NULL;
    AVDictionaryEntry* t = NULL;
    int sample_rate, nb_channels;
    int64_t channel_layout;
    int ret = 0;
    int stream_lowres = 0;

    // 检查流索引
    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return -1;

    // 初始化解码器上下文
    avctx = avcodec_alloc_context3(NULL); // 分配解码器上下文
    if (!avctx) return AVERROR(ENOMEM);

    ret = avcodec_parameters_to_context(avctx, ic->streams[stream_index]->codecpar); //复制流参数到解码器上下文

    if (ret < 0)  goto fail;

    // 将流的时间基（time_base）设置到解码器上下文，用于时间戳计算
    av_codec_set_pkt_timebase(avctx, ic->streams[stream_index]->time_base);

    //查找并设置解码器
    codec = avcodec_find_decoder(avctx->codec_id);

    switch (avctx->codec_type)  // 音频、视频、字幕解码器
    {
    case AVMEDIA_TYPE_AUDIO: is->last_audio_stream = stream_index; break;
    case AVMEDIA_TYPE_SUBTITLE: is->last_subtitle_stream = stream_index; break;
    case AVMEDIA_TYPE_VIDEO: is->last_video_stream = stream_index; break;
    }
    if (!codec)
    {
        ret = AVERROR(EINVAL);
        goto fail;
    }

    // 设置解码器 ID
    avctx->codec_id = codec->id;
    if (stream_lowres > av_codec_get_max_lowres(codec))
    {
        av_log(avctx, AV_LOG_WARNING, "The maximum value for lowres supported by the decoder is %d\n",
            av_codec_get_max_lowres(codec));
        stream_lowres = av_codec_get_max_lowres(codec);
    }
    av_codec_set_lowres(avctx, stream_lowres);

#if FF_API_EMU_EDGE
    if (stream_lowres) avctx->flags |= CODEC_FLAG_EMU_EDGE;
#endif

#if FF_API_EMU_EDGE
    if (codec->capabilities & AV_CODEC_CAP_DR1)
        avctx->flags |= CODEC_FLAG_EMU_EDGE;
#endif

    opts = nullptr;
    if (!av_dict_get(opts, "threads", NULL, 0))
        av_dict_set(&opts, "threads", "auto", 0);
    if (stream_lowres)
        av_dict_set_int(&opts, "lowres", stream_lowres, 0);
    if (avctx->codec_type == AVMEDIA_TYPE_VIDEO || avctx->codec_type == AVMEDIA_TYPE_AUDIO)
        av_dict_set(&opts, "refcounted_frames", "1", 0);
        
    // 调用 avcodec_open2 打开解码器
    if ((ret = avcodec_open2(avctx, codec, &opts)) < 0)
    {
        goto fail;
    }
    if ((t = av_dict_get(opts, "", NULL, AV_DICT_IGNORE_SUFFIX)))
    {
        av_log(NULL, AV_LOG_ERROR, "Option %s not found.\n", t->key);
        ret = AVERROR_OPTION_NOT_FOUND;
        goto fail;
    }

    is->eof = 0;
    ic->streams[stream_index]->discard = AVDISCARD_DEFAULT;
    //  根据流类型初始化
    switch (avctx->codec_type)
    {
    case AVMEDIA_TYPE_AUDIO: // 音频流

        // 配置音频过滤器（如通道数和采样率）
        sample_rate = avctx->sample_rate;
        nb_channels = avctx->channels;
        channel_layout = avctx->channel_layout;

        // 打开音频设备并设置硬件参数。
        if ((ret = AudioOpen(is, channel_layout, nb_channels, sample_rate, &is->audio_tgt)) < 0)
            goto fail;

        is->audio_hw_buf_size = ret;
        is->audio_src = is->audio_tgt;
        is->audio_buf_size = 0;
        is->audio_buf_index = 0;


        is->audio_diff_avg_coef = exp(log(0.01) / AUDIO_DIFF_AVG_NB);
        is->audio_diff_avg_count = 0;

        is->audio_diff_threshold = (double)(is->audio_hw_buf_size) / is->audio_tgt.bytes_per_sec;

        is->audio_stream = stream_index;
        is->audio_st = ic->streams[stream_index];

        decoder_init(&is->auddec, avctx, &is->audioq, is->continue_read_thread);
        if ((is->ic->iformat->flags & (AVFMT_NOBINSEARCH | AVFMT_NOGENSEARCH | AVFMT_NO_BYTE_SEEK)) && !is->ic->iformat->read_seek) {
            is->auddec.start_pts = is->audio_st->start_time;
            is->auddec.start_pts_tb = is->audio_st->time_base;
        }
        packet_queue_start(is->auddec.queue);
        is->auddec.decode_thread = std::thread(&VideoCtl::AudioThread, this, is);
        SDL_PauseAudio(0);
        break;
    case AVMEDIA_TYPE_VIDEO: // 视频流

        is->video_stream = stream_index;
        is->video_st = ic->streams[stream_index];

        // 初始化视频解码器
        decoder_init(&is->viddec, avctx, &is->videoq, is->continue_read_thread);

        // 启动视频解码线程
        packet_queue_start(is->viddec.queue);
        is->viddec.decode_thread = std::thread(&VideoCtl::VideoThread, this, is);
        is->queue_attachments_req = 1;
        break;
    case AVMEDIA_TYPE_SUBTITLE: // 字幕流
        is->subtitle_stream = stream_index;
        is->subtitle_st = ic->streams[stream_index];


        // 初始化字幕解码器
        decoder_init(&is->subdec, avctx, &is->subtitleq, is->continue_read_thread);

        // 启动字幕解码线程
        packet_queue_start(is->subdec.queue);
        is->subdec.decode_thread = std::thread(&VideoCtl::SubTitleThread, this, is);
        qDebug() << "字幕解码线程已启动";
        break;
    default:
        break;
    }
    goto out;


fail:
    avcodec_free_context(&avctx);
out:
    av_dict_free(&opts);

    return ret;
}
int decode_interrupt_cb(void* ctx)
{
    VideoState* is = (VideoState*)ctx;
    return is->abort_request;
}

// 判断指定媒体流是否有足够的数据包进行解码和播放的函数
int VideoCtl::StreamPackets(AVStream* st, int stream_id, PacketQueue* queue)
{
    return stream_id < 0 || queue->abort_request || (st->disposition & AV_DISPOSITION_ATTACHED_PIC) || queue->nb_packets > MIN_FRAMES && (!queue->duration || av_q2d(st->time_base) * queue->duration > 1.0);
}

// 判断指定的媒体流是否是实时流
int VideoCtl::RealTime(AVFormatContext* s)
{
    if (!strcmp(s->iformat->name, "rtp")
        || !strcmp(s->iformat->name, "rtsp")
        || !strcmp(s->iformat->name, "sdp")
        )
        return 1;

    if (s->pb && (!strncmp(s->url, "rtp:", 4)
        || !strncmp(s->url, "udp:", 4)
        )
        )
        return 1;
    return 0;
}


// 从多媒体文件或流中读取数据，并将数据包分配到音频、视频和字幕的解码队列
void VideoCtl::ReadThread(VideoState* is)
{
    AVFormatContext* ic = NULL;
    int err, i, ret;
    int st_index[AVMEDIA_TYPE_NB];
    AVPacket pkt1, * pkt = &pkt1;
    int64_t stream_start_time;
    int pkt_in_play_range = 0;
    AVDictionaryEntry* t;
    AVDictionary** opts;
    int orig_nb_streams;


    SDL_mutex* wait_mutex = SDL_CreateMutex();
    int scan_all_pmts_set = 0;
    int64_t pkt_ts;

    const char* wanted_stream_spec[AVMEDIA_TYPE_NB] = { 0 };

    if (!wait_mutex)
    {
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    memset(st_index, -1, sizeof(st_index));
    is->last_video_stream = is->video_stream = -1;
    is->last_audio_stream = is->audio_stream = -1;
    is->last_subtitle_stream = is->subtitle_stream = -1;
    is->eof = 0;

    // 分配 AVFormatContext
    //构建 处理封装格式 结构体
    ic = avformat_alloc_context(); // 分配封装格式上下文 
    if (!ic)
    {
        ret = AVERROR(ENOMEM);
        goto fail;
    }
    ic->interrupt_callback.callback = decode_interrupt_cb;
    ic->interrupt_callback.opaque = is;

    // 打开文件或流
    err = avformat_open_input(&ic, is->filename, is->iformat, nullptr);
    if (err < 0)
    {
        ret = -1;
        goto fail;
    }

    is->ic = ic;


    av_format_inject_global_side_data(ic);

    opts = nullptr;
    orig_nb_streams = ic->nb_streams;


    //读取一部分视音频数据并且获得一些相关的信息
    // 提取媒体流信息（如编解码器参数、时基等

    err = avformat_find_stream_info(ic, opts);
    if (err < 0)
    {
        ret = -1;
        goto fail;
    }

    if (ic->pb)
        ic->pb->eof_reached = 0;

    is->max_frame_duration = (ic->iformat->flags & AVFMT_TS_DISCONT) ? 10.0 : 3600.0;

    //  确定是否为实时流
    is->realtime = RealTime(ic);
    emit SigVideoTotalSeconds(ic->duration / 1000000LL);


    for (i = 0; i < ic->nb_streams; i++)
    {
        AVStream* st = ic->streams[i];
        enum AVMediaType type = st->codecpar->codec_type;
        st->discard = AVDISCARD_ALL;
        if (type >= 0 && wanted_stream_spec[type] && st_index[type] == -1)
            if (avformat_match_stream_specifier(ic, st, wanted_stream_spec[type]) > 0)  st_index[type] = i;
    }
    for (i = 0; i < AVMEDIA_TYPE_NB; i++)
    {
        if (wanted_stream_spec[(AVMediaType)i] && st_index[(AVMediaType)i] == -1)
        {
            st_index[(AVMediaType)i] = INT_MAX;
        }
    }


    //获得视频、音频、字幕的流索引
    st_index[AVMEDIA_TYPE_VIDEO] = av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO, st_index[AVMEDIA_TYPE_VIDEO], -1, NULL, 0);
    if (st_index[AVMEDIA_TYPE_VIDEO] >= 0)
    {
        qDebug() << "找到视频流，索引:" << st_index[AVMEDIA_TYPE_VIDEO];
    }
    else
    {
        qDebug() << "未找到视频流";
    }
    st_index[AVMEDIA_TYPE_AUDIO] = av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO, st_index[AVMEDIA_TYPE_AUDIO], st_index[AVMEDIA_TYPE_VIDEO], NULL, 0);

    if (st_index[AVMEDIA_TYPE_AUDIO] >= 0)
    {
        qDebug() << "找到音频流，索引:" << st_index[AVMEDIA_TYPE_AUDIO];
    }
    else
    {
        qDebug() << "未找到音频流";
    }

    st_index[AVMEDIA_TYPE_SUBTITLE] = av_find_best_stream(ic, AVMEDIA_TYPE_SUBTITLE, st_index[AVMEDIA_TYPE_SUBTITLE], (st_index[AVMEDIA_TYPE_AUDIO] >= 0 ? st_index[AVMEDIA_TYPE_AUDIO] : st_index[AVMEDIA_TYPE_VIDEO]), NULL, 0);

    if (st_index[AVMEDIA_TYPE_SUBTITLE] >= 0)
    {
        HasWord = true;
        qDebug() << "找到字幕流，索引:" << st_index[AVMEDIA_TYPE_SUBTITLE];
    }
    else
    {
        HasWord = false;
        qDebug() << "未找到字幕流";
    }

    if (st_index[AVMEDIA_TYPE_VIDEO] >= 0)
    {
        AVStream* st = ic->streams[st_index[AVMEDIA_TYPE_VIDEO]];
        AVCodecParameters* codecpar = st->codecpar;
        AVRational sar = av_guess_sample_aspect_ratio(ic, st, NULL);
    }

    //打开流并初始化解码器
    if (st_index[AVMEDIA_TYPE_AUDIO] >= 0)
    {
        StreamOpen(is, st_index[AVMEDIA_TYPE_AUDIO]);
    }

    //打开视频流
    ret = -1;
    if (st_index[AVMEDIA_TYPE_VIDEO] >= 0)
    {
        ret = StreamOpen(is, st_index[AVMEDIA_TYPE_VIDEO]);
    }
    else
    {
        is->no_video = true;
        is->force_refresh = 1;
        qDebug() << "未找到视频流, 将使用本地图片作为假视频...";
    }

    //打开字幕流
    if (st_index[AVMEDIA_TYPE_SUBTITLE] >= 0)
    {
        qDebug() << "字幕流打开" << '\n';
        StreamOpen(is, st_index[AVMEDIA_TYPE_SUBTITLE]);
    }

    if (is->video_stream < 0 && is->audio_stream < 0)
    {
        ret = -1;
        qDebug() << "ERROR THERE" << '\n';
        goto fail;
    }

    if (infinite_buffer < 0 && is->realtime) infinite_buffer = 1;

    // 读取数据包循环
    for (;;)
    {
        if (is->abort_request)  break;
        if (is->paused != is->last_paused)  // 如果播放器进入或退出暂停状态，调用 av_read_pause 或 av_read_play 控制流的读取
        {
            is->last_paused = is->paused;
            if (is->paused) is->read_pause_return = av_read_pause(ic);
            else
                av_read_play(ic);
        }

        // 如果用户请求跳转，调用 avformat_seek_file 执行跳转，并清空解码队列。
        if (is->seek_req)
        {
            int64_t seek_target = is->seek_pos;
            int64_t seek_min = is->seek_rel > 0 ? seek_target - is->seek_rel + 2 : INT64_MIN;
            int64_t seek_max = is->seek_rel < 0 ? seek_target - is->seek_rel - 2 : INT64_MAX;

            ret = avformat_seek_file(is->ic, -1, seek_min, seek_target, seek_max, is->seek_flags);
            if (ret < 0)
            {
                av_log(NULL, AV_LOG_ERROR, "%s: error while seeking\n", is->ic->url);
            }
            else
            {
                if (is->audio_stream >= 0)
                {
                    packet_queue_flush(&is->audioq);
                    packet_queue_put(&is->audioq, &flush_pkt);
                }
                if (is->subtitle_stream >= 0)
                {
                    packet_queue_flush(&is->subtitleq);
                    packet_queue_put(&is->subtitleq, &flush_pkt);
                }
                if (is->video_stream >= 0)
                {
                    packet_queue_flush(&is->videoq);
                    packet_queue_put(&is->videoq, &flush_pkt);
                }
                if (is->seek_flags & AVSEEK_FLAG_BYTE)
                {
                    SetClock(&is->extclk, NAN, 0);
                }
                else
                {
                    SetClock(&is->extclk, seek_target / (double)AV_TIME_BASE, 0);
                }
            }
            is->seek_req = 0;
            is->queue_attachments_req = 1;
            is->eof = 0;
            if (is->paused) ToNextFrame(is);
        }
        if (is->queue_attachments_req)
        {
            if (is->video_st && is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC)
            {
                AVPacket copy;
                if ((ret = av_copy_packet(&copy, &is->video_st->attached_pic)) < 0)
                    goto fail;
                packet_queue_put(&is->videoq, &copy);
                packet_queue_put_nullpacket(&is->videoq, is->video_stream);
            }
            is->queue_attachments_req = 0;
        }


        if (infinite_buffer < 1 && (is->audioq.size + is->videoq.size + is->subtitleq.size > MAX_QUEUE_SIZE || (StreamPackets(is->audio_st, is->audio_stream, &is->audioq) && StreamPackets(is->video_st, is->video_stream, &is->videoq) && StreamPackets(is->subtitle_st, is->subtitle_stream, &is->subtitleq))))
        {
            /* wait 10 ms */
            SDL_LockMutex(wait_mutex);
            SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
            SDL_UnlockMutex(wait_mutex);
            continue;
        }

        if (!is->paused && (!is->audio_st || (is->auddec.finished == is->audioq.serial && frame_queue_nb_remaining(&is->sampq) == 0)) && (!is->video_st || (is->viddec.finished == is->videoq.serial && frame_queue_nb_remaining(&is->pictq) == 0)))
        {
            if (isloop==1)
            {
                qDebug() << "结束了，即将重新播放" << '\n';

                av_usleep(1000000); // 等待一会

                StreamSeek(is, 0, 0);
                is->eof = 0;
                is->auddec.finished = 0;
                is->viddec.finished = 0;
                is->subdec.finished = 0;
                continue;
            }
            else if(isloop==2)
            {
                qDebug() << "结束了，即将播放下一个" << '\n';
                av_usleep(1000000);
                emit SigList();
                break;
            }
            else
            {
                qDebug() << "结束了，即将随机播放" << '\n';
                av_usleep(1000000);
                emit SigRandom();
                break;
            }
        }

        // 按帧读取并分发数据包
        ret = av_read_frame(ic, pkt);
        if (ret < 0)
        {
            if ((ret == AVERROR_EOF || avio_feof(ic->pb)) && !is->eof)
            {
                // 根据数据包的流索引（音频、视频或字幕）将其送入对应的队列
                if (is->video_stream >= 0)
                    packet_queue_put_nullpacket(&is->videoq, is->video_stream);
                if (is->audio_stream >= 0)
                    packet_queue_put_nullpacket(&is->audioq, is->audio_stream);
                if (is->subtitle_stream >= 0)
                    packet_queue_put_nullpacket(&is->subtitleq, is->subtitle_stream);
                is->eof = 1;
            }

            if (ic->pb && ic->pb->error) break;
            SDL_LockMutex(wait_mutex);
            SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
            SDL_UnlockMutex(wait_mutex);
            continue;
        }
        else
        {
            is->eof = 0;
        }


        stream_start_time = ic->streams[pkt->stream_index]->start_time;
        pkt_ts = pkt->pts == AV_NOPTS_VALUE ? pkt->dts : pkt->pts;
        pkt_in_play_range = AV_NOPTS_VALUE == AV_NOPTS_VALUE ||
            (pkt_ts - (stream_start_time != AV_NOPTS_VALUE ? stream_start_time : 0)) *
            av_q2d(ic->streams[pkt->stream_index]->time_base) -
            (double)(0 != AV_NOPTS_VALUE ? 0 : 0) / 1000000
            <= ((double)AV_NOPTS_VALUE / 1000000);

        // 根据数据包的流索引（音频、视频或字幕）将其送入对应的队列
        if (pkt->stream_index == is->audio_stream && pkt_in_play_range)
        {
            packet_queue_put(&is->audioq, pkt);
        }
        else if (pkt->stream_index == is->video_stream && pkt_in_play_range && !(is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC))
        {
            packet_queue_put(&is->videoq, pkt);
        }
        else if (pkt->stream_index == is->subtitle_stream && pkt_in_play_range)
        {
            packet_queue_put(&is->subtitleq, pkt);
        }
        else
        {
            av_packet_unref(pkt);
        }
    }
    ret = 0;
fail:
    if (ic && !is->ic)
        avformat_close_input(&ic);

    if (ret != 0)
    {
        SDL_Event event;

        event.type = FF_QUIT_EVENT;
        event.user.data1 = is;
        SDL_PushEvent(&event);
    }
    SDL_DestroyMutex(wait_mutex);
    return;
}

// 初始化播放器的关键状态数据，并启动读取线程来从文件或流中读取音视频数据
VideoState* VideoCtl::StreamOpen(const char* filename)
{
    VideoState* is;
    //构造视频状态类
    //分配内存并初始化存储播放器状态的结构体 VideoState
    is = (VideoState*)av_mallocz(sizeof(VideoState));
    if (!is)
        return NULL;
    //视频文件名
    is->last_video_stream = is->video_stream = -1;
    is->last_audio_stream = is->audio_stream = -1;
    is->last_subtitle_stream = is->subtitle_stream = -1;
    is->filename = av_strdup(filename);
    if (!is->filename)
        goto fail;
    //指定输入格式
    is->ytop = 0;
    is->xleft = 0;

    //为音频、视频和字幕分别初始化帧队列和数据包队列

    //初始化视频帧队列
    if (frame_queue_init(&is->pictq, &is->videoq, VIDEO_PICTURE_QUEUE_SIZE, 1) < 0)
        goto fail;
    //初始化字幕帧队列
    if (frame_queue_init(&is->subpq, &is->subtitleq, SUBPICTURE_QUEUE_SIZE, 0) < 0)
        goto fail;
    //初始化音频帧队列
    if (frame_queue_init(&is->sampq, &is->audioq, SAMPLE_QUEUE_SIZE, 1) < 0)
        goto fail;
    //初始化队列中的数据包
    if (packet_queue_init(&is->videoq) < 0 ||
        packet_queue_init(&is->audioq) < 0 ||
        packet_queue_init(&is->subtitleq) < 0)
        goto fail;
    //构建 继续读取线程 信号量
    if (!(is->continue_read_thread = SDL_CreateCond())) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateCond(): %s\n", SDL_GetError());
        goto fail;
    }
    //视频、音频 时钟
    InitClock(&is->vidclk, &is->videoq.serial);
    InitClock(&is->audclk, &is->audioq.serial);
    InitClock(&is->extclk, &is->extclk.serial);
    is->audio_clock_serial = -1;

    is->no_video = false;
    is->default_tex = nullptr;

    //音量
    if (startup_volume < 0)
        av_log(NULL, AV_LOG_WARNING, "-volume=%d < 0, setting to 0\n", startup_volume);
    if (startup_volume > 100)
        av_log(NULL, AV_LOG_WARNING, "-volume=%d > 100, setting to 100\n", startup_volume);
    startup_volume = av_clip(startup_volume, 0, 100);
    startup_volume = av_clip(SDL_MIX_MAXVOLUME * startup_volume / 100, 0, SDL_MIX_MAXVOLUME);
    is->audio_volume = startup_volume;

    emit SigVideoVolume(startup_volume * 1.0 / SDL_MIX_MAXVOLUME);
    emit SigPauseStat(is->paused);

    is->av_sync_type = AV_SYNC_AUDIO_MASTER;
    //构建读取线程
    is->read_tid = std::thread(&VideoCtl::ReadThread, this, is);

    return is;

fail:
    StreamClose(is);
    return NULL;
}
void VideoCtl::ClearSubtitleCache()
{
    std::lock_guard<std::mutex> lock(subtitle_cache_mutex);
    for (auto& pair : subtitle_texture_cache)
    {
        if (pair.second)
        {
            SDL_DestroyTexture(pair.second);
            pair.second = nullptr;
        }
    }
    subtitle_texture_cache.clear();
    qDebug() << "已清理字幕纹理缓存";
}
// 在多媒体文件或流中切换指定类型的流（音频、视频或字幕）,它会根据当前激活的流索引循环切换到同一类型的下一个流，提供流切换功能
void VideoCtl::StreamCycleChannel(VideoState* is, int codec_type)
{
    AVFormatContext* ic = is->ic;
    int start_index, stream_index;
    int old_index;
    AVStream* st;
    AVProgram* p = NULL;
    int nb_streams = is->ic->nb_streams;

    // 初始化流信息
    // 找到当前激活的流索引以及上一次切换的流索引
    if (codec_type == AVMEDIA_TYPE_VIDEO)
    {
        start_index = is->last_video_stream;
        old_index = is->video_stream;
    }
    else if (codec_type == AVMEDIA_TYPE_AUDIO)
    {
        start_index = is->last_audio_stream;
        old_index = is->audio_stream;
    }
    else
    {
        start_index = is->last_subtitle_stream;
        old_index = is->subtitle_stream;
    }
    stream_index = start_index;

    // 处理属于同一节目流的流
    // 如果当前处理的不是视频流且有激活的视频流，尝试找到与该视频流属于同一节目的流
    if (codec_type != AVMEDIA_TYPE_VIDEO && is->video_stream != -1)
    {
        p = av_find_program_from_stream(ic, NULL, is->video_stream);
        if (p)
        {
            nb_streams = p->nb_stream_indexes;
            for (start_index = 0; start_index < nb_streams; start_index++)
                if (p->stream_index[start_index] == stream_index)
                    break;
            if (start_index == nb_streams)
                start_index = -1;
            stream_index = start_index;
        }
    }
    // 循环查找目标流
    for (;;)
    {
        if (++stream_index >= nb_streams)
        {
            if (codec_type == AVMEDIA_TYPE_SUBTITLE)
            {
                stream_index = -1;
                is->last_subtitle_stream = -1;
                goto end;
            }
            if (start_index == -1)  
                return;
            stream_index = 0;
        }

        if (stream_index == start_index)
            return;

        st = is->ic->streams[p ? p->stream_index[stream_index] : stream_index];
        if (st->codecpar->codec_type == codec_type)
        {
            switch (codec_type)
            {
            case AVMEDIA_TYPE_AUDIO: // 对音频流，进一步检查采样率和通道数是否有效
                if (st->codecpar->sample_rate != 0 &&
                    st->codecpar->channels != 0)
                    goto end;
                break;
            case AVMEDIA_TYPE_VIDEO:
            case AVMEDIA_TYPE_SUBTITLE:
                goto end;
            default:
                break;
            }
        }
    }

    //切换流
end:
    if (p && stream_index != -1)
        stream_index = p->stream_index[stream_index];
    av_log(NULL, AV_LOG_INFO, "Switch %s stream from #%d to #%d\n",
        av_get_media_type_string((AVMediaType)codec_type),
        old_index,
        stream_index);

    //关闭当前激活的流
    StreamComponentClose(is, old_index);

    if (codec_type == AVMEDIA_TYPE_SUBTITLE)
    {
        packet_queue_flush(&is->subtitleq);
       // packet_queue_flush(&is->subpq);
        is->current_subtitle.text.clear();
        is->current_subtitle.end_time = 0.0;
        ClearSubtitleCache(); // 清理字幕缓存
        qDebug() << "已切换字幕流，并清理字幕缓存";
    }

    StreamOpen(is, stream_index);
}

// 负责控制视频的刷新逻辑
void VideoCtl::RefreshLoop(VideoState* is, SDL_Event* event)
{
    double remaining_time = 0.0;
    SDL_PumpEvents();
    while (!SDL_PeepEvents(event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT) && m_bPlayLoop)
    {
        if (remaining_time > 0.0)  av_usleep((int64_t)(remaining_time * 1000000.0));
        remaining_time = REFRESH_RATE;
        if (!is->paused || is->force_refresh) VideoRefresh(is, &remaining_time);
        SDL_PumpEvents();
    }
}

//播放控制循环
// 监听用户事件并响应，包括播放控制、流切换、窗口调整等操作
void VideoCtl::LoopThread(VideoState* cur_stream)
{
    SDL_Event event;
    double incr, pos, frac;

    m_bPlayLoop = true;

    while (m_bPlayLoop)
    {
        double x;
        RefreshLoop(cur_stream, &event);
        switch (event.type)
        {
        case SDL_KEYDOWN:
            switch (event.key.keysym.sym)
            {
            case SDLK_s:
                ToNextFrame(cur_stream);
                break;
            case SDLK_a:
                StreamCycleChannel(cur_stream, AVMEDIA_TYPE_AUDIO);
                break;
            case SDLK_v:
                StreamCycleChannel(cur_stream, AVMEDIA_TYPE_VIDEO);
                break;
            case SDLK_c:
                StreamCycleChannel(cur_stream, AVMEDIA_TYPE_VIDEO);
                StreamCycleChannel(cur_stream, AVMEDIA_TYPE_AUDIO);
                StreamCycleChannel(cur_stream, AVMEDIA_TYPE_SUBTITLE);
                break;
            case SDLK_t:
                StreamCycleChannel(cur_stream, AVMEDIA_TYPE_SUBTITLE);
                break;

            default:
                break;
            }
            break;
        case SDL_WINDOWEVENT:

            switch (event.window.event)
            {
            case SDL_WINDOWEVENT_RESIZED:
                screen_width = cur_stream->width = event.window.data1;
                screen_height = cur_stream->height = event.window.data2;
            case SDL_WINDOWEVENT_EXPOSED:
                cur_stream->force_refresh = 1;
            }
            break;
        case SDL_QUIT:
        case FF_QUIT_EVENT:
            DoExit(cur_stream);
            break;
        default:
            break;
        }
    }
    DoExit(m_CurStream);
}

// 用于在播放过程中根据用户提供的百分比值（dPercent）对播放位置进行跳转
void VideoCtl::OnPlaySeek(double dPercent)
{
    if (m_CurStream == nullptr)
    {
        return;
    }
    int64_t ts = dPercent * m_CurStream->ic->duration;
    if (m_CurStream->ic->start_time != AV_NOPTS_VALUE)
        ts += m_CurStream->ic->start_time;
    StreamSeek(m_CurStream, ts, 0);
}

// 用于调整播放的音量大小。通过接收用户提供的百分比值（dPercent），计算对应的音量值并应用到当前音频流
void VideoCtl::OnPlayVolume(double dPercent)
{
    startup_volume = dPercent * SDL_MIX_MAXVOLUME;
    if (m_CurStream == nullptr)
    {
        return;
    }
    qDebug() << "改了音量为" << startup_volume << '\n';
    m_CurStream->audio_volume = startup_volume;
}

// 用于在播放过程中向前跳转一定时间（默认 5 秒）。它通过获取当前播放位置，增加时间后计算新目标位置，并调用 StreamSeek 实现跳转
void VideoCtl::OnSeekForward()
{
    if (m_CurStream == nullptr)
    {
        return;
    }
    double incr = 5.0;
    double pos = GetMasterClock(m_CurStream);
    if (std::isnan(pos))
        pos = (double)m_CurStream->seek_pos / AV_TIME_BASE;
    pos += incr;
    if (m_CurStream->ic->start_time != AV_NOPTS_VALUE && pos < m_CurStream->ic->start_time / (double)AV_TIME_BASE)
        pos = m_CurStream->ic->start_time / (double)AV_TIME_BASE;
    StreamSeek(m_CurStream, (int64_t)(pos * AV_TIME_BASE), (int64_t)(incr * AV_TIME_BASE));
}

// 回跳 5秒
void VideoCtl::OnSeekBack()
{
    if (m_CurStream == nullptr)
    {
        return;
    }
    double incr = -5.0;
    double pos = GetMasterClock(m_CurStream);
    if (std::isnan(pos))
        pos = (double)m_CurStream->seek_pos / AV_TIME_BASE;
    pos += incr;
    if (m_CurStream->ic->start_time != AV_NOPTS_VALUE && pos < m_CurStream->ic->start_time / (double)AV_TIME_BASE)
        pos = m_CurStream->ic->start_time / (double)AV_TIME_BASE;
    StreamSeek(m_CurStream, (int64_t)(pos * AV_TIME_BASE), (int64_t)(incr * AV_TIME_BASE));
}


// 更新音量
void VideoCtl::UpdateVolume(int sign, double step)
{
    if (m_CurStream == nullptr)
    {
        return;
    }
    double volume_level = m_CurStream->audio_volume ? (20 * log(m_CurStream->audio_volume / (double)SDL_MIX_MAXVOLUME) / log(10)) : -1000.0;
    int new_volume = lrint(SDL_MIX_MAXVOLUME * pow(10.0, (volume_level + sign * step) / 20.0));
    m_CurStream->audio_volume = av_clip(m_CurStream->audio_volume == new_volume ? (m_CurStream->audio_volume + sign) : new_volume, 0, SDL_MIX_MAXVOLUME);

    emit SigVideoVolume(m_CurStream->audio_volume * 1.0 / SDL_MIX_MAXVOLUME);
}

// 将解码后的帧渲染到屏幕上
void VideoCtl::VideoDisplay(VideoState* is)
{
    //qDebug() << "调用了VideoDisplay" << '\n';
    if (!window) VideoOpen(is);
    if (renderer)
    {
        //恰好显示控件大小在变化，则不刷新显示
        if (show_rect.tryLock())
        {
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            VideoImageDisplay(is);
            if (isRecording)
            {
                std::string recordingText = "正在录屏中";
                int margin_x = 5;
                int margin_y = 5;
                int text_x = margin_x;
                int text_y = is->height - subtitle_font_size - margin_y;
                RenderText(recordingText, text_x, text_y,2);
            }
            if (!currentSubtitle.isEmpty())
            {
                std::string subtitleText = currentSubtitle.toStdString();
                int margin_x = 5;
                int margin_y = 5;
                int text_x = margin_x;
                int text_y = is->height - subtitle_font_size - margin_y;

                RenderText(subtitleText, text_x, text_y,2);
            }
            SDL_RenderPresent(renderer);
            show_rect.unlock();
        }
    }
}

// 初始化 SDL 窗口和渲染器
// 创建或调整窗口和渲染器，确保播放器可以正确显示视频内容
int VideoCtl::VideoOpen(VideoState* is)
{
    int w, h;

    w = screen_width;
    h = screen_height;

    if (!window)
    {
        int flags = SDL_WINDOW_SHOWN;
        flags |= SDL_WINDOW_RESIZABLE;

        window = SDL_CreateWindowFrom((void*)play_wid);
        
        SDL_GetWindowSize(window, &w, &h);//初始宽高设置为显示控件宽高
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
        if (window)
        {
            SDL_RendererInfo info;
            if (!renderer)
                renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
            if (!renderer)
            {
                av_log(NULL, AV_LOG_WARNING, "Failed to initialize a hardware accelerated renderer: %s\n", SDL_GetError());
                renderer = SDL_CreateRenderer(window, -1, 0);
            }
            if (renderer)
            {
                if (!SDL_GetRendererInfo(renderer, &info))
                    av_log(NULL, AV_LOG_VERBOSE, "Initialized %s renderer.\n", info.name);
                if (SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND) < 0)
                {
                    qDebug() << "无法设置渲染器混合模式:" << SDL_GetError();
                }
                else qDebug() << "渲染器设置成功" << '\n';
            }
        }
        if (is->no_video)
        {
            QString imagePath = QDir::currentPath() + "/res/image.png"; // 确认路径正确
            SDL_Surface* pSurface = IMG_Load(imagePath.toStdString().c_str());
            if (!pSurface)
            {
                qDebug() << "加载假视频图片失败:" << imagePath << ", 错误：" << IMG_GetError();
            }
            else
            {
                is->default_tex = SDL_CreateTextureFromSurface(renderer, pSurface);
                SDL_FreeSurface(pSurface);
                if (!is->default_tex)
                {
                    qDebug() << "创建假视频纹理失败:" << SDL_GetError();
                }
                else
                {
                    qDebug() << "假视频图片加载成功，已创建纹理。";
                }
            }
        }
    }
    else
    {
        SDL_SetWindowSize(window, w, h);
    }

    if (!window || !renderer)
    {
        av_log(NULL, AV_LOG_FATAL, "SDL: could not set video mode - exiting\n");
        DoExit(is);
    }

    is->width = w;
    is->height = h;



    return 0;
}

// 清理播放器资源并结束播放流程
void VideoCtl::DoExit(VideoState*& is)
{
    if (is)
    {

        StreamClose(is);
        is = nullptr;
    }
    if (renderer)
    {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }

    if (window)
    {
        window = nullptr;
    }

    emit SigStopFinished();
}

void VideoCtl::OnAddVolume()
{
    if (m_CurStream == nullptr)
    {
        return;
    }
    UpdateVolume(1, SDL_VOLUME_STEP);
}

void VideoCtl::OnSubVolume()
{
    if (m_CurStream == nullptr)
    {
        return;
    }
    UpdateVolume(-1, SDL_VOLUME_STEP);
}

void VideoCtl::OnPause()
{
    if (m_CurStream == nullptr)
    {
        return;
    }
    TogglePause(m_CurStream);
    emit SigPauseStat(m_CurStream->paused);
}

void VideoCtl::OnStop()
{
    qDebug() << "调用了OnStop()" << '\n';
    m_bPlayLoop = false;
}
int VideoCtl::IsLooping()
{
    return isloop;
}
void VideoCtl::OnSetWordPlay(bool flag)
{
    wordPlay = flag;
}
bool VideoCtl::GetHasWord()
{
    return HasWord;
}
VideoCtl::VideoCtl(QObject* parent) :
    QObject(parent),
    m_bInited(false),
    m_CurStream(nullptr),
    m_bPlayLoop(false),
    screen_width(0),
    screen_height(0),
    startup_volume(30),
    renderer(nullptr),
    window(nullptr),
    m_nFrameW(0),
    ffmpegProcess(new QProcess(this)),
    m_nFrameH(0),
    speed(1),
    ischange(0),
    audio_speed_convert(NULL),
    wordPlay(false),
    HasWord(true),
    OnloadWord(new QProcess(this)),
    captureProcess(new QProcess(this)),
    currentSubtitle(""),
    subtitleTimer(new QTimer(this)),
    isRecording(false),
    recordStartTime(0),
    recordEndTime(0),
    isloop(1),
    m_targetWidth(0),
    m_targetHeight(0)
{
    avdevice_register_all();
    //网络格式初始化
    avformat_network_init();

    resmap["360p"] = 1000;
    resmap["480p"] = 1500;
    resmap["720p"] = 3000;
    resmap["1080p"] = 8000;
    resmap["1080p_60"] = 16000;

    connect(ffmpegProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        this, &VideoCtl::onFfmpegFinished);

    connect(OnloadWord, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &VideoCtl::onWordFinished);

    audio_speed_convert = sonicCreateStream(get_target_frequency(), get_target_channels());
    if (audio_speed_convert)
    {
        sonicSetSpeed(audio_speed_convert, speed);
        sonicSetPitch(audio_speed_convert, 1.0);
        sonicSetRate(audio_speed_convert, 1.0);
    }
    else {
        qDebug() << "Failed to create sonic stream during initialization.";
    }

    if (IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG) == 0) {
        qDebug() << "SDL2_image 初始化失败：" << IMG_GetError();
    }

    connect(captureProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        this, &VideoCtl::OnCaptureFinished);


    connect(subtitleTimer, &QTimer::timeout, this, &VideoCtl::ClearSubtitle);
}
void VideoCtl::ShowTemporarySubtitle(const QString& text, int duration_ms)
{
    currentSubtitle = text;
    subtitleTimer->start(duration_ms);
}

void VideoCtl::ClearSubtitle()
{
    currentSubtitle.clear();
}
bool VideoCtl::Init()
{
    if (m_bInited == true)
    {
        return true;
    }

    if (ConnectSignalSlots() == false)
    {
        return false;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
    {
        av_log(NULL, AV_LOG_FATAL, "Could not initialize SDL - %s\n", SDL_GetError());
        av_log(NULL, AV_LOG_FATAL, "(Did you set the DISPLAY variable?)\n");
        return false;
    }
    SDL_EventState(SDL_SYSWMEVENT, SDL_IGNORE);
    SDL_EventState(SDL_USEREVENT, SDL_IGNORE);

    if (TTF_Init() == -1)
    {
        qDebug() << "无法初始化 SDL_ttf: " << TTF_GetError();
        return false;
    }

    subtitle_font = TTF_OpenFont(subtitle_font_path.c_str(), subtitle_font_size);
    if (!subtitle_font)
    {
        qDebug() << "无法加载字体：" << QString::fromStdString(subtitle_font_path) << ", 错误：" << TTF_GetError();
        return false;
    }
    else
    {
        qDebug() << "字体加载成功：" << QString::fromStdString(subtitle_font_path);
    }

    m_bInited = true;

    return true;
}

bool VideoCtl::ConnectSignalSlots()
{
    connect(this, &VideoCtl::SigStop, &VideoCtl::OnStop);
    return true;
}


VideoCtl* VideoCtl::m_pInstance = new VideoCtl();

VideoCtl* VideoCtl::GetInstance()
{
    if (false == m_pInstance->Init())
    {
        return nullptr;
    }
    return m_pInstance;
}

VideoCtl::~VideoCtl()
{
    avformat_network_deinit();

    SDL_Quit();

}
bool VideoCtl::ReInit()
{
    if (m_CurStream)
    {
        DoExit(m_CurStream); 
        m_CurStream = nullptr;
    }
    SDL_Quit();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        qDebug() << "Failed to re-init SDL:" << SDL_GetError();
        return false;
    }
    return true;
}
bool VideoCtl::StartPlay(QString strFileName,QLabel* label, double start)
{
    m_bPlayLoop = false;
    if (m_tPlayLoopThread.joinable())
    {
        m_tPlayLoopThread.join();
    }
    if (start == (double)0)
    {
        emit SigStartPlay(strFileName);//正式播放，发送给标题栏
        Filename = strFileName;

        widlay = label->winId();
        emit SigClear();

    }
    else emit SigStartPlay(Filename);
    
    play_wid = label->winId();

    VideoState* is;

    char file_name[1024];
    memset(file_name, 0, 1024);
    sprintf(file_name, "%s", /*strFileName.toLocal8Bit().data()*/strFileName.toStdString().c_str());
    //打开流
    is = StreamOpen(file_name);
    if (!is)
    {
        av_log(NULL, AV_LOG_FATAL, "Failed to initialize VideoState!\n");
        DoExit(m_CurStream);
    }

    m_CurStream = is;

    if (start != (double)0)
    {
        StreamSeek(m_CurStream, (int64_t)((start + 0.2) * AV_TIME_BASE), (int64_t)(0 * AV_TIME_BASE));
    }
    m_tPlayLoopThread = std::thread(&VideoCtl::LoopThread, this, is);


    return true;
}

void VideoCtl::update_sample_display(VideoState* is, short* samples, int samples_size)
{
}

void VideoCtl::ChangeResolution(const std::string& resolution)
{
    if (resolution == "自动")
    {
        qDebug() << "输出文件已存在，直接使用：" << Filename;
        double incr = 0.0;
        double pos = GetMasterClock(m_CurStream);
        if (std::isnan(pos))
            pos = (double)m_CurStream->seek_pos / AV_TIME_BASE;
        pos += incr;
        if (m_CurStream->ic->start_time != AV_NOPTS_VALUE && pos < m_CurStream->ic->start_time / (double)AV_TIME_BASE)
            pos = m_CurStream->ic->start_time / (double)AV_TIME_BASE;
        qDebug() << "转换前的位置" << pos << '\n';
        emit ResolutionChanged(true, Filename, pos);
        return;
    }

    QString outputFilePath = QString("%1output_%2.mp4").arg(Filename).arg(QString::fromStdString(resolution));

    QFile outputFile(outputFilePath);
    if (outputFile.exists())
    {
        qDebug() << "输出文件已存在，直接使用：" << outputFilePath;
        double incr = 0.0;
        double pos = GetMasterClock(m_CurStream);
        if (std::isnan(pos))
            pos = (double)m_CurStream->seek_pos / AV_TIME_BASE;
        pos += incr;
        if (m_CurStream->ic->start_time != AV_NOPTS_VALUE && pos < m_CurStream->ic->start_time / (double)AV_TIME_BASE)
            pos = m_CurStream->ic->start_time / (double)AV_TIME_BASE;
        qDebug() << "转换前的位置" << pos << '\n';
        emit ResolutionChanged(true, outputFilePath, pos+1);
        return;
    }

    std::map<std::string, std::pair<int, int>> resolutionMap = {
    {"360p", {640, 360}},
    {"480p", {854, 480}},
    {"720p", {1280, 720}},
    {"1080p", {1920, 1080}},
    { "1080p_60" ,{1920,1080} }
    };
    int k = resmap[resolution];
    int rate = 30;
    if (resolution == "1080p_60") rate = 60;

    auto target = resolutionMap[resolution];
    int targetWidth = target.first;
    int targetHeight = target.second;

    outFile = outputFilePath;

    int crfValue = 23;
    QString ffmpegCmd = QString(
        "ffmpeg  -hwaccel cuda -hwaccel_output_format cuda -i \"%1\" -c:v h264_nvenc -crf %2 -b:v %3k -preset slow -vf \"scale_cuda=%4:%5\" -r %6 \"%7\"")
        .arg(Filename)
        .arg(crfValue)
        .arg(k)
        .arg(targetWidth)
        .arg(targetHeight)
        .arg(rate)
        .arg(outputFilePath);

    qDebug() << "FFmpeg Command:" << ffmpegCmd;

    if (ffmpegProcess->state() != QProcess::NotRunning)
    {
        qDebug() << "FFmpeg is already running.";
    }
    currentSubtitle = QString::fromStdString("正在切换画面质量至" + resolution);
    ffmpegProcess->start(ffmpegCmd);

    if (!ffmpegProcess->waitForStarted())
    {
        qDebug() << "Failed to start FFmpeg process:" << ffmpegProcess->errorString();
    }
}
double VideoCtl::GetCurrentPosition()
{
    if (m_CurStream)
    {
        return GetMasterClock(m_CurStream);
    }
    return 0.0;
}
void VideoCtl::onFfmpegFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    currentSubtitle = "";
    if (exitStatus == QProcess::NormalExit && exitCode == 0)
    {
        double incr = 0.0;
        double pos = GetMasterClock(m_CurStream);
        if (std::isnan(pos))
            pos = (double)m_CurStream->seek_pos / AV_TIME_BASE;
        pos += incr;
        if (m_CurStream->ic->start_time != AV_NOPTS_VALUE && pos < m_CurStream->ic->start_time / (double)AV_TIME_BASE)
            pos = m_CurStream->ic->start_time / (double)AV_TIME_BASE;
        qDebug() << "FFmpeg process finished successfully!";
        qDebug() << "转换前的位置" << pos << '\n';
        emit ResolutionChanged(true, outFile, pos+1);

    }
    else
    {
        qDebug() << "FFmpeg process failed with exit code" << exitCode;
        qDebug() << "Error:" << ffmpegProcess->errorString();
    }
}
void VideoCtl::onWordFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (exitStatus == QProcess::NormalExit && exitCode == 0)
    {
        emit ResolutionChanged(true,Filename,0);
    }
    else
    {
        qDebug() << "FFmpeg process failed with exit code" << exitCode;
        qDebug() << "Error:" << OnloadWord->errorString();
    }
}
// 录屏
void VideoCtl::LoadingSc()
{
    if (!isRecording)
    {
        // 开始标记截取起点
        isRecording = true;
        recordStartTime = GetMasterClock(m_CurStream);
        if (std::isnan(recordStartTime))
        {
            recordStartTime = (double)m_CurStream->seek_pos / AV_TIME_BASE;
        }
        qDebug() << "截取起点时间：" << recordStartTime << "秒";
        emit SigCaptureStarted(recordStartTime);
    }
    else
    {
        // 标记截取终点并执行截取
        isRecording = false;
        recordEndTime = GetMasterClock(m_CurStream);
        if (std::isnan(recordEndTime))
        {
            recordEndTime = (double)m_CurStream->seek_pos / AV_TIME_BASE;
        }

        // 确保结束时间大于开始时间
        if (recordEndTime <= recordStartTime)
        {
            qDebug() << "结束时间小于或等于起始时间，截取失败。";
            QMessageBox::warning(nullptr, "警告", "结束时间必须大于起始时间。");
            isRecording = false;
            return;
        }

        qDebug() << "截取终点时间：" << recordEndTime << "秒";

        QString currentDir = QDir::currentPath();
        QString captureDir = currentDir + "/Captures";

        QDir dir;
        if (!dir.exists(captureDir))
        {
            if (!dir.mkpath(captureDir))
            {
                qDebug() << "无法创建截取文件夹：" << captureDir;
                QMessageBox::critical(nullptr, "错误", "无法创建截取文件夹。");
                return;
            }
        }

        QString fileName = QString("capture_%1_%2.mp4")
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"))
            .arg(static_cast<int>(recordStartTime));
        captureOutputFile = captureDir + "/" + fileName;

        qDebug() << "起始时间" << recordStartTime << "终止时间:" << recordEndTime << '\n';
        QStringList arguments;
        arguments << "-y"
            << "-hwaccel" << "cuda"
            << "-hwaccel_output_format" << "cuda"
            << "-ss" << QString::number(recordStartTime)
            << "-to" << QString::number(recordEndTime)
            << "-i" << Filename
            << "-b:v"<< "0"
            << "-c:v" << "h264_nvenc"
            << captureOutputFile;

        QString ffmpegPath = "ffmpeg";

        captureProcess->start(ffmpegPath, arguments);

        if (!captureProcess->waitForStarted())
        {
            qDebug() << "无法启动 FFmpeg 进程：" << captureProcess->errorString();
            QMessageBox::critical(nullptr, "错误", "无法启动 FFmpeg 进程。请确保 FFmpeg 已正确安装并在系统 PATH 中。");
            return;
        }

        qDebug() << "开始截取视频片段：" << captureOutputFile;
        emit SigCaptureStopped(captureOutputFile, true);
    }
}
void VideoCtl::OnSetLoop(int flag)
{
    isloop = flag;
    ShowTemporarySubtitle("正在切换播放方式", 1000);
}
void VideoCtl::OnCaptureFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (!isRecording && !captureOutputFile.isEmpty())
    {
        if (exitStatus == QProcess::NormalExit && exitCode == 0)
        {
            qDebug() << "FFmpeg 截取进程成功完成。文件：" << captureOutputFile;
            emit SigCaptureStopped(captureOutputFile, true);
        }
        else
        {
            qDebug() << "FFmpeg 截取进程失败，退出代码：" << exitCode << "，退出状态：" << exitStatus;
            QMessageBox::critical(nullptr, "错误", "视频截取过程中出现错误。");
            emit SigCaptureStopped(captureOutputFile, false);
        }
        captureOutputFile.clear();
    }
}
void VideoCtl::RenderText(const std::string& text, int x, int y,int id)
{
    if (id==2)
    {
        subtitle_font = TTF_OpenFont(subtitle_font_path.c_str(), 30);
    }
    else
    {
        subtitle_font = TTF_OpenFont(subtitle_font_path.c_str(), subtitle_font_size);
    }
    if (!subtitle_font)
    {
        qDebug() << "字幕字体未加载";
        return;
    }

    if (!renderer)
    {
        qDebug() << "渲染器未初始化";
        return;
    }

    if (text.empty())
    {
        qDebug() << "字幕文本为空，跳过渲染。";
        return;
    }

    //qDebug() << "准备渲染字幕文本：" << QString::fromStdString(text);
    SDL_Color yellow = { 255,255,0,255 };
    SDL_Color white = { 255,255,255,255 };
    SDL_Color color;
    if (id == 1) color = white;
    else color = yellow;

    SDL_Texture* text_texture = nullptr;

    {
        std::lock_guard<std::mutex> lock(subtitle_cache_mutex);
        // 检查是否已有缓存的字幕纹理
        auto it = subtitle_texture_cache.find(text);
        if (it != subtitle_texture_cache.end())
        {
            text_texture = it->second;
        }
        else
        {
            // 渲染文本到表面
            SDL_Surface* text_surface = TTF_RenderUTF8_Blended(subtitle_font, text.c_str(), color);
            if (!text_surface)
            {
                qDebug() << "无法渲染字幕文本：" << TTF_GetError();
                return;
            }

            // 创建纹理
            text_texture = SDL_CreateTextureFromSurface(renderer, text_surface);
            if (!text_texture)
            {
                qDebug() << "无法创建字幕纹理：" << SDL_GetError();
                SDL_FreeSurface(text_surface);
                return;
            }

            // 设置纹理混合模式
            SDL_SetTextureBlendMode(text_texture, SDL_BLENDMODE_BLEND);

            // 缓存纹理
            subtitle_texture_cache[text] = text_texture;

            // 释放表面
            SDL_FreeSurface(text_surface);
        }
    }

    // 获取纹理宽高
    int text_width, text_height;
    if (SDL_QueryTexture(text_texture, NULL, NULL, &text_width, &text_height) != 0)
    {
        qDebug() << "无法查询字幕纹理：" << SDL_GetError();
        return;
    }

    // 定义字幕渲染的位置
    SDL_Rect dest_rect;
    dest_rect.x = x;
    dest_rect.y = y;
    dest_rect.w = text_width;
    dest_rect.h = text_height;

    //qDebug() << "渲染位置：" << dest_rect.x << "," << dest_rect.y;
    //qDebug() << "渲染的宽高：" << dest_rect.w << "x" << dest_rect.h;

    // 将字幕纹理渲染到指定位置
    if (SDL_RenderCopy(renderer, text_texture, NULL, &dest_rect) < 0)
    {
        qDebug() << "无法渲染字幕纹理：" << SDL_GetError();
    }
}