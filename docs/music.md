# 音乐功能执行大纲

## 1. 第一版目标
- 实现本地 SD 卡音乐播放。
- 默认只扫描 SD 卡 `/music` 目录。
- 扫描完成后写入音乐索引文件。
- 音乐列表按页读取索引并显示，不一次性把全部歌曲常驻 RAM。
- 使用外置 DAC `PCM5102A` 输出音频。
- 使用 `lib/ESP32-audioI2S` 播放库。
- 退出音乐列表后停止播放、释放音频硬件并卸载 SD，不做后台播放。

## 2. 已确认硬件

### 2.1 音频链路
```txt
ESP32-S3 I2S -> PCM5102A -> 耳机 / NS4150B 扬声器功放
```

### 2.2 I2S 引脚
- `BoardConfig::kPinPcmDin = 40`
- `BoardConfig::kPinPcmLrck = 39`
- `BoardConfig::kPinPcmBck = 41`

### 2.3 控制引脚
- `BoardConfig::kPinPcmXsmt = 5`
  - PCM5102A 静音控制。
  - 拉高：软不静音，允许输出。
  - 拉低：静音。
- `BoardConfig::kPinAudioLdoEn = 38`
  - 音频 LDO 使能。
  - 拉高：音频供电开启。
  - 拉低：音频供电关闭。
- `BoardConfig::kPinNsCtrl = 13`
  - NS4150B 关断控制。
  - 拉高：启用扬声器功放。
  - 拉低：关闭扬声器功放。
- `BoardConfig::kPinHpCon = 15`
  - 3.5mm 耳机插入检测。
  - 默认上拉。
  - 低电平：耳机已插入。
  - 高电平：耳机未插入。

## 3. 第三方库

已放入本项目：

```txt
lib/ESP32-audioI2S
```

使用方式：

```cpp
#include <Audio.h>
```

本地文件播放入口：

```cpp
audio.connecttoFS(SD_MMC, path);
```

主循环播放推进：

```cpp
audio.loop();
```

根据 `ESP32-audioI2S` 源码，第一版扫描这些扩展名：
- `.mp3`
- `.m4a`
- `.aac`
- `.wav`
- `.flac`
- `.opus`
- `.ogg`
- `.oga`

说明：
- 扫描阶段只按扩展名过滤。
- 不在扫描阶段解析音频头、ID3、封面、码率或总时长。
- 播放失败由播放服务上报，不在列表阶段提前判定文件一定可播放。

## 4. 第一版运行流程

### 4.1 进入 Music 子菜单
- 不初始化音频硬件。
- 不打开音频 LDO。
- 不运行 `Audio::loop()`。
- 不挂载 SD，直到用户进入音乐列表详情页。

### 4.2 进入音乐列表详情页
执行顺序：
1. 挂载 SD。
2. 检查 `/music` 是否存在。
3. 扫描 `/music` 目录。
4. 过滤支持的音频扩展名。
5. 扫描完成后写入 `/music/.openbread_music.idx`。
6. 按页读取索引文件并渲染音乐列表。

失败处理：
- SD 未插入：显示 `SD卡未插入`。
- `/music` 不存在：显示 `未找到音乐文件`。
- 扫描失败：显示 `SD卡读取失败`。
- 扫描失败时不覆盖旧索引。

### 4.3 列表选择播放
用户在音乐列表页按 `OK`：
1. 读取当前焦点歌曲路径。
2. 开启音频 LDO：`kPinAudioLdoEn = HIGH`。
3. 等待 LDO 稳定。
4. 初始化音频 GPIO 和 I2S pinout。
5. 根据 `kPinHpCon` 判断输出路径。
6. `kPinPcmXsmt = HIGH`，解除 PCM5102A 静音。
7. 调用 `audio.connecttoFS(SD_MMC, path)`。
8. 进入播放控制页。

