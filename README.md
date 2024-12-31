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

# 三、快捷菜单栏：

快捷菜单栏给出了部分功能的快捷按钮提示，同时也附加了一些控制栏功能在一起，方便用户能够更快的进行界面或画面的控制，包括快捷打开文件、快捷退出等等操作。

![image](https://github.com/user-attachments/assets/aff3abd2-b669-45a9-b60d-9d5053a9d4f7)

# 四、视频播放列表展示：

  视频播放列表界面提供了播放视频的标题和具体封面（视频的第一个i帧），同时针对视频播放列表使用不同的菜单栏，帮助更快的进行添加文件、删除文件等等操作。

![image](https://github.com/user-attachments/assets/1e992b9c-8641-4783-8f98-1cb94e87dd17)

# 五、标题界面展示：

  标题界面主要用于展示当前模仿视频的名称和控制窗口的最小化、最大化、全屏、关闭等操作，同时为了增加功能性，在点击界面上的Bilibili按钮后输入播放的B站视频的URL就能够在线爬取B站的视频到本地并播放。

![image](https://github.com/user-attachments/assets/1ba81f45-60d8-4737-9b94-5677cc7f4d9c)


## 功能实现效果展示：
基础功能：
# 一、视频播放、暂停：

![image](https://github.com/user-attachments/assets/cb100bc9-cacb-4bf1-8b1c-acf22795732c)

