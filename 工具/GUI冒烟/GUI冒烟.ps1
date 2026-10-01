<#
.SYNOPSIS
    无人值守驱动一个 Win32 GUI 程序：点按钮、读回控件文本、截图。

.DESCRIPTION
    GUI 程序没有控制台输出，结果只在窗口里，肉眼看不等于能验证。
    这个脚本绕开「看」这一步：找到窗口，给控件发消息，把控件里的文本读回来，
    必要时截图并统计颜色像素，于是 GUI 也能进自动化流程。

    它按顺序做四件事：
      1. 启动程序，按标题片段找到它的主窗口（标题不写死，任何窗口都能用）
      2. 列出所有子控件：控件编号、类名、当前文本
      3. 按参数依次执行：设置文本 → 在客户区某点按下左键 → 点按钮 → 读回控件文本
      4. 需要时用 PrintWindow 截图，并统计指定颜色的像素个数

    识别的消息：
      WM_SETTEXT      (0x000C)  改控件文本
      WM_GETTEXT      (0x000D)  读控件文本
      WM_LBUTTONDOWN  (0x0201)  在客户区坐标按下左键，用于自绘按钮或面板
      BM_CLICK        (0x00F5)  点标准按钮与复选框
      WM_CLOSE        (0x0010)  收尾关窗

.PARAMETER Exe
    要启动的可执行文件路径。相对路径按当前目录解析。

.PARAMETER WindowTitle
    主窗口标题里应当包含的片段。留空时取该进程面积最大的可见顶层窗口。

.PARAMETER InputId
    要改文本的控件编号（配合 -Text）。

.PARAMETER Text
    写进上面那个控件的文本。

.PARAMETER ClickAt
    在客户区坐标处按下左键，写法 "x,y"。自绘的按钮、面板、列表用这个。

.PARAMETER ClickId
    要点击的控件编号（标准按钮、复选框用这个）。

.PARAMETER ReadId
    要读回文本的控件编号，可以给多个。

.PARAMETER Shot
    截图保存路径（PNG）。用 PrintWindow 抓客户区。

.PARAMETER ColorCount
    统计截图里某个颜色的像素个数，写法 "R,G,B"。
    自绘内容（GDI 画的柱子、面板）只能这样验证。

.PARAMETER WaitMs
    启动后等待多少毫秒再找窗口，默认 1500。

.PARAMETER KeepOpen
    结束时不要关闭窗口（默认发 WM_CLOSE）。加上它便于自己接着手动操作。

.EXAMPLE
    pwsh -File 工具\GUI冒烟\GUI冒烟.ps1 -Exe .\build\app_gui.exe -WindowTitle "演示"

.EXAMPLE
    pwsh -File 工具\GUI冒烟\GUI冒烟.ps1 -Exe .\app_gui.exe -WindowTitle "IntVector" `
        -InputId 1001 -Text "3 1 4 1 5 9 2 6" -ClickId 1002 -ReadId 1004

.EXAMPLE
    pwsh -File 工具\GUI冒烟\GUI冒烟.ps1 -Exe .\app_gui.exe -WindowTitle "导出器" `
        -ClickAt "522,60" -ReadId 3004 -Shot "$env:TEMP\gui.png" -ColorCount "198,224,245"

.NOTES
    脚本用 pwsh -File 的方式运行。每次都是一新进程，
    因此内部的 Add-Type 不会和上一次运行的类型重名。
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Exe,
    [string]$WindowTitle = "",
    [int]$InputId = 0,
    [string]$Text = "",
    [string]$ClickAt = "",
    [int]$ClickId = 0,
    [int[]]$ReadId = @(),
    [string]$Shot = "",
    [string]$ColorCount = "",
    [int]$WaitMs = 1500,
    [switch]$KeepOpen
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

