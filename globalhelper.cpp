// PCH.H 公共头
// Editor: Liwh 2024/12/21

#include <QFile>
#include <QDebug>
#include <QSettings>
#include <QCoreApplication>
#include <QDir>
#include <QMenu>

#include "globalhelper.h"
#include "ctrlbar.h"

const QString PLAYER_CONFIG_BASEDIR = QDir::tempPath();
const QString PLAYER_CONFIG = "player_config.ini";

GlobalHelper::GlobalHelper()
{

}
void GlobalHelper::CreateSpeedMenu(QPushButton* speedButton)
{
    // 创建菜单
    QMenu* menu = new QMenu(speedButton);
    menu->setStyleSheet("QMenu { background-color: #2C2C2C; color: white; border: 1px solid #5C5C5C; }"
        "QMenu::item { padding: 5px 20px; }"
        "QMenu::item:selected { background-color: #4C9AFF; }");

    // 添加倍速选项
    QAction* slowSpeed = menu->addAction("0.5x");
    QAction* noSlowSpeed = menu->addAction("0.75x");
    QAction* normalSpeed = menu->addAction("1.0x");
    QAction* nofastSpeed = menu->addAction("1.25x");
    QAction* fastSpeed = menu->addAction("1.5x");
    QAction* fasterSpeed = menu->addAction("2.0x");

    QObject::connect(normalSpeed, &QAction::triggered, [=]() {
        speedButton->setText("倍速");
        });
    QObject::connect(noSlowSpeed, &QAction::triggered, [=]() {
        speedButton->setText("0.75x");
        });
    QObject::connect(nofastSpeed, &QAction::triggered, [=]() {
        speedButton->setText("1.25x");
        });

    QObject::connect(fastSpeed, &QAction::triggered, [=]() {
        speedButton->setText("1.5x");
        });

    QObject::connect(fasterSpeed, &QAction::triggered, [=]() {
        speedButton->setText("2.0x");
        });
    QObject::connect(slowSpeed, &QAction::triggered, [=]() {
        speedButton->setText("0.5x");
        });

    speedButton->setMenu(menu);

}

void GlobalHelper::CreateClearMenu(QPushButton* clearButton)
{
    // 创建菜单
    QMenu* menu = new QMenu(clearButton);
    menu->setStyleSheet("QMenu { background-color: #2C2C2C; color: white; border: 1px solid #5C5C5C; }"
        "QMenu::item { padding: 5px 20px; }"
        "QMenu::item:selected { background-color: #4C9AFF; }");

    // 添加倍速选项
    QAction* slowSpeed = menu->addAction("360P 流畅");
    QAction* noSlowSpeed = menu->addAction("480P 清晰");
    QAction* normalSpeed = menu->addAction("720P 高清");
    QAction* nofastSpeed = menu->addAction("1080P 高清");
    QAction* fastSpeed = menu->addAction("1080P 60帧");
    QAction* fasterSpeed = menu->addAction("自动");

    QObject::connect(normalSpeed, &QAction::triggered, [=]() {
        clearButton->setText("720P 高清");
        });
    QObject::connect(noSlowSpeed, &QAction::triggered, [=]() {
        clearButton->setText("480P 清晰");
        });
    QObject::connect(nofastSpeed, &QAction::triggered, [=]() {
        clearButton->setText("1080P 高清");
        });

    QObject::connect(fastSpeed, &QAction::triggered, [=]() {
        clearButton->setText("1080P 60帧");
        });

    QObject::connect(fasterSpeed, &QAction::triggered, [=]() {
        clearButton->setText("自动");
        });
    QObject::connect(slowSpeed, &QAction::triggered, [=]() {
        clearButton->setText("360P 流畅");
        });

    clearButton->setMenu(menu);

}
void GlobalHelper::CreateSettingMenu(QPushButton* settingButton)
{
    QMenu* menu = new QMenu(settingButton);
    menu->setStyleSheet("QMenu { background-color: #2C2C2C; color: white; border: 1px solid #5C5C5C; }"
        "QMenu::item { padding: 5px 20px; }"
        "QMenu::item:selected { background-color: #4C9AFF; }");

    QAction* slowSpeed = menu->addAction("循环播放");
    QAction* noSlowSpeed = menu->addAction("列表播放");
    QAction* normalSpeed = menu->addAction("随机播放");

    QObject::connect(slowSpeed, &QAction::triggered, [=]() {
        GlobalHelper::SetIcon(settingButton, 15, QChar(0xf2f9));
        });
    QObject::connect(noSlowSpeed, &QAction::triggered, [=]() {
        GlobalHelper::SetIcon(settingButton, 15, QChar(0xf883));
        });
    QObject::connect(normalSpeed, &QAction::triggered, [=]() {
        GlobalHelper::SetIcon(settingButton, 15,QChar(0xf074));
        });

    settingButton->setMenu(menu);

}
QString GlobalHelper::GetQssStr(QString strQssPath)
{
    QString strQss;
    QFile FileQss(strQssPath);
    if (FileQss.open(QIODevice::ReadOnly))
    {
        strQss = FileQss.readAll();
        FileQss.close();
    }
    else
    {
        qDebug() << "读取样式表失败" << strQssPath;
    }
    return strQss;
}

void GlobalHelper::SetIcon(QPushButton* btn, int iconSize, QChar icon)
{
    QFont font;
    font.setFamily("Font Awesome 6 Pro");
    font.setPointSize(iconSize);

    btn->setFont(font);
    btn->setText(icon);
}

void GlobalHelper::SavePlaylist(QStringList& playList)
{
    QString strPlayerConfigFileName = PLAYER_CONFIG_BASEDIR + QDir::separator() + PLAYER_CONFIG;
    QSettings settings(strPlayerConfigFileName, QSettings::IniFormat);
    settings.beginWriteArray("playlist");
    for (int i = 0; i < playList.size(); ++i)
    {
        settings.setArrayIndex(i);
        settings.setValue("movie", playList.at(i));
    }
    settings.endArray();
}

void GlobalHelper::GetPlaylist(QStringList& playList)
{
    QString strPlayerConfigFileName = PLAYER_CONFIG_BASEDIR + QDir::separator() + PLAYER_CONFIG;
    QSettings settings(strPlayerConfigFileName, QSettings::IniFormat);

    int size = settings.beginReadArray("playlist");
    for (int i = 0; i < size; ++i) 
    {
        settings.setArrayIndex(i);
        playList.append(settings.value("movie").toString());
    }
    settings.endArray();
}

void GlobalHelper::SavePlayVolume(double& nVolume)
{
    QString strPlayerConfigFileName = PLAYER_CONFIG_BASEDIR + QDir::separator() + PLAYER_CONFIG;
    QSettings settings(strPlayerConfigFileName, QSettings::IniFormat);
    settings.setValue("volume/size", nVolume);
}

void GlobalHelper::GetPlayVolume(double& nVolume)
{
    QString strPlayerConfigFileName = PLAYER_CONFIG_BASEDIR + QDir::separator() + PLAYER_CONFIG;
    QSettings settings(strPlayerConfigFileName, QSettings::IniFormat);
    QString str = settings.value("volume/size").toString();
    nVolume = settings.value("volume/size", nVolume).toDouble();
}

