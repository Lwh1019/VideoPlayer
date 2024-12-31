// 播放列表界面
// Editor: Liwh  2024/12/22
#pragma once

#include <QWidget>
#include <QListWidgetItem>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QMimeData>
#include "ui_playlist.h"

class Playlist : public QWidget,public Ui::Playlist
{
    Q_OBJECT

public:
    QString GenerateThumbnail(const QString& videoPath);
    explicit Playlist(QWidget *parent = 0);
    ~Playlist();

	bool Init();
    bool GetPlaylistStatus(); //列表状态

public:

    void OnAddFile(QString strFileName);
    void OnAddFileAndPlay(QString strFileName);

    void OnBackwardPlay();
    void OnForwardPlay();
    void OnRandomPlay();
    QSize sizeHint() const
    {
        return QSize(150, 900);
    }
protected:

    void dropEvent(QDropEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);

signals:
    void SigUpdateUi();	
	void SigPlay(QString strFile);

private:
    bool InitUi();
    bool ConnectSignalSlots();
    
private slots:

	void on_List_itemDoubleClicked(QListWidgetItem *item);

private:
    Ui::Playlist *ui;

    int CurrentPlayListIndex;
};