# ---------------------------------------------------------------
#  Win32 接口：找窗口、发消息、截图
# ---------------------------------------------------------------
Add-Type -TypeDefinition @"
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public static class GuiSmoke {
    public delegate bool EnumProc(IntPtr hwnd, IntPtr param);

    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr param);
    [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr parent, EnumProc cb, IntPtr param);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
    [DllImport("user32.dll")] public static extern int GetDlgCtrlID(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hwnd, out RECT rect);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hwnd, IntPtr hdc, uint flags);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr hwnd, StringBuilder text, int max);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassNameW(IntPtr hwnd, StringBuilder text, int max);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern IntPtr SendMessageW(IntPtr hwnd, uint msg, IntPtr wp, StringBuilder lp);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern IntPtr SendMessageW(IntPtr hwnd, uint msg, IntPtr wp, IntPtr lp);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern IntPtr SendMessageW(IntPtr hwnd, uint msg, IntPtr wp, string lp);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }

    public static List<IntPtr> TopLevelOf(uint pid) {
        List<IntPtr> found = new List<IntPtr>();
        EnumWindows(delegate(IntPtr hwnd, IntPtr param) {
            uint owner;
            GetWindowThreadProcessId(hwnd, out owner);
            if (owner == pid) { found.Add(hwnd); }
            return true;
        }, IntPtr.Zero);
        return found;
    }

    public static List<IntPtr> Children(IntPtr parent) {
        List<IntPtr> found = new List<IntPtr>();
        EnumChildWindows(parent, delegate(IntPtr hwnd, IntPtr param) {
            found.Add(hwnd);
            return true;
        }, IntPtr.Zero);
        return found;
    }

    public static string TextOf(IntPtr hwnd) {
        StringBuilder sb = new StringBuilder(16384);
        GetWindowTextW(hwnd, sb, 16384);
        return sb.ToString();
    }

    public static string ClassOf(IntPtr hwnd) {
        StringBuilder sb = new StringBuilder(256);
        GetClassNameW(hwnd, sb, 256);
        return sb.ToString();
    }

    public static string ReadText(IntPtr hwnd) {
        StringBuilder sb = new StringBuilder(16384);
        SendMessageW(hwnd, 0x000D, (IntPtr)16384, sb);
        return sb.ToString();
    }

    public static void WriteText(IntPtr hwnd, string text) { SendMessageW(hwnd, 0x000C, IntPtr.Zero, text); }
    public static void ClickButton(IntPtr hwnd) { SendMessageW(hwnd, 0x00F5, IntPtr.Zero, IntPtr.Zero); }
    public static void ClickAtPoint(IntPtr hwnd, int x, int y) {
        SendMessageW(hwnd, 0x0201, IntPtr.Zero, (IntPtr)((y << 16) | (x & 0xFFFF)));
    }
    public static void CloseWindow(IntPtr hwnd) { SendMessageW(hwnd, 0x0010, IntPtr.Zero, IntPtr.Zero); }
}
"@

function Get-ChildById {
    param([System.Collections.Generic.List[IntPtr]]$Children, [int]$Id)
    foreach ($child in $Children) {
        if ([GuiSmoke]::GetDlgCtrlID($child) -eq $Id) { return $child }
    }
    throw "窗口里没有编号为 $Id 的控件"
}

# ---------------------------------------------------------------
#  1. 启动并找到主窗口
# ---------------------------------------------------------------
$exePath = (Resolve-Path -LiteralPath $Exe).Path
$process = Start-Process -FilePath $exePath -PassThru
Start-Sleep -Milliseconds $WaitMs
$process.Refresh()
if ($process.HasExited) {
    throw "进程已退出（退出码 $($process.ExitCode)），没有窗口可驱动"
}
Write-Output "[进程] pid=$($process.Id)  程序=$exePath"

$candidates = @([GuiSmoke]::TopLevelOf([uint32]$process.Id) |
    Where-Object { [GuiSmoke]::IsWindowVisible($_) -and [GuiSmoke]::TextOf($_) -ne "" })
if ($candidates.Count -eq 0) {
    throw "该进程没有可见的顶层窗口"
}

$main = $null
if ($WindowTitle -ne "") {
    $main = $candidates | Where-Object { [GuiSmoke]::TextOf($_) -like "*$WindowTitle*" } | Select-Object -First 1
    if (-not $main) {
        $titles = ($candidates | ForEach-Object { [GuiSmoke]::TextOf($_) }) -join " / "
        throw "没有标题含「$WindowTitle」的窗口。现有的顶层窗口：$titles"
    }
} else {
    # 没给标题片段时，取客户区面积最大的那个
    $best = 0
    foreach ($candidate in $candidates) {
        $rect = New-Object GuiSmoke+RECT
        [void][GuiSmoke]::GetClientRect($candidate, [ref]$rect)
        $area = $rect.Right * $rect.Bottom
        if ($area -gt $best) { $best = $area; $main = $candidate }
    }
}

