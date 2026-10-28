# 统一透视页面：选中心与 ChArUco 配置

## 操作

1. 打开透视页面，加载相机内参 YAML 或固定平面 YAML。顶部自动显示识别出的模式。
2. 打开同分辨率的输入图片。相机模式确认“相机原图／已去畸变图”；固定平面模式只接受原图。
3. 左侧配置当前板型，支持表单、JSON 导入／保存和整板 ID 预览。普通棋盘的行列也填写整板格数。
4. 在输入图片上左键单击选中心，青色十字立即显示；也可点击“取图像中心”。中键拖动、滚轮缩放后仍按原图像素定位。
5. 未加载／求解映射时先保存像素选择，毫米位置显示“待求解”。相机模式检测成功后自动换算；固定平面模式使用已有映射。
6. 输入中心毫米坐标可微调，修改宽高或密度保持中心不变。默认范围 310×310 mm，密度 10 px/mm。
7. 等待对应预览成功后保存校正图或导出振镜测量 YAML。计算期间可以继续选中心；旧请求的结果不会覆盖新选择。

切换图片清除旧选择，参数变化立即清除旧预览并禁用导出。输入图外点击不生效，失败后不会保存上一次图像。

## Step06 默认板

新建配置使用 DICT_5X5_100，12×9 方格，方格 30 mm，标记 22.5 mm，连续 ID 0～53。
完整板包含 54 个标记、88 个内角点。最低有效标记数 7、角点数 8、插值相邻标记数 2。

图案 360×270 mm，左右各留白 20 mm、上下各 15 mm，板材 400×300 mm，板厚 3.2 mm。
预览按这些尺寸绘制板材边界、图案、坐标轴和标记 ID。

JSON 与表单双向同步。显式 marker_ids 优先作为 ID 布局；同时提供 min_marker_id 时必须与最小 ID 一致。
没有显式数组时按起始 ID 连续生成；字典容量不足、重复 ID、非法尺寸、留白与外形不一致均会失败。
旧配置未提供留白时保持原模型坐标约定，不能用新默认值改写。

棋盘图案左上边界是中心输入框所用的板坐标原点；最终毫米输出原点始终为所选 ROI 左上边界。
留白不会重复加到输出毫米坐标中。板厚记录为配置元数据，不会自动修正不同高度的成像比例；标定图案与测量点须位于同一高度平面。

## 两种模式

相机内参模式使用当前板配置检测一次，先拟合去畸变像素到棋盘毫米的基础 H。之后修改 ROI 只组合输出映射，不重新检测。
Raw 输入按文件中的 K_new 去畸变角点；已去畸变输入直接使用角点，须来自同一 K_new、原分辨率且未经裁剪。

输出 ROI 可以小于整块棋盘；需完整落在输入图片内，且映射无退化或折叠。区域超出棋盘角点凸包时允许调试出图和导出，
显示“棋盘覆盖外推、精度未验收”，不能据此认定达到 0.1 mm。

固定平面模式保留模型中的原始板配置。当前检查板配置可单独修改，检查仅叠加观测结果，不改毫米映射或验收状态。
不含棋盘的后续测量图片也能选范围和校正。原有效域、原图边界、3×3 分区覆盖及实物验收规则继续执行。

## 接口与文件

原工厂和已有方法签名保留。头文件与本轮 DLL/LIB 必须配套使用，不能混用旧版 Config 的二进制。

- BoardConfigurationPort.h：DefaultBoardConfiguration、NormalizeBoardConfiguration、PreviewBoardConfiguration、InspectBoardConfiguration。
- CameraCalibPort.h：新增 CalibrateBoard(images, boardJson)，与透视共用完整参数解析。原 Calibrate 调用行为保留。
- CameraWarptivePort.h：Config 新增 boardConfiguration 和 useOutputRegion；新增 MapInputToBoard、SetOutputRegion、GetReport。
- PlaneCalibrationPort.h：新增 InspectBoard，检查不会改变模型、ROI 或独立实物验收记录。

相机区域模式先 CalibWarptive，再 MapInputToBoard 将所选输入像素换算为板毫米坐标，调用 SetOutputRegion 后出图／导出。
板参数或图片变化重新求解；中心、宽高和密度变化只调用 SetOutputRegion。

相机模式仍导出 v1，固定平面导出 v2。客户继续 LoadCalibrationData(oneYaml) 后用原始振镜点阵图片检测，不需要调用生成端接口。
v1 的 homography/pixPerMm 负责毫米换算；实际渲染 H 单独记录在 region_metadata_json 的 base64 分块中，并保留 ROI、实际 X/Y 密度和半像素约定。
两种 H 对应相同物理区域，但像素采样不要求采用相同标称密度。

Nx=max(1,round(W×p))、Ny=max(1,round(H×p))；校正像素中心对应 X=(u+0.5)W/Nx、Y=(v+0.5)H/Ny。
改变渲染密度不改变毫米点位。区域越界不会自动缩小物理范围。

## 示例与验收

SDK 新增 CameraPerspectiveExample，接受相机 YAML、输入图片、输出 YAML、板 JSON、宽、高、密度及可选像素中心。
sample/step06_board.json 和 step06_raw.png 是与默认板一致的合成接入示例；不能作为设备的真实标定。
现有大板局部图示例和旧版 v1/v2 点阵示例继续保留。

测试包含实际鼠标事件、缩放／中键平移、图片外点击、连续预览请求、54 标记／88 角点、
局部观测、起始 ID、JSON 同步、两种输入阶段及渲染／点映射一致性。
实际 0.1 mm 精度仍须冻结当前模型与 ROI，使用可靠独立尺寸参考验收。

角点精化窗口按方格与标记之间的像素间距限制，避免标记纹理进入棋盘角点邻域。
邻近图案可能引起亚像素位置偏移的说明见 [OpenCV ChArUco 检测文档](https://docs.opencv.org/3.4.0/df/d4a/tutorial_charuco_detection.html)。
