// 标题界面：显示名称和视频名
// Editor: Liwh

#include <QPainter>
#include <QFileInfo>
#include <QFontMetrics>
#include <QMessageBox>
#include <QFileDialog>
#include <QPixmap>
#include <QPainter>
#include <QIcon>
#include <QInputDialog>
#include <QMessageBox>
#include <QDialog>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QInputDialog>
#include <QApplication>
#include <QDir>
#include <QMovie>
#include <QLabel>
#include <QTimer>

#include <qjsonarray.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QEventLoop>
#include <QDebug>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QtSvgWidgets/QSvgWidget>
#include <QtSvg/QSvgRenderer>


#include "title.h"
#include "ui_title.h"
#include "globalhelper.h"
#include <QProgressDialog>

#pragma execution_character_set("utf-8")


void parseAvailableQualities(const QJsonObject& data)
{
    if (data.contains("accept_quality") && data.contains("accept_description"))
    {
        QJsonArray acceptQuality = data["accept_quality"].toArray();
        QJsonArray acceptDescription = data["accept_description"].toArray();

        qDebug() << "Available Qualities:";
        for (int i = 0; i < acceptQuality.size(); ++i) {
            qDebug() << acceptQuality[i].toInt() << "-" << acceptDescription[i].toString();
        }
    }
}

QString extractBV(const QString& url)
{
    QRegularExpression bvRegex("BV[\\w\\d]{10}");
    QRegularExpressionMatch match = bvRegex.match(url);
    if (match.hasMatch()) {
        return match.captured(0);
    }
    return QString();
}

QString getVideoCID(const QString& bv)
{
    QString apiUrl = QString("https://api.bilibili.com/x/web-interface/view?bvid=%1").arg(bv);
    QUrl url(apiUrl);

    QNetworkAccessManager manager;
    QNetworkRequest request(url);

    // 添加必要的头信息
    request.setRawHeader("Referer", "https://www.bilibili.com/");
    request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/89.0.4389.82 Safari/537.36");
    request.setRawHeader("Cookie", "buvid4=A9ABD297-5E42-890F-CB6F-0B21BE41617071464-023021210-GRYOe0wAEu5lob3K%2BtPmgw%3D%3D; buvid_fp_plain=undefined; DedeUserID=405828899; DedeUserID__ckMd5=b1d4a52913d5ed67; is-2022-channel=1; enable_web_push=DISABLE; header_theme_version=CLOSE; buvid3=2DCAED86-E007-E95C-F23D-3764EF27E56493798infoc; b_nut=1707703593; _uuid=2106797D4-276B-2652-B51E-35F5ACD3416B29018infoc; hit-dyn-v2=1; FEED_LIVE_VERSION=V_WATCHLATER_PIP_WINDOW2; rpdid=|(m)~uJ|Jlm0J'u~u|~JlR)k; LIVE_BUVID=AUTO7017112857089501; b-user-id=22eeb5df-91d4-8850-1b5e-6ccfc35d49c1; buvid_fp=fac1143bfd37ac4898dbaaf242b17e91; CURRENT_QUALITY=116; fingerprint=9172e218046df6d5df4a9a6fc7962abd; bili_ticket=eyJhbGciOiJIUzI1NiIsImtpZCI6InMwMyIsInR5cCI6IkpXVCJ9.eyJleHAiOjE3MzUzODQ1NzUsImlhdCI6MTczNTEyNTMxNSwicGx0IjotMX0.ePRbQrlU91bUwfuspT4ZtZo36EvTehSwz4COrTs2EcI; bili_ticket_expires=1735384515; blackside_state=0; CURRENT_BLACKGAP=0; PVID=2; home_feed_column=4; bmg_af_switch=1; bmg_src_def_domain=i0.hdslb.com; browser_resolution=1245-714; bp_t_offset_405828899=1015303678631870464; b_lsid=7A457B10A_1940458A260; SESSDATA=0864b01a%2C1750792849%2Cc6feb%2Ac2CjDVf6T9W6I4xTwu_7RIAMlzv_09oi-bTtLJcAeuu2PgYfdnVYkfSdKLyw6hkirE4Z0SVjg3SG5mdmZOeC1uU2gxNFJxZi1oYUJfQ21TeUw4TmQ0RVRzRmJiRy0tOGpFeFhyQTV5N0ZIX3BTTFMzcDEweVQyYm1HcEJuVG04bENhVHVrT0xzVGd3IIEC; bili_jct=1162fc29620c29b16a63d1d4c0314a96; bsource=search_baidu; sid=8em2tvpc; CURRENT_FNVAL=4048");


    QNetworkReply* reply = manager.get(request);
    QEventLoop loop;

    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QByteArray response = reply->readAll();

    // 保存 API 响应到文件
    QString filePath = "cid_api_response_debug.json";
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(response);
        file.close();
        qDebug() << "Saved API Response to:" << filePath;
    }

    reply->deleteLater();

    QJsonDocument jsonDoc = QJsonDocument::fromJson(response);
    if (!jsonDoc.isObject()) {
        qDebug() << "Failed to parse JSON. Response might be invalid!";
        return QString();
    }

    QJsonObject obj = jsonDoc.object();
    if (obj["code"].toInt() == 0) {
        QJsonObject data = obj["data"].toObject();
        qDebug() << "Data Object:" << data;

        if (data.contains("pages")) {
            QJsonArray pages = data["pages"].toArray();
            qDebug() << "Pages Array:" << pages;

            if (!pages.isEmpty()) {
                QJsonObject firstPage = pages[0].toObject();
                qDebug() << "First Page Object:" << firstPage;

                if (firstPage.contains("cid")) {
                    QVariant cidVariant = firstPage["cid"].toVariant();
                    QString cidStr = cidVariant.toString();
                    qDebug() << "Extracted CID from QVariant as QString:" << cidStr;
                    return cidStr;
                }
            }
        }
    }

    return QString();
}

