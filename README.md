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

# 二、控制栏界面：

  该界面包含了前向播放、播放/停止、向后播放按钮，用于控制视频的播放；同时还有用于显示视频播放的全部时长和当前时长，附加上倍速控制、画质控制、字幕控制、录制控制、列表控制、播放方式控制，构成整个控制栏界面。倍速控制支持多个倍速的调整功能（例如0.5x,0.75x,2.0x等），画质控制支持（360P，720P，1080P，1080P 60帧等），播放方式包括循环播放、列表播放、随机播放等。字幕控制会自动检测当前视频有无字幕，若内嵌字幕则播放字幕，否则可以支持在视频中嵌入软字幕（.srt,.ass等字幕文件）。

![image](https://github.com/user-attachments/assets/466980e1-8899-4fc1-b11f-49d9133b885a)