输出路径规则：
- `kPinHpCon == LOW`：耳机已插入，`kPinNsCtrl = LOW`，关闭扬声器。
- `kPinHpCon == HIGH`：耳机未插入且正在播放，`kPinNsCtrl = HIGH`，启用扬声器。

### 4.4 播放中 tick
`MusicService::tick()` 负责：
- 调用 `audio.loop()`。
- 低频检测 `kPinHpCon`，处理耳机插拔。
- 更新播放状态。
- 上报 UI 是否需要刷新。

耳机插拔处理：
- 插入耳机：立即 `kPinNsCtrl = LOW`。
- 拔出耳机且仍在播放：`kPinNsCtrl = HIGH`。
- 不在 UI 渲染函数里读取耳机检测脚。

### 4.5 暂停和停止
- 暂停：调用播放库暂停/继续接口；第一版可短时间保留音频硬件，保证恢复速度。
- 停止：停止播放、关闭文件、静音 PCM5102A、关闭 NS4150B。

### 4.6 退出音乐列表
用户从音乐列表页返回 Music 子菜单时：
1. 停止播放。
2. 停止调用 `audio.loop()`。
3. 关闭当前音频文件。
4. `kPinPcmXsmt = LOW`。
5. `kPinNsCtrl = LOW`。
6. `kPinAudioLdoEn = LOW`。
7. 卸载 SD。
8. 清理 MusicService 内部播放状态。

## 5. SD 扫描和索引

### 5.1 路径约定
第一版固定使用：

```txt
/music
```

第一版严格只支持小写 `/music`，不兼容 `/Music`。

第一版固定索引文件：

```txt
/music/.openbread_music.idx
```

第一版不递归扫描 `/music` 子文件夹。

收藏歌曲使用独立索引文件：

```txt
/music/.openbread_favorites.idx
```

用户收藏歌曲后，将歌曲记录写入收藏索引文件。收藏页只读取收藏索引，不扫描收藏目录。

### 5.2 索引文件用途
- 保存歌曲路径、文件名、文件大小、格式。
- 支持分页读取。
- 避免 UI 每次翻页重新扫描目录。
- 避免把大量歌曲全部放进 RAM。

### 5.3 索引写入规则
- 扫描成功后重写索引。
- 扫描失败时不覆盖旧索引。
- 写入时先写临时文件，再替换正式索引，避免掉电造成半文件。

建议文件：

```txt
/music/.openbread_music.tmp
/music/.openbread_music.idx
```

### 5.4 索引格式
推荐定长二进制格式，便于按页 seek 读取。

索引头：

```cpp
struct MusicIndexHeader {
  char magic[4];          // "OBMI"
  uint16_t version;       // 1
  uint16_t recordSize;
  uint32_t recordCount;
  uint8_t truncated;
  uint8_t reserved[15];
};
```

索引记录：

```cpp
static constexpr uint8_t kMaxNameLen = 64;
static constexpr uint8_t kMaxPathLen = 96;

enum class MusicFormat : uint8_t {
  Unknown,
  Mp3,
  M4a,
  Aac,
  Wav,
  Flac,
  Opus,
  Ogg,
  Oga
};

struct MusicIndexRecord {
  char path[kMaxPathLen];
  char name[kMaxNameLen];
  uint32_t sizeBytes;
  MusicFormat format;
};
```

第一版暂时固定使用 `kMaxNameLen = 64` 和 `kMaxPathLen = 96`。

### 5.5 分页读取
推荐：

```cpp
static constexpr uint8_t kMusicRowsPerPage = 5;
```

分页规则：
- UI 只读取当前页记录。
- 可缓存当前页、上一页、下一页。
- `Up/Down` 移动焦点。
- 焦点跨页时读取目标页索引记录。
- 底部显示 `当前序号/总数` 和 `当前页/总页`。

## 6. 模块落点

