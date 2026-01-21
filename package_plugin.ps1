# 自动检测构建配置 (Debug 或 Release)
$buildRoot = "build"
$config = "Debug" # 默认优先查找 Debug
if (Test-Path "$buildRoot/Release/TemplateMatchPlugin.dll") {
    $config = "Release"
} elseif (Test-Path "$buildRoot/Debug/TemplateMatchPlugin.dll") {
    $config = "Debug"
} else {
    Write-Warning "未在 build/Debug 或 build/Release 中找到 TemplateMatchPlugin.dll。"
    Write-Warning "请先编译项目！(尝试运行 'CMake Build (Debug)' 任务)"
    exit
}

$buildDir = "$buildRoot/$config"
$outputDir = "dll/TemplateMatchPlugin_Package_$config"
$sourceDir = "src"

Write-Host "检测到构建配置: $config"
Write-Host "构建目录: $buildDir"
Write-Host "输出目录: $outputDir"

# 1. 清理并创建输出目录
Write-Host "`n1. 正在清理旧的发布包..."
if (Test-Path $outputDir) { Remove-Item -Recurse -Force $outputDir }
New-Item -ItemType Directory -Path "$outputDir/include" -Force | Out-Null
New-Item -ItemType Directory -Path "$outputDir/bin" -Force | Out-Null

# 2. 复制头文件 (接口)
Write-Host "2. 正在复制头文件..."
if (Test-Path "$sourceDir/interfaces/IMatchPlugin.h") {
    Copy-Item "$sourceDir/interfaces/IMatchPlugin.h" "$outputDir/include/"
    Write-Host "  [OK] IMatchPlugin.h"
} else {
    Write-Error "  [FAIL] 未找到 IMatchPlugin.h"
}

Write-Host "2. 正在复制头文件..."
if (Test-Path "$sourceDir/interfaces/MatchParams.h") {
    Copy-Item "$sourceDir/interfaces/MatchParams.h" "$outputDir/include/"
    Write-Host "  [OK] MatchParams.h"
} else {
    Write-Error "  [FAIL] 未找到 MatchParams.h"
}

if (Test-Path "$sourceDir/common/tools/bscvTool.h") {
    Copy-Item "$sourceDir/common/tools/bscvTool.h" "$outputDir/include/"
    Write-Host "  [OK] bscvTool.h"
} else {
    Write-Error "  [FAIL] 未找到 bscvTool.h"
}

if (Test-Path "$sourceDir/interfaces/Imageprocess.h") {
    Copy-Item "$sourceDir/interfaces/Imageprocess.h" "$outputDir/include/IImageProcess.h"
    Write-Host "  [OK] Imageprocess.h -> IImageProcess.h"
} else {
    Write-Error "  [FAIL] 未找到 Imageprocess.h"
}

# 3. 复制插件 DLL
Write-Host "3. 正在复制插件 DLL..."
$pluginDll = "$buildDir/TemplateMatchPlugin.dll"
if (Test-Path $pluginDll) {
    Copy-Item $pluginDll "$outputDir/bin/"
    Write-Host "  [OK] TemplateMatchPlugin.dll"
} else {
    Write-Error "  [FAIL] 未找到 TemplateMatchPlugin.dll"
}

# 4. 复制 BSCV 依赖库
Write-Host "4. 正在复制 BSCV 依赖库..."
$bscvToolDir = "$sourceDir/plugins/template/tools/bscv"
$bscvBinDir = "$bscvToolDir/bin/$config"

if (Test-Path $bscvBinDir) {
    $bscvDlls = Get-ChildItem -Path $bscvBinDir -Filter "*.dll"
    if ($bscvDlls.Count -gt 0) {
        foreach ($dll in $bscvDlls) {
            Copy-Item $dll.FullName "$outputDir/bin/"
            Write-Host "  [OK] $($dll.Name) (动态库)"
        }
    } else {
        Write-Warning "  在 $bscvBinDir 中未找到 DLL 文件。"
    }
} else {
    Write-Warning "  未找到 BSCV bin 目录: $bscvBinDir"
}

# 解释静态库情况
Write-Host "`n[信息] 关于其他 BSCV 库 (templmatchLib, imgprocLib 等):"
Write-Host "  这些库在 'lib/$config' 中作为静态库 (.lib) 存在，但没有对应的 DLL。"
Write-Host "  这意味着它们已被静态链接到 TemplateMatchPlugin.dll 中。"
Write-Host "  因此，发布包中不需要包含这些库的 DLL 文件。"

# 5. 创建说明文档
$readmeContent = @"
TemplateMatch Plugin 发布包 ($config)
====================================

目录结构:
- include/: 包含开发所需的头文件 (IMatchPlugin.h)
- bin/:     包含插件 DLL (TemplateMatchPlugin.dll) 及其运行时依赖 (如 OpenCV)

集成步骤:
1. 将 'include/IMatchPlugin.h' 包含到您的项目中。
2. 使用 QPluginLoader 加载 'bin/TemplateMatchPlugin.dll'。
3. 确保 'bin' 目录下的所有 DLL 文件都位于应用程序的可执行文件同级目录，或在系统 PATH 中。

注意:
- 本插件已静态链接 BSCV 核心库 (templmatchLib 等)，无需额外分发。
- 本插件依赖 OpenCV 动态库 (已包含在 bin 目录中)。
"@

Set-Content -Path "$outputDir/README.txt" -Value $readmeContent

Write-Host "`n发布包制作完成！位置: $outputDir"
