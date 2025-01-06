#include <QContextMenuEvent>
#include <QFileDialog>

#include "medialist.h"

#pragma execution_character_set("utf-8")

MediaList::MediaList(QWidget *parent)
    : QListWidget(parent),
    Menu(this),
      ActAdd(this),
      ActRemove(this),
      ActClearList(this)
{
}

MediaList::~MediaList()
{
}

bool MediaList::Init()
{
    ActAdd.setText("添加");
    Menu.addAction(&ActAdd);
    ActRemove.setText("移除所选项");
    QMenu* stRemoveMenu = Menu.addMenu("移除");
    stRemoveMenu->addAction(&ActRemove);
    ActClearList.setText("清空列表");
    Menu.addAction(&ActClearList);
    Menu.setStyleSheet("QMenu { background-color: #2C2C2C; color: white; border: 1px solid #5C5C5C; }"
        "QMenu::item { padding: 5px 20px; }"
        "QMenu::item:selected { background-color: #4C9AFF; }");

    connect(&ActAdd, &QAction::triggered, this, &MediaList::AddFile);
    connect(&ActRemove, &QAction::triggered, this, &MediaList::RemoveFile);
    connect(&ActClearList, &QAction::triggered, this, &QListWidget::clear);

    return true;
}

void MediaList::contextMenuEvent(QContextMenuEvent* event)
{
    Menu.popup(event->globalPos());
}

void MediaList::AddFile()
{
    QStringList listFileName = QFileDialog::getOpenFileNames(this, "打开文件", QDir::homePath(),"音视频文件(*.wav *.ogg *.mp3 *.mkv *.rmvb *.mp4 *.avi *.flv *.wmv *.3gp *.mov *.yuv)");
    for (QString strFileName : listFileName)
    {
        emit SigAddFile(strFileName);
    }
}
void MediaList::RemoveFile()
{
    takeItem(currentRow());
}