QString getVideoStreamURL(const QString& bv, const QString& cid)
{
    QString apiUrl = QString("https://api.bilibili.com/x/player/playurl?cid=%1&bvid=%2&qn=%3")
        .arg(cid)
        .arg(bv)
        .arg(116);

    QUrl url(apiUrl);
    QNetworkAccessManager manager;
    QNetworkRequest request(url);

    // 添加头信息
    request.setRawHeader("Referer", "https://www.bilibili.com/video/");
    request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/89.0.4389.82 Safari/537.36");
    request.setRawHeader("Cookie", "buvid4=A9ABD297-5E42-890F-CB6F-0B21BE41617071464-023021210-GRYOe0wAEu5lob3K%2BtPmgw%3D%3D; buvid_fp_plain=undefined; DedeUserID=405828899; DedeUserID__ckMd5=b1d4a52913d5ed67; is-2022-channel=1; enable_web_push=DISABLE; header_theme_version=CLOSE; buvid3=2DCAED86-E007-E95C-F23D-3764EF27E56493798infoc; b_nut=1707703593; _uuid=2106797D4-276B-2652-B51E-35F5ACD3416B29018infoc; hit-dyn-v2=1; FEED_LIVE_VERSION=V_WATCHLATER_PIP_WINDOW2; rpdid=|(m)~uJ|Jlm0J'u~u|~JlR)k; LIVE_BUVID=AUTO7017112857089501; b-user-id=22eeb5df-91d4-8850-1b5e-6ccfc35d49c1; buvid_fp=fac1143bfd37ac4898dbaaf242b17e91; CURRENT_QUALITY=116; fingerprint=9172e218046df6d5df4a9a6fc7962abd; bili_ticket=eyJhbGciOiJIUzI1NiIsImtpZCI6InMwMyIsInR5cCI6IkpXVCJ9.eyJleHAiOjE3MzUzODQ1NzUsImlhdCI6MTczNTEyNTMxNSwicGx0IjotMX0.ePRbQrlU91bUwfuspT4ZtZo36EvTehSwz4COrTs2EcI; bili_ticket_expires=1735384515; blackside_state=0; CURRENT_BLACKGAP=0; PVID=2; home_feed_column=4; bmg_af_switch=1; bmg_src_def_domain=i0.hdslb.com; browser_resolution=1245-714; bp_t_offset_405828899=1015303678631870464; b_lsid=7A457B10A_1940458A260; SESSDATA=0864b01a%2C1750792849%2Cc6feb%2Ac2CjDVf6T9W6I4xTwu_7RIAMlzv_09oi-bTtLJcAeuu2PgYfdnVYkfSdKLyw6hkirE4Z0SVjg3SG5mdmZOeC1uU2gxNFJxZi1oYUJfQ21TeUw4TmQ0RVRzRmJiRy0tOGpFeFhyQTV5N0ZIX3BTTFMzcDEweVQyYm1HcEJuVG04bENhVHVrT0xzVGd3IIEC; bili_jct=1162fc29620c29b16a63d1d4c0314a96; bsource=search_baidu; sid=8em2tvpc; CURRENT_FNVAL=4048");

    QNetworkReply* reply = manager.get(request);
    QEventLoop loop;

    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Network error:" << reply->errorString();
        reply->deleteLater();
        return QString();
    }

    QByteArray response = reply->readAll();
    qDebug() << "API Response:" << response; // 输出 JSON 数据

    reply->deleteLater();

    QJsonDocument jsonDoc = QJsonDocument::fromJson(response);
    if (jsonDoc.isObject()) {
        QJsonObject obj = jsonDoc.object();
        if (obj["code"].toInt() == 0) {
            QJsonObject data = obj["data"].toObject();
            QJsonArray durlArray = data["durl"].toArray();
            parseAvailableQualities(data);
            if (!durlArray.isEmpty()) {
                QJsonObject firstDurl = durlArray[0].toObject();
                return firstDurl["url"].toString(); // 返回视频流 URL
            }
        }
        else {
            qDebug() << "API Error:" << obj["message"].toString();
        }
    }

    return QString();
}