### 6.1 `src/bsp/DacDriver.h/.cpp`
职责：
- 初始化音频相关 GPIO。
- 控制 `kPinAudioLdoEn`。
- 控制 `kPinPcmXsmt`。
- 控制 `kPinNsCtrl`。
- 读取 `kPinHpCon`。
- 设置 I2S pinout。
- 提供音频硬件开启、静音、关闭接口。

不负责：
- 歌曲列表。
- 索引文件。
- 页面状态。
- 播放列表业务。

### 6.2 `src/services/SdCardService.h/.cpp`
职责：
- 挂载 SD。
- 刷新 SD 状态。
- 卸载 SD。
- 向 `MusicService` 提供 SD 是否可用。

### 6.3 `src/services/MusicService.h/.cpp`
职责：
- 扫描 `/music`。
- 写入 `/music/.openbread_music.idx`。
- 按页读取索引记录。
- 保存当前选中歌曲和播放状态。
- 调用 `ESP32-audioI2S` 播放文件。
- 调用 `DacDriver` 控制音频硬件。
- 提供 `tick()` 推进播放。
- 退出音乐列表时释放音频和 SD。

### 6.4 `src/ui/pages/MusicPage.h/.cpp`
职责：
- 渲染音乐列表页。
- 渲染播放控制页。
- 渲染空状态、扫描中、SD 错误状态。
- 解释页面级输入。
- 不直接访问 SD_MMC、Audio、GPIO。

### 6.5 `src/ui/UiManager.h/.cpp`
职责：
- 持有 `MusicPage` 和 `MusicService`。
- 在进入音乐详情页时触发扫描。
- 将 `Up/Down/OK/Left` 分发给 MusicPage/MusicService。
- 离开音乐列表时调用 MusicService 释放资源。

## 7. UI 和按键

### 7.1 页面结构
```txt
Home
  -> Music
    -> 音乐列表
      -> 音乐列表页
      -> 播放控制页
    -> 收藏歌曲
      -> 后续实现
```

第一版优先完成 `音乐列表`。

### 7.2 音乐列表页
示例：

```txt
[音乐列表]                 SD
> 001 song_name_01.mp3
  002 song_name_02.wav
  003 song_name_03.flac
  004 song_name_04.m4a
  005 song_name_05.ogg

3/28                    1/6
```

状态：
- `Scanning`：`正在扫描音乐...`
- `SdMissing`：`SD卡未插入`
- `SdError`：`SD卡读取失败`
- `Empty`：`未找到音乐文件`
- `Ready`：显示分页列表

长文件名处理：
- 非焦点行裁剪显示。
- 焦点行使用局部横向滚动显示完整文件名。
- 文件名滚动不得改变行高、页脚位置或焦点框尺寸。

### 7.3 播放控制页
示例：

```txt
[正在播放]

song_name_01.mp3

00:42 / 03:15
[==========      ]

> 暂停
  下一首
  上一首
  音量 60%
  停止
```

播放页第一版必须显示总时长。如果播放库无法可靠取得某个文件的总时长，该文件应显示播放失败或不进入播放页，避免播放页出现不完整信息。

### 7.4 按键规则
- `Up/Down`
  - 列表页：移动焦点，跨页时加载上一页/下一页。
  - 播放页：移动操作焦点。
- `OK`
  - 列表页：播放当前歌曲并进入播放页。
  - 播放页：执行当前操作。
- `Left`
  - 播放页：返回列表页。
  - 列表页：退出音乐列表，释放音频并卸载 SD，返回 Music 子菜单。
- `Right`
  - 第一版不使用。

播放页操作项：
- `暂停` / `继续`
- `下一首`
- `上一首`
- `音量`
- `停止`

## 8. 低功耗要求
- 进入 Music 子菜单不启用音频硬件。
- 进入音乐列表才挂载 SD。
- 开始播放才开启音频 LDO 和 I2S。
- 暂停超过设定时间后，记录当前播放时间，然后自动关闭音频硬件。
- 暂停超时关闭后，继续播放时按记录的播放时间恢复。
- 停止播放或退出音乐列表必须关闭音频 LDO。
- 退出音乐列表必须卸载 SD。
- 不做后台播放。
- 不在 UI 每帧中执行 SD 扫描或硬件检测。

