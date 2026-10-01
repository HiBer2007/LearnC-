# GUI 冒烟测试：让窗口也能无人值守验证

`GUI冒烟.ps1` 解决一个问题：**GUI 程序没有控制台输出，结果只在窗口里，
肉眼看过不等于验证过，自动化流程也拿不到任何东西。**

它绕开「看」这一步：启动程序 → 按标题找到窗口 → 给控件发消息 →
把控件里的文本读回来 → 需要时截图并统计颜色像素。

| 能做的 | 靠什么 |
|---|---|
| 点按钮、勾复选框 | `BM_CLICK`（`0x00F5`） |
| 改输入框文本 | `WM_SETTEXT`（`0x000C`） |
| 读回结果框文本 | `WM_GETTEXT`（`0x000D`） |
| 点自绘的面板、列表 | `WM_LBUTTONDOWN`（`0x0201`），客户区坐标 |
| 验证自绘内容画出来了 | `PrintWindow` 截图 + 颜色像素统计 |
| 收尾 | `WM_CLOSE`（`0x0010`） |

## 怎么用

`PowerShell`

```powershell
pwsh -File <工作区>\工具\GUI冒烟\GUI冒烟.ps1 -Exe <程序路径> -WindowTitle <标题片段> [其它参数]
```

| 参数 | 说明 |
|---|---|
| `-Exe` | 要启动的可执行文件，必填 |
| `-WindowTitle` | 主窗口标题里应当包含的片段。**不写死任何程序的名字**，留空时取该进程面积最大的可见顶层窗口 |
| `-InputId` + `-Text` | 给该编号的控件写文本 |
| `-ClickId` | 给该编号的控件发 `BM_CLICK` |
| `-ClickAt "x,y"` | 在客户区坐标处按下左键 |
| `-ReadId` | 读回这些编号的控件文本，可给多个 |
| `-Shot` | 截图保存路径（PNG） |
| `-ColorCount "R,G,B"` | 统计截图里该颜色的像素个数 |
| `-WaitMs` | 启动后等待多少毫秒再找窗口，默认 1500 |
| `-KeepOpen` | 结束时保留窗口，便于接着手动操作 |

动作按固定顺序执行：写文本 → 点坐标 → 点按钮 → 读文本 → 截图。
控件编号就是 Win32 里的控件 ID，即 `CreateWindowExW` 的 `hMenu` 参数；
窗口里有哪些控件、编号是多少，脚本会在启动后直接列出来。

## 两个真实用例

### 用例一：`B-examples/07-cpp-class-basics` 的 Win32 界面

先把示例构建好（见该示例的 README），再运行：

`PowerShell`

```powershell
cd <工作区>
cmake --build B-examples\07-cpp-class-basics\build\mingw        # 若还没构建过，先 cmake --preset mingw-gdb

pwsh -File 工具\GUI冒烟\GUI冒烟.ps1 `
  -Exe B-examples\07-cpp-class-basics\build\mingw\bin\app_gui_win32.exe `
  -WindowTitle "IntVector" -InputId 1001 -Text "3 1 4 1 5 9 2 6" -ClickId 1002 -ReadId 1004
```

`实测数据`
`Text`

```text
[进程] pid=51068  程序=K:\C相关课程\B-examples\07-cpp-class-basics\build\mingw\bin\app_gui_win32.exe
[窗口] 标题=[示例 07 · IntVector 值类型演示]  类名=IntVectorDemoWnd  客户区=720x620
[控件] 共 6 个
        id=0     类名=Static   文本=[输入（空格、逗号或分号分隔的整数，最多 64 个）：]
        id=1001  类名=Edit     文本=[1 1 2 3 5 8 13 21 34 55]
        id=1002  类名=Button   文本=[计算]
        id=1003  类名=Button   文本=[跑自测]
        id=0     类名=Static   文本=[结果（只读）：]
        id=1004  类名=Edit     文本=[]
[操作] 给控件 1001 写入「3 1 4 1 5 9 2 6」
[操作] 点击控件 1002（BM_CLICK）
[读回] 控件 1004 的文本：
        输入数列 : [3, 1, 4, 1, 5, 9, 2, 6]
        前缀和   : [3, 4, 8, 9, 14, 23, 25, 31]
        每项乘 3 : [9, 3, 12, 3, 15, 27, 6, 18]
        每项加 1 : [4, 2, 5, 2, 6, 10, 3, 7]
        长度 8，容量 8，首项 3，末项 6，总和 31
        以上都是 IntVector 算出来的：+ 与 * 走的是自己写的运算符
[收尾] 窗口还在吗 = False；进程已退出 = True
```