QString downloadVideo(const QString& url, const QString& outputFile, const QString& bv)
{
    QUrl qurl(url);
    QNetworkAccessManager manager;
    QNetworkRequest request(qurl);

    QString refererUrl = QString("https://www.bilibili.com/video/%1").arg(bv);
    request.setRawHeader("Referer", refererUrl.toUtf8());
    request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/89.0.4389.82 Safari/537.36");
    request.setRawHeader("Cookie", "buvid4=A9ABD297-5E42-890F-CB6F-0B21BE41617071464-023021210-GRYOe0wAEu5lob3K%2BtPmgw%3D%3D; buvid_fp_plain=undefined; DedeUserID=405828899; DedeUserID__ckMd5=b1d4a52913d5ed67; is-2022-channel=1; enable_web_push=DISABLE; header_theme_version=CLOSE; buvid3=2DCAED86-E007-E95C-F23D-3764EF27E56493798infoc; b_nut=1707703593; _uuid=2106797D4-276B-2652-B51E-35F5ACD3416B29018infoc; hit-dyn-v2=1; FEED_LIVE_VERSION=V_WATCHLATER_PIP_WINDOW2; rpdid=|(m)~uJ|Jlm0J'u~u|~JlR)k; LIVE_BUVID=AUTO7017112857089501; b-user-id=22eeb5df-91d4-8850-1b5e-6ccfc35d49c1; buvid_fp=fac1143bfd37ac4898dbaaf242b17e91; CURRENT_QUALITY=116; fingerprint=9172e218046df6d5df4a9a6fc7962abd; bili_ticket=eyJhbGciOiJIUzI1NiIsImtpZCI6InMwMyIsInR5cCI6IkpXVCJ9.eyJleHAiOjE3MzUzODQ1NzUsImlhdCI6MTczNTEyNTMxNSwicGx0IjotMX0.ePRbQrlU91bUwfuspT4ZtZo36EvTehSwz4COrTs2EcI; bili_ticket_expires=1735384515; blackside_state=0; CURRENT_BLACKGAP=0; PVID=2; home_feed_column=4; bmg_af_switch=1; bmg_src_def_domain=i0.hdslb.com; browser_resolution=1245-714; bp_t_offset_405828899=1015303678631870464; b_lsid=7A457B10A_1940458A260; SESSDATA=0864b01a%2C1750792849%2Cc6feb%2Ac2CjDVf6T9W6I4xTwu_7RIAMlzv_09oi-bTtLJcAeuu2PgYfdnVYkfSdKLyw6hkirE4Z0SVjg3SG5mdmZOeC1uU2gxNFJxZi1oYUJfQ21TeUw4TmQ0RVRzRmJiRy0tOGpFeFhyQTV5N0ZIX3BTTFMzcDEweVQyYm1HcEJuVG04bENhVHVrT0xzVGd3IIEC; bili_jct=1162fc29620c29b16a63d1d4c0314a96; bsource=search_baidu; sid=8em2tvpc; CURRENT_FNVAL=4048");

    QNetworkReply* reply = manager.get(request);
    QEventLoop loop;

    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "下载失败:" << reply->errorString();
        reply->deleteLater();
        return QString();
    }

    QFile file(outputFile);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "无法保存文件:" << outputFile;
        reply->deleteLater();
        return QString();
    }

    file.write(reply->readAll());
    file.close();
    reply->deleteLater();

    return outputFile;
}

