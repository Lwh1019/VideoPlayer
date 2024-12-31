# VideoPlayer

改编自 https://github.com/itisyang/playerdemo/tree/master

用Visual Studio 2022编写，在原先的基础上修复了一些隐藏的Bug，同时增加了字幕显示与插入、倍速播放（变速不变调）、视频质量调整、截图、录屏、播放方式调整、爬取B站视频等功能

编译环境：Visual Studio 2022、FFmpeg 4.3、SDL2

注意：本项目非纯从FFmpeg底层原理上实现部分功能，在一些没有必要的情况下直接使用了FFmpeg命令行实现对应功能，如有需要可自行实现。

# 一、主交互界面：
未运行时：
![image](https://github.com/user-attachments/assets/0d054bc4-d9e5-44a4-9393-1ac9dfc0d52b)

运行时界面：
![image](https://github.com/user-attachments/assets/ddf7b178-eb31-456a-849f-5c2e9b8b08a4)
