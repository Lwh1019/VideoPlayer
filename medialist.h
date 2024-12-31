// 播放列表菜单
// Editor: Liwh 2024/12/22

#pragma once

#include <QListWidget>
#include <QMenu>
#include <QAction>

class MediaList : public QListWidget
{
    Q_OBJECT

public:
    MediaList(QWidget *parent = 0);
    ~MediaList();
    bool Init();
protected:
    void contextMenuEvent(QContextMenuEvent* event);
private:
    void AddFile();
    void RemoveFile();
signals:
    void SigAddFile(QString strFileName);

private:
    QMenu Menu;

    QAction ActAdd;     //添加文件
    QAction ActRemove;  //移除文件
    QAction ActClearList;//清空列表
};
