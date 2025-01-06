# VideoPlayer

改编自 https://github.com/itisyang/playerdemo/tree/master

用Visual Studio 2022编写，在原先的基础上修复了一些隐藏的Bug，同时增加了字幕显示与插入、倍速播放（变速不变调）、视频质量调整、截图、录屏、播放方式调整、爬取B站视频等功能

编译环境：Visual Studio 2022、FFmpeg 4.3、SDL2

注意：本项目非纯从FFmpeg底层原理上实现部分功能，在一些没有必要的情况下直接使用了FFmpeg命令行实现对应功能，如有需要可自行实现。

## 2025年1月5日 一改：
Bug修复：
1.切换视频后菜单难以切出

具体修改：因为貌似涉及到SDL的窗口绑定，将原先的QT的Label锁定，但在很多线程上还是用到了SDL，所以无法直接调用SDL_Exit()函数来进行初始化；所以退而求其次，新增一个快捷键M来快速打开对应的菜单

2.视频条切换只能1秒之间跳转

具体修改：将原先使用int型的变量替换为double，并修改对应的SliderMAX，使视频条能够平滑过渡移动

功能修改：

1.添加音乐播放功能并设置对应的播放界面（去除原先黑色底）、封面设置为唱片 （已完成）

2.修改原先的视频质量调整功能 （完成一半但受阻）

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
# 基础功能：
# 一、视频播放、暂停：

![image](https://github.com/user-attachments/assets/cb100bc9-cacb-4bf1-8b1c-acf22795732c)

# 二、视频进度、音量调整：

![image](https://github.com/user-attachments/assets/c5fa8291-6e6a-4fe5-8947-a4873967cd69)

# 三、支持添加并播放多种视频格式：

![image](https://github.com/user-attachments/assets/f2d7f12e-d79f-4864-9681-d6c49bcfd8c4)

# 拓展功能：
# 一、视频倍速播放控制：

![image](https://github.com/user-attachments/assets/873f02f6-0ff0-492c-acb4-a37d6271c485)

# 二、视频截图功能：

![image](https://github.com/user-attachments/assets/4d2722d8-f180-459a-aafa-ffd29ffa975f)

# 三、视频质量调整控制：

![image](https://github.com/user-attachments/assets/1fda9bb8-a981-452a-aa7f-07304bed8293)

# 四、字幕加载与显示控制：
未打开时：

![image](https://github.com/user-attachments/assets/f87df601-3273-451f-9ceb-c9938716ade0)

打开时：

![image](https://github.com/user-attachments/assets/023edfab-9289-498b-8a94-76ed071ed1ef)

对于没有字幕的原视频：   先进行字幕文件的加载

![image](https://github.com/user-attachments/assets/28cd4d21-3a79-4cb5-9bd7-1fdf9433ba67)

# 五、录制功能：
按下后：   左下角会有提示

![image](https://github.com/user-attachments/assets/8999db06-21ba-4a80-b7f1-27032d0ac9d3)

再次按下后即可保存下来：

![image](https://github.com/user-attachments/assets/da2e21ce-c25d-47d2-9876-1f3977d4b03b)

# 六、播放方式功能：
针对于一个视频播放结束后接下来要做什么，目前实现了三种方式：循环播放、列表播放、随机播放。

![image](https://github.com/user-attachments/assets/40fd643c-6b81-42fb-a74f-612184d82aea)

# 七、网络视频爬取功能：
目前主要只实现了对于Bilibili上的视频进行爬取，通过网络技术连接Bilibili的网络视频库爬取其中对应视频的最高清晰度视频到本地，同时也可以设置大会员Cookie来进行更高画质的爬取。

![image](https://github.com/user-attachments/assets/107ad124-7218-45a8-a1b0-2e35479b3a24)

下载后即可播放：

![image](https://github.com/user-attachments/assets/969a23ca-7fd3-4446-a800-9688af66f92d)