## 9. 日志要求

硬件和 SD 相关日志必须可通过宏关闭。

推荐前缀：
- `[SD]`
- `[AUDIO]`
- `[ERR]`

关键日志：
- `[SD] mount begin`
- `[SD] mount ok`
- `[SD] mount failed`
- `[SD] music scan begin path=/music`
- `[SD] music scan done count=...`
- `[SD] music index write begin path=/music/.openbread_music.idx`
- `[SD] music index write done count=...`
- `[SD] unmount after music exit`
- `[AUDIO] power on ldo=1 xsmt=1`
- `[AUDIO] output=headphone ns=0`
- `[AUDIO] output=speaker ns=1`
- `[AUDIO] init i2s din=40 lrck=39 bck=41`
- `[AUDIO] play file=...`
- `[AUDIO] pause`
- `[AUDIO] resume`
- `[AUDIO] stop reason=...`
- `[AUDIO] power off ldo=0 xsmt=0 ns=0`
- `[ERR] audio open failed path=...`

## 10. 实现顺序

### Step 1：补 SD 卸载能力
- 在 `SdCardService` / `SdCardDriver` 中增加卸载接口。
- 确保退出音乐列表能调用卸载。

### Step 2：实现 DacDriver
- 初始化音频 GPIO。
- 实现音频供电开关。
- 实现 PCM5102A 静音控制。
- 实现耳机检测。
- 实现 NS4150B 控制。

### Step 3：实现 MusicService 索引
- 扫描 `/music`。
- 过滤支持扩展名。
- 写入临时索引。
- 替换正式索引。
- 按页读取索引。

### Step 4：接入 ESP32-audioI2S
- 持有 `Audio` 对象。
- 设置 I2S pinout。
- 实现播放、暂停、继续、停止。
- 在 `tick()` 中调用 `audio.loop()`。

### Step 5：实现 MusicPage
- 列表页。
- 播放页。
- 状态页。
- 分页焦点逻辑。

### Step 6：接入 UiManager
- 进入音乐列表时扫描。
- 分发按键。
- 退出时释放资源。
- 渲染音乐详情页。

### Step 7：编译和实机验证
- 每次改动后执行 `pio run`。
- 上机验证 SD、PCM5102A、耳机检测和 NS4150B。

## 11. 验证清单

### 11.1 编译验证
- 执行：

```txt
pio run
```

- 汇报：
  - 编译结果
  - RAM 占用
  - Flash 占用

### 11.2 逻辑验证
- 进入音乐列表才挂载 SD。
- 扫描只读取 `/music`。
- 扫描完成后生成 `/music/.openbread_music.idx`。
- 列表分页来自索引文件。
- `Up/Down` 焦点和分页正确。
- `OK` 能播放当前歌曲。
- `Left` 从播放页返回列表页。
- `Left` 从列表页退出并释放资源。
- 退出音乐列表后 SD 已卸载。
- UI 不会每帧扫描 SD。

### 11.3 硬件验证
- `kPinAudioLdoEn` 拉高后音频供电正常。
- `kPinAudioLdoEn` 拉低后音频供电关闭。
- `kPinPcmXsmt` 拉高后 PCM5102A 不静音。
- `kPinPcmXsmt` 拉低后 PCM5102A 静音。
- 耳机插入时 `kPinHpCon` 为低。
- 耳机插入时 `kPinNsCtrl` 必须拉低。
- 耳机拔出且正在播放时 `kPinNsCtrl` 拉高。
- 退出音乐列表时 `kPinNsCtrl` 拉低。
- PCM5102A 能正常播放 SD 卡文件。

## 12. 剩余待确认项
- 音量档位如何映射到 `ESP32-audioI2S::setVolume()`。