QString downloadAndSaveBilibiliVideo(const QString& url)
{
    QString bv = extractBV(url);
    if (bv.isEmpty()) {
        qDebug() << "无法提取 BV 号！";
        return QString();
    }
    qDebug() << "BV:" << bv << '\n';
    QString cid = getVideoCID(bv);
    if (cid.isEmpty()) {
        qDebug() << "无法获取 CID！";
        return QString();
    }
    qDebug() << "CID：" << cid << '\n';
    QString streamURL = getVideoStreamURL(bv, cid);
    if (streamURL.isEmpty()) {
        qDebug() << "无法获取视频流 URL！";
        return QString();
    }

    QString outputFile = bv + ".mp4";
    return downloadVideo(streamURL, outputFile,bv);
}
void Title::OnMenuBtnClicked()
{
    bool ok;

    // 创建输入框
    QInputDialog inputDialog(this);
    inputDialog.setWindowTitle("输入 Bilibili 视频 URL");
    inputDialog.setLabelText("请输入 Bilibili 视频 URL：");
    inputDialog.setFixedSize(400, 200); // 设置窗口大小

    // 设置样式
    inputDialog.setStyleSheet(
        "QInputDialog {"
        "    background-color: #2C2C2C;"        // 背景色
        "    color: #FFFFFF;"                  // 字体颜色
        "    border-radius: 10px;"             // 圆角
        "}"
        "QLabel {"
        "    font-size: 18px;"
        "    color: #FFFFFF;"
        "}"
        "QLineEdit {"
        "    font-size: 16px;"
        "    color: #000000;"
        "    background-color: #FFFFFF;"
        "    border: 1px solid #4C9AFF;"
        "    border-radius: 5px;"
        "    padding: 5px;"
        "}"
        "QPushButton {"
        "    font-size: 16px;"
        "    color: #FFFFFF;"
        "    background-color: #4C9AFF;"
        "    border: none;"
        "    border-radius: 5px;"
        "    padding: 5px 15px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #3A8ED6;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #2E76B4;"
        "}"
    );

    // 居中显示输入框
    QRect screenGeometry = QGuiApplication::primaryScreen()->geometry();
    inputDialog.move(screenGeometry.center() - inputDialog.rect().center());

    // 获取用户输入
    if (inputDialog.exec() != QDialog::Accepted)
    {
        return; // 用户取消或关闭输入框
    }
    QString url = inputDialog.textValue().trimmed(); // 获取用户输入并去掉首尾空格

    // 验证 URL 格式
    if (url.isEmpty() || !url.startsWith("https://www.bilibili.com/video/"))
    {
        QMessageBox::warning(this, "错误", "请输入有效的 Bilibili 视频 URL！");
        return;
    }

    // 下载进度提示
    QProgressDialog progressDialog("正在下载视频，请稍候...", "取消", 0, 100, this);
    progressDialog.setWindowModality(Qt::WindowModal);
    progressDialog.setMinimumDuration(0);
    progressDialog.setValue(0); // 设置初始进度
    progressDialog.setCancelButton(nullptr); // 移除取消按钮
    progressDialog.setStyleSheet(
        "QProgressDialog {"
        "    background-color: #2C2C2C;"
        "    color: #FFFFFF;"
        "    border-radius: 10px;"
        "}"
        "QLabel {"
        "    font-size: 18px;"
        "    color: #FFFFFF;"
        "}"
        "QProgressBar {"
        "    border: 1px solid #4C9AFF;"
        "    border-radius: 5px;"
        "    background: #1A1A1A;"
        "    text-align: center;"
        "    color: #FFFFFF;"
        "}"
        "QProgressBar::chunk {"
        "    background-color: #4C9AFF;"
        "    width: 20px;"
        "}"
    );
    progressDialog.show();

    QTimer timer;
    int progressValue = 0;

    connect(&timer, &QTimer::timeout, [&]()
        {
        if (progressValue < 95) {
            progressValue += 1; // 缓慢增加进度值，最大值设置为 95
            progressDialog.setValue(progressValue);
        }
        });

    timer.start(200);

    QString localFile = downloadAndSaveBilibiliVideo(url);

    if (localFile.isEmpty())
    {
        progressDialog.close();
        QMessageBox::warning(this, "错误", "无法下载视频！");
        return;
    }

    progressDialog.setValue(100);
    progressDialog.close();

    emit SigOpenFile(localFile);

}

