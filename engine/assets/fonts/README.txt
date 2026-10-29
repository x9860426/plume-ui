把任意 TTF/OTF 字体复制到本目录并重命名为 font.ttf，
示例程序会自动加载并烘焙成 1024x1024 字形图集上传 GPU。

例如（Windows 自带微软雅黑）：
    copy C:\Windows\Fonts\msyh.ttf font.ttf

或使用思源黑体 / Noto Sans CJK 等开源字体以获得中文支持。
若要支持中文，需修改 src/ui/UIEngine.cpp 中 Font::bake 的字符范围
（当前仅烘焙 ASCII 32~126），改为按需烘焙常用汉字区间即可。