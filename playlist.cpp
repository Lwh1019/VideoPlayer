// 播放列表界面
// Editor: Liwh  2024/12/22
#include <QDebug>
#include <QDir>
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
#include <QProcess>

#include "playlist.h"
#include "ui_playlist.h"
#include "globalhelper.h"
#include <QRandomGenerator>

QString Playlist::GenerateThumbnail(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    QString extension = fileInfo.suffix().toLower();

    // 定义音频和视频扩展名
    const QStringList audioExtensions = { "mp3", "wav", "aac", "flac", "ogg", "m4a", "wma" };
    const QStringList videoExtensions = { "mkv", "rmvb", "mp4", "avi", "flv", "wmv", "mov", "yuv", "3gp" };

    // 如果是音频文件，返回默认封面图像
    if (audioExtensions.contains(extension))
    {
        QString defaultImagePath = "://res//c.jpeg"; // 确保这个路径正确
        return defaultImagePath;
    }

    if (videoExtensions.contains(extension))
    {
        QString thumbnailPath = QDir::tempPath() + "/" + fileInfo.baseName() + "_thumbnail.jpg";

        if (QFile::exists(thumbnailPath))
        {
            return thumbnailPath;
        }

        QString ffmpegCmd = QString("ffmpeg -i \"%1\" -vf \"select='eq(pict_type,I)'\" -frames:v 1 \"%2\"")
            .arg(filePath)
            .arg(thumbnailPath);

        QProcess process;
        process.start(ffmpegCmd);
        process.waitForFinished();

        // 检查是否生成成功
        if (!QFile::exists(thumbnailPath))
        {
            qDebug() << "Failed to generate thumbnail for" << filePath;
            return QString();
        }

        return thumbnailPath;
    }
    return QString();
}

Playlist::Playlist(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Playlist)
{
    ui->setupUi(this);
}

Playlist::~Playlist()
{
    QStringList strListPlayList;
    for (int i = 0; i < ui->List->count(); i++)
    {
        strListPlayList.append(ui->List->item(i)->toolTip());
    }
    GlobalHelper::SavePlaylist(strListPlayList);
    delete ui;
}

bool Playlist::Init()
{
    if (ui->List->Init() == false)
    {
        return false;
    }

    if (InitUi() == false)
    {
        return false;
    }

    if (ConnectSignalSlots() == false)
    {
        return false;
    }

    setAcceptDrops(true);

	return true;
}

bool Playlist::InitUi()
{
    setStyleSheet(GlobalHelper::GetQssStr("://res/qss/playlist.css"));
    ui->List->clear();

    QStringList strListPlaylist;
    GlobalHelper::GetPlaylist(strListPlaylist);

    for (QString strVideoFile : strListPlaylist)
    {
        QFileInfo fileInfo(strVideoFile);
        if (fileInfo.exists())
        {
            QListWidgetItem *pItem = new QListWidgetItem(ui->List);
            pItem->setData(Qt::UserRole, QVariant(fileInfo.filePath()));  // 用户数据
            pItem->setText(QString("%1").arg(fileInfo.fileName()));  // 显示文本
            pItem->setToolTip(fileInfo.filePath());
            QString thumbnailPath = GenerateThumbnail(fileInfo.filePath());
            if (!thumbnailPath.isEmpty())
            {
                QPixmap pixmap;
                if (thumbnailPath.startsWith("://")) // 资源文件路径
                {
                    pixmap.load(thumbnailPath);
                }
                else
                {
                    pixmap.load(thumbnailPath);
                }
                if (!pixmap.isNull())
                {
                    pItem->setIcon(QIcon(pixmap.scaled(100, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation))); // 设置缩略图
                }
                else
                {
                    qDebug() << "Failed to load pixmap from" << thumbnailPath;
                }
            }
            ui->List->addItem(pItem);
        }
    }
    if (strListPlaylist.length() > 0)
    {
        ui->List->setCurrentRow(0);
    }

    ui->List->setIconSize(QSize(75, 75));
    ui->List->setResizeMode(QListWidget::Adjust);
    ui->List->setStyleSheet("QListWidget { background: #222222; border: none; }"
        "QListWidget::item { border: none; }");
    return true;
}

bool Playlist::ConnectSignalSlots()
{
	QList<bool> listRet;
	bool bRet;

    bRet = connect(ui->List, &MediaList::SigAddFile, this, &Playlist::OnAddFile);
    listRet.append(bRet);

	for (bool bReturn : listRet)
	{
		if (bReturn == false)
		{
			return false;
		}
	}

	return true;
}

void Playlist::on_List_itemDoubleClicked(QListWidgetItem *item)
{
	emit SigPlay(item->data(Qt::UserRole).toString());
    CurrentPlayListIndex = ui->List->row(item);
    ui->List->setCurrentRow(CurrentPlayListIndex);
}

bool Playlist::GetPlaylistStatus()
{
    if (this->isHidden())
    {
        return false;
    }

    return true;
}