$client = New-Object GuiSmoke+RECT
[void][GuiSmoke]::GetClientRect($main, [ref]$client)
Write-Output "[窗口] 标题=[$([GuiSmoke]::TextOf($main))]  类名=$([GuiSmoke]::ClassOf($main))  客户区=$($client.Right)x$($client.Bottom)"

# ---------------------------------------------------------------
#  2. 列出子控件
# ---------------------------------------------------------------
$children = [GuiSmoke]::Children($main)
Write-Output "[控件] 共 $($children.Count) 个"
foreach ($child in $children) {
    $label = [GuiSmoke]::TextOf($child)
    if ($label.Length -gt 46) { $label = $label.Substring(0, 46) + "…" }
    Write-Output ("        id={0,-5} 类名={1,-8} 文本=[{2}]" -f [GuiSmoke]::GetDlgCtrlID($child), [GuiSmoke]::ClassOf($child), $label)
}

# ---------------------------------------------------------------
#  3. 依次执行：改文本 → 点坐标 → 点按钮 → 读文本
# ---------------------------------------------------------------
if ($InputId -gt 0) {
    [GuiSmoke]::WriteText((Get-ChildById $children $InputId), $Text)
    Write-Output "[操作] 给控件 $InputId 写入「$Text」"
}

if ($ClickAt -ne "") {
    $xy = $ClickAt -split ","
    if ($xy.Count -ne 2) { throw "-ClickAt 要写成 `"x,y`" 的形式" }
    [GuiSmoke]::ClickAtPoint($main, [int]$xy[0], [int]$xy[1])
    Start-Sleep -Milliseconds 300
    Write-Output "[操作] 在客户区 ($ClickAt) 处按下左键"
}

if ($ClickId -gt 0) {
    [GuiSmoke]::ClickButton((Get-ChildById $children $ClickId))
    Start-Sleep -Milliseconds 300
    Write-Output "[操作] 点击控件 $ClickId（BM_CLICK）"
}

foreach ($id in $ReadId) {
    $child = Get-ChildById $children $id
    Write-Output "[读回] 控件 $id 的文本："
    [GuiSmoke]::ReadText($child) -split "`r`n" | ForEach-Object { Write-Output "        $_" }
}

# ---------------------------------------------------------------
#  4. 截图与颜色统计
# ---------------------------------------------------------------
if ($Shot -ne "") {
    $bitmap = New-Object System.Drawing.Bitmap($client.Right, $client.Bottom)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $hdc = $graphics.GetHdc()
    [void][GuiSmoke]::PrintWindow($main, $hdc, 0)
    $graphics.ReleaseHdc($hdc)
    $graphics.Dispose()
    $bitmap.Save($Shot, [System.Drawing.Imaging.ImageFormat]::Png)
    Write-Output "[截图] 已保存 $Shot（$($client.Right)x$($client.Bottom)，PrintWindow）"

    if ($ColorCount -ne "") {
        $rgb = $ColorCount -split ","
        if ($rgb.Count -ne 3) { throw "-ColorCount 要写成 `"R,G,B`" 的形式" }
        $r = [int]$rgb[0]; $g = [int]$rgb[1]; $b = [int]$rgb[2]
        $hits = 0
        for ($y = 0; $y -lt $bitmap.Height; $y++) {
            for ($x = 0; $x -lt $bitmap.Width; $x++) {
                $pixel = $bitmap.GetPixel($x, $y)
                if ($pixel.R -eq $r -and $pixel.G -eq $g -and $pixel.B -eq $b) { $hits++ }
            }
        }
        Write-Output "[颜色] $ColorCount 在截图里出现 $hits 个像素"
    }
    $bitmap.Dispose()
}

# ---------------------------------------------------------------
#  收尾
# ---------------------------------------------------------------
if ($KeepOpen) {
    Write-Output "[收尾] 按 -KeepOpen 保留窗口，请自行关闭"
} else {
    [GuiSmoke]::CloseWindow($main)
    Start-Sleep -Milliseconds 600
    Write-Output "[收尾] 窗口还在吗 = $([GuiSmoke]::IsWindow($main))；进程已退出 = $($process.HasExited)"
}