结果框里的数列与命令行版一致，说明界面确实把输入交给了核心模块，
并且把核心模块的结果显示了出来。

### 用例二：`B-examples/08-cpp-inheritance-polymorphism` 的自绘面板

这个例子的格式面板是 GDI 自己画的，不是标准控件，因此用 `-ClickAt` 点坐标，
再用 `-ColorCount` 确认面板真的画出来了：

`PowerShell`

```powershell
pwsh -File 工具\GUI冒烟\GUI冒烟.ps1 `
  -Exe B-examples\08-cpp-inheritance-polymorphism\build\mingw\bin\app_gui_win32.exe `
  -WindowTitle "导出器" -ClickAt "522,60" -ReadId 3004 `
  -Shot "$env:TEMP\gui08.png" -ColorCount "240,248,242"
```

`实测数据`
`Text`

```text
[进程] pid=50736  程序=K:\C相关课程\B-examples\08-cpp-inheritance-polymorphism\build\mingw\bin\app_gui_win32.exe
[窗口] 标题=[示例 08 · 导出器：抽象基类、工厂与虚析构]  类名=ExporterDemoWnd  客户区=780x600
[控件] 共 6 个
        id=0     类名=Static   文本=[渲染结果（只读）：]
        id=3004  类名=Edit     文本=[]
        id=3001  类名=Button   文本=[重绘]
        id=3002  类名=Button   文本=[换成含转义字符的表格]
        id=3003  类名=Button   文本=[跑自测]
        id=0     类名=Static   文本=[当前格式：csv（逗号分隔，字段含逗号或引号时加双引号）；表格：学生成绩]
[操作] 在客户区 (522,60) 处按下左键
[读回] 控件 3004 的文本：
        {
  "title": "学生成绩",
  "columns": ["姓名", "语文", "数学"],
  "rows": [
    ["小明", "78", "92"],
    ["小红", "95", "88"],
    ["小刚", "65", "71"]
  ]
}
[截图] 已保存 C:\Users\HIBER2~1\AppData\Local\Temp\guiprobe\smoke08.png（780x600，PrintWindow）
[颜色] 240,248,242 在截图里出现 23619 个像素
[收尾] 窗口还在吗 = False；进程已退出 = True
```

点第三个面板之后结果框换成了 JSON，说明 `WM_LBUTTONDOWN` 的命中判定生效；
颜色统计里的 240,248,242 正是未被选中面板的底色，说明 GDI 绘制确实执行了。

## 局限

| 局限 | 说明 |
|---|---|
| 只能驱动标准控件 | `WM_GETTEXT` / `WM_SETTEXT` 靠系统跨进程编组，标准控件支持；自绘控件与 Qt 控件读不到文本 |
| Qt 界面只能截图 | Qt 的子控件不是 Win32 窗口（没有独立 HWND），脚本只能确认顶层窗口存在并截图比对 |
| 自绘内容要靠截图 | 颜色像素个数能证明「画了」，证明不了「画对了」；要看画得对不对还得看图 |
| 坐标依赖窗口大小 | `-ClickAt` 用的是客户区坐标，窗口尺寸变了要跟着改 |
| 一次只能驱动一个窗口 | 脚本按进程找窗口，多窗口程序需要多次调用并指定不同标题 |
| 提权不一致时消息会被挡 | 被驱动的程序若以管理员身份运行，普通权限的脚本发不过去消息（UIPI） |
| 不是单元测试 | 它是冒烟验证：确认界面能起来、能响应、能显示正确内容，不替代核心逻辑的自测 |

## 实现说明

脚本用 PowerShell 调用 `user32.dll`：

- 找窗口：`EnumWindows` + `GetWindowThreadProcessId` 按进程号筛，再用标题片段匹配
  （不用 `FindWindow`，因为它要求标题完全一致）
- 列控件：`EnumChildWindows` + `GetDlgCtrlID` + `GetClassNameW`
- 发消息：`SendMessageW`；读文本时缓冲区由系统跨进程编组，不必自己分配远端内存
- 截图：`PrintWindow` 把窗口画到 `System.Drawing.Bitmap` 的 HDC 上，再逐像素统计颜色

用 `pwsh -File` 运行：每次都是新进程，`Add-Type` 不会和上一次的类型重名。