void Playlist::OnAddFile(QString strFileName)
{
    bool bSupportMovie = strFileName.endsWith(".mkv", Qt::CaseInsensitive) ||
        strFileName.endsWith(".rmvb", Qt::CaseInsensitive) ||
        strFileName.endsWith(".mp4", Qt::CaseInsensitive) ||
        strFileName.endsWith(".avi", Qt::CaseInsensitive) ||
        strFileName.endsWith(".flv", Qt::CaseInsensitive) ||
        strFileName.endsWith(".wmv", Qt::CaseInsensitive) ||
        strFileName.endsWith(".mov", Qt::CaseInsensitive) ||
        strFileName.endsWith(".yuv", Qt::CaseInsensitive) ||
        strFileName.endsWith(".mp3", Qt::CaseInsensitive) ||
        strFileName.endsWith(".wav", Qt::CaseInsensitive) ||
        strFileName.endsWith(".ogg", Qt::CaseInsensitive) ||
        strFileName.endsWith(".3gp", Qt::CaseInsensitive);
    if (!bSupportMovie)
    {
        return;
    }

    QFileInfo fileInfo(strFileName);
	QList<QListWidgetItem *> listItem = ui->List->findItems(fileInfo.fileName(), Qt::MatchExactly);
    QListWidgetItem *pItem = nullptr;
	if (listItem.isEmpty())
	{
        pItem = new QListWidgetItem(ui->List);
        pItem->setData(Qt::UserRole, QVariant(fileInfo.filePath()));  // 用户数据
        pItem->setText(fileInfo.fileName());  // 显示文本
        pItem->setToolTip(fileInfo.filePath());
        QString thumbnailPath = GenerateThumbnail(fileInfo.filePath());
        if (!thumbnailPath.isEmpty())
        {
            QPixmap pixmap(thumbnailPath);
            pItem->setIcon(QIcon(pixmap.scaled(100, 100, Qt::KeepAspectRatio))); // 设置缩略图
        }
        ui->List->addItem(pItem);
	}
    else
    {
        pItem = listItem.at(0);
    }
}

void Playlist::OnAddFileAndPlay(QString strFileName)
{
    bool bSupportMovie = strFileName.endsWith(".mkv", Qt::CaseInsensitive) ||
        strFileName.endsWith(".rmvb", Qt::CaseInsensitive) ||
        strFileName.endsWith(".mp4", Qt::CaseInsensitive) ||
        strFileName.endsWith(".avi", Qt::CaseInsensitive) ||
        strFileName.endsWith(".flv", Qt::CaseInsensitive) ||
        strFileName.endsWith(".wmv", Qt::CaseInsensitive) ||
        strFileName.endsWith(".mov", Qt::CaseInsensitive) ||
        strFileName.endsWith(".yuv", Qt::CaseInsensitive) ||
        strFileName.endsWith(".mp3", Qt::CaseInsensitive) ||
        strFileName.endsWith(".wav", Qt::CaseInsensitive) ||
        strFileName.endsWith(".ogg", Qt::CaseInsensitive) ||
        strFileName.endsWith(".3gp", Qt::CaseInsensitive);
    if (!bSupportMovie)
    {
        return;
    }

    QFileInfo fileInfo(strFileName);
    QList<QListWidgetItem *> listItem = ui->List->findItems(fileInfo.fileName(), Qt::MatchExactly);
    QListWidgetItem *pItem = nullptr;
    if (listItem.isEmpty())
    {
        pItem = new QListWidgetItem(ui->List);
        pItem->setData(Qt::UserRole, QVariant(fileInfo.filePath()));  // 用户数据
        pItem->setText(fileInfo.fileName());  // 显示文本
        pItem->setToolTip(fileInfo.filePath());
        QString thumbnailPath = GenerateThumbnail(fileInfo.filePath());
        if (!thumbnailPath.isEmpty())
        {
            QPixmap pixmap(thumbnailPath);
            pItem->setIcon(QIcon(pixmap.scaled(100, 100, Qt::KeepAspectRatio))); // 设置缩略图
        }
        ui->List->addItem(pItem);
    }
    else
    {
        pItem = listItem.at(0);
    }
    on_List_itemDoubleClicked(pItem);
}

void Playlist::OnBackwardPlay()
{
    if (CurrentPlayListIndex == 0)
    {
        CurrentPlayListIndex = ui->List->count() - 1;
        on_List_itemDoubleClicked(ui->List->item(CurrentPlayListIndex));
        ui->List->setCurrentRow(CurrentPlayListIndex);
    }
    else
    {
        CurrentPlayListIndex--;
        on_List_itemDoubleClicked(ui->List->item(CurrentPlayListIndex));
        ui->List->setCurrentRow(CurrentPlayListIndex);
    }
}

void Playlist::OnForwardPlay()
{
    if (CurrentPlayListIndex == ui->List->count() - 1)
    {
        CurrentPlayListIndex = 0;
        on_List_itemDoubleClicked(ui->List->item(CurrentPlayListIndex));
        ui->List->setCurrentRow(CurrentPlayListIndex);
    }
    else
    {
        CurrentPlayListIndex++;
        on_List_itemDoubleClicked(ui->List->item(CurrentPlayListIndex));
        ui->List->setCurrentRow(CurrentPlayListIndex);
    }
}
void Playlist::OnRandomPlay()
{
    int listCount = ui->List->count();
    if (listCount == 0)
    {
        return;
    }
    int newIndex;
    if (listCount == 1)
    {
        newIndex = 0;
    }
    else
    {
        do
        {
            newIndex = QRandomGenerator::global()->bounded(listCount);
        } while (newIndex == CurrentPlayListIndex);
    }
    CurrentPlayListIndex = newIndex;
    on_List_itemDoubleClicked(ui->List->item(CurrentPlayListIndex));
    ui->List->setCurrentRow(CurrentPlayListIndex);
}

void Playlist::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty())
    {
        return;
    }

    for (QUrl url : urls)
    {
        QString strFileName = url.toLocalFile();

        OnAddFile(strFileName);
    }
}

void Playlist::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction();
}