Title::Title(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Title),
    ActionGroup(this),
    Menu(this)
{
    ui->setupUi(this);

    connect(ui->CloseBtn, &QPushButton::clicked, this, &Title::SigCloseBtnClicked);
    connect(ui->MinBtn, &QPushButton::clicked, this, &Title::SigMinBtnClicked);
    connect(ui->MaxBtn, &QPushButton::clicked, this, &Title::SigMaxBtnClicked);
    connect(ui->FullScreenBtn, &QPushButton::clicked, this, &Title::SigFullScreenBtnClicked);
    connect(ui->MenuBtn, &QPushButton::clicked, this, &Title::OnMenuBtnClicked);
 
    Menu.addAction("最大化", this, &Title::SigMaxBtnClicked);
    Menu.addAction("最小化", this, &Title::SigMinBtnClicked);
    Menu.addAction("退出", this, &Title::SigCloseBtnClicked);

    QMenu* stMenu = Menu.addMenu("打开");
    stMenu->addAction("打开文件", this, &Title::OpenFile);

    ui->MenuBtn->setToolTip("Bilibili视频爬取");
    ui->MinBtn->setToolTip("最小化");
    ui->MaxBtn->setToolTip("最大化");
    ui->CloseBtn->setToolTip("关闭");
    ui->FullScreenBtn->setToolTip("全屏");
}

Title::~Title()
{
    delete ui;
}

bool Title::Init()
{
    if (InitUi() == false)
    {
        return false;
    }

    return true;
}

bool Title::InitUi()
{
    ui->MovieNameLab->clear();

    setAttribute(Qt::WA_TranslucentBackground);

    setStyleSheet(GlobalHelper::GetQssStr("://res/qss/title.css"));

    GlobalHelper::SetIcon(ui->MaxBtn, 15, QChar(0xf2d0));
    GlobalHelper::SetIcon(ui->MinBtn, 15, QChar(0xf068));
    GlobalHelper::SetIcon(ui->CloseBtn, 15, QChar(0xf00d));
    GlobalHelper::SetIcon(ui->FullScreenBtn, 15, QChar(0xf065));

    //loadSvgFromUrl("http://www.w3.org/2000/svg", ui->FullScreenBtn);


    return true;
}

void Title::OpenFile()
{
    QString strFileName = QFileDialog::getOpenFileName(this, "打开文件", QDir::homePath(), 
        "视频文件(*.mkv *.rmvb *.mp4 *.avi *.flv *.wmv *.3gp *.mp3)");

    emit SigOpenFile(strFileName);
}

void Title::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
}

void Title::mouseDoubleClickEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
        emit SigDoubleClicked();
    }
}

void Title::resizeEvent(QResizeEvent *event)
{

}

void Title::ChangeMovieNameShow()
{
    QFontMetrics font_metrics(ui->MovieNameLab->font());
    QRect rect = font_metrics.boundingRect(MovieName);
    int font_width = rect.width();
    int show_width = ui->MovieNameLab->width();
    if (font_width > show_width)
    {
        QString str = font_metrics.elidedText(MovieName, Qt::ElideRight, ui->MovieNameLab->width());
        ui->MovieNameLab->setText(str);
    }
    else
    {
        ui->MovieNameLab->setText(MovieName);
    }
}

void Title::OnChangeMaxBtnStyle(bool bIfMax)
{
    if (bIfMax)
    {
        GlobalHelper::SetIcon(ui->MaxBtn, 9, QChar(0xf2d2));
        ui->MaxBtn->setToolTip("还原");
    }
    else
    {
        GlobalHelper::SetIcon(ui->MaxBtn, 9, QChar(0xf2d0));
        ui->MaxBtn->setToolTip("最大化");
    }
}

void Title::OnPlay(QString strMovieName)
{
    qDebug() << "Title::OnPlay";
    QFileInfo fileInfo(strMovieName);
    MovieName = fileInfo.fileName();
    ui->MovieNameLab->setText(MovieName);
}

void Title::OnStopFinished()
{
    qDebug() << "Title::OnStopFinished";
    ui->MovieNameLab->clear();
}

