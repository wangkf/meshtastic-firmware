# InkHUD

这是一些可能对开发人员有帮助的杂乱笔记集合。

<img src="disclaimer.jpg" width="250" alt="自谦的梗图" />

---

- [目的](#目的)
- [设计原则](#设计原则)
  - [自包含](#自包含)
  - [静态](#静态)
  - [非交互式](#非交互式)
  - [可定制](#可定制)
  - [事件驱动渲染](#事件驱动渲染)
  - [避免预处理](#避免预处理)
- [实现方式](#实现方式)
- [渲染流程](#渲染流程)
- [核心概念](#核心概念)
  - [NicheGraphics框架](#nichegraphics框架)
  - [NicheGraphics电子墨水驱动](#nichegraphics电子墨水驱动)
  - [InkHUD小程序](#inkhud小程序)
- [添加变体](#添加变体)
  - [platformio.ini](#platformioini)
  - [nicheGraphics.h](#nichegraphicsh)
- [字体](#字体)
  - [解析Unicode文本](#解析unicode文本)
  - [本地化](#本地化)
  - [创建/修改](#创建修改)
- [类说明](#类说明)
  - [`InkHUD::InkHUD`](#inkhudinkhud)
  - [`InkHUD::Persistence`](#inkhudpersistence)
  - [`InkHUD::Persistence::Settings`](#inkhudpersistencesettings)
  - [`InkHUD::Persistence::LatestMessage`](#inkhudpersistencelatestmessage)
  - [`InkHUD::WindowManager`](#inkhudwindowmanager)
  - [`InkHUD::Renderer`](#inkhudrenderer)
  - [`InkHUD::Renderer::DisplayHealth`](#inkhudrendererdisplayhealth)
  - [`InkHUD::Events`](#inkhudevents)
  - [`InkHUD::Applet`](#inkhudapplet)
  - [`InkHUD::SystemApplet`](#inkhudsystemapplet)
  - [`InkHUD::Tile`](#inkhudtile)
  - [`InkHUD::AppletFont`](#inkhudappletfont)

## 目的

InkHUD是一个适用于电子墨水设备的最小化UI。它以尽可能静态的方式显示用户选择的信息，以最小化显示屏刷新次数。

它旨在作为已连接客户端应用的补充。

## 设计原则

### 自包含

- 将InkHUD代码保存在`/src/graphics/niche/InkHUD`内。
- 将可重用组件放在`/src/graphics/niche`中，以便其他UI可以利用。
- 使用**模块API**、**观察者模式**和其他类似的非侵入式钩子与固件代码交互。

### 静态

信息应该以尽可能静态的方式显示。应避免不必要的更新。

例如，使用固定时间戳而不是`X秒前`的标签，因为后者需要不断更新才能保持最新状态。

### 非交互式

InkHUD的目标是作为一个"平视显示器"。用户的意图是快速浏览显示内容。而不是频繁地与显示器交互。

一些交互是作为达到目的的手段而被容忍的：显示器**应该**是可定制的，但这应该尽可能地最小化。

_编辑：由于对键盘支持的需求很大，最终可能需要添加某种自由文本功能，尽管这违背了原始设计原则。_

### 可定制

用户应该有权选择他们想要接收的信息以及接收方式。

### 事件驱动渲染

显示图像不会"自动"更新。各个小程序负责决定何时有新信息要显示，然后请求显示更新。

### 避免预处理

**不要**使用预处理宏来编写针对个别设备的代码。

**请**在[`nicheGraphics.h`](#nichegraphicsh)中配置InkHUD以适应每个设备。

**请**使用预处理宏来保护所有文件

- InkHUD文件使用`#ifdef MESHTASTIC_INCLUDE_INKHUD`
- 可重用组件（驱动程序等）使用`#ifdef MESHTASTIC_INCLUDE_NICHE_GRAPHICS`

## 实现方式

- 变体的platformio.ini文件继承`inkhud`（在InkHUD/PlatformioConfig.ini中定义）
  - 原始屏幕类被抑制：`MESHTASTIC_EXCLUDE_SCREEN`
  - ButtonThread被抑制：`HAS_BUTTON=0`
  - 包含NicheGraphics组件：`MESHTASTIC_INCLUDE_NICHE_GRAPHICS`
  - 包含InkHUD组件：`MESHTASTIC_INCLUDE_INKHUD`
- `main.cpp`
  - 包含`nicheGraphics.h`（来自变体文件夹）
  - 调用`setupNicheGraphics`（来自nicheGraphics.h）
- `nicheGraphics.h`
  - 包含InkHUD组件
  - 包含共享的NicheGraphics组件
  - `setupNicheGraphics`
    - 配置和连接组件
    - `inkhud->begin`

## 渲染流程

（动画图表）

<img src="rendering.gif" alt="InkHUD渲染的动画流程图表" height="480" width="480" />

概述：

- 组件调用`requestUpdate`（仅限小程序）或`InkHUD::forceUpdate`
- `Renderer`使用`Renderer::runOnce`为下一个loop()调度渲染周期
- `Renderer`确定更新请求是否有效
- `Renderer`要求相关小程序进行渲染
- 小程序尺寸被更新（通过Applet的`Tile`）
- 小程序生成像素输出，并将其传递给它们的`Tile`
- Tiles将这些"相对"像素移至其真实区域，用于多路复用
- Tiles将像素传递给`Renderer`
- `Renderer`对像素应用全局显示旋转
- `Renderer`将像素组合成最终图像
- 最终图像被传递给显示驱动程序，开始物理更新过程

## 核心概念

### NicheGraphics框架

InkHUD作为一个_NicheGraphics_ UI实现。

旨在作为实现自包含UI的模式/理念，以适应各种特殊设备，这些设备最好由自己的自定义用户界面提供服务。

假设示例：电子墨水屏、1602 LCD、小型OLED、智能手表等

一个NicheGraphics UI：

- 是自包含的
- 利用`/src/graphics/niche`文件夹中收集的松散资源集合（驱动程序、输入方法等）
- 实现`setupNicheGraphics()`方法

### NicheGraphics电子墨水驱动

InkHUD使用一组自定义电子墨水驱动程序。这些不是基于GxEPD2或任何其他代码库。它们直接在Meshtastic固件的基础上编写，利用OSThread类进行异步显示更新。

与驱动程序的交互很简单。InkHUD生成一帧1位图像数据。此图像数据与要使用的刷新类型（FULL或FAST）一起传递给驱动程序。

`driver->update(uint8_t* buffer, EInk::UpdateTypes::FULL)`

有关更多信息，请参阅`src/graphics/niche/Drivers/EInk`中的文档。

### InkHUD小程序

InkHUD小程序是一个为显示屏生成信息屏幕的类。

例如：`DMApplet.h`（显示最近的直接消息）和`RecentsList.h`（显示最近听到的节点列表）

- 小程序是模块化的：它们易于编写，易于实现。用户使用菜单选择他们想要的小程序。
- 小程序使用响应式设计。它们应该适应不同的屏幕/布局/字体。
- 小程序决定何时更新。它们使用Module API、Observers等获取信息，并在有有趣内容显示时请求显示更新。

请参阅`src/graphics/niche/InkHUD/Applets/Examples`中的示例代码。

#### 编写小程序

您的新小程序类将继承`InkHUD::Applet`。

```cpp
class BasicExampleApplet : public Applet
{
  public:
    // 您必须有一个onRender()方法
    // 所有绘图都在这里进行

    void onRender() override;
};
```

`onRender`方法在显示图像重新绘制时调用。这可能在任何时间发生，所以请随时准备好！

```cpp
// 所有绘图都在这里进行
// 我们的基本示例不做任何有用的事情。它只是被动地打印一些文本。
void InkHUD::BasicExampleApplet::onRender()
{
    printAt(0, 0, "Hello, world!");
}
```

您的小程序需要自动缩放，以适应各种屏幕/布局/字体。确保您的绘制是相对于小程序的大小的。

| 边缘   | 坐标      | 简写     |
| ------ | --------- | -------- |
| 左     | 0         | `X(0.0)` |
| 上     | 0         | `Y(0.0)` |
| 右     | `width()` | `X(1.0)` |
| 下     | `height()` | `Y(1.0)` |

对于绘制文本也适用相同的原则。`AppletFont::lineHeight`和`getTextWidth`等方法在这里很有用。

```cpp
std::string line1 = "Line 1";
printAt(0, Y(0.5), line1);
drawRect(0, Y(0.5), getTextWidth(line1), fontSmall.lineHeight(), BLACK);
```

您的小程序只会在_某些东西_请求显示更新时重新绘制。当您的小程序确定它有新信息要显示时，欢迎通过调用`requestUpdate`来请求显示更新。

您如何确定这一点，取决于您的小程序实际做什么。下面是一个示例小程序的代码片段。当收到新消息时，小程序请求更新。

```cpp
// 我们配置了Module API在收到新文本消息时调用此方法
ProcessMessage InkHUD::NewMsgExampleApplet::handleReceived(const meshtastic_MeshPacket &mp)
{

    // 如果小程序完全停用，则中止
    // 不要浪费时间：无论如何我们都不会被渲染
    if (!isActive())
        return ProcessMessage::CONTINUE;

    // 检查这是否是传入消息
    // 传出消息（由我们发送）也会调用handleReceived

    if (!isFromUs(&mp)) {
        // 存储发件人的节点编号
        // 我们需要保留此信息，以便在调用render()时重用它
        haveMessage = true;
        fromWho = mp.from;

        // 告诉InkHUD我们有新的东西要在屏幕上显示
        requestUpdate();
    }

    // 告诉Module API继续通知其他固件组件有关此消息
    // 我们不是唯一对新文本消息感兴趣的组件
    return ProcessMessage::CONTINUE;
}
```

#### 实现小程序

将您的新小程序整合到InkHUD中很容易。

在变体的`nicheGraphics.h`中：

- `#include` 您的小程序
- `inkhud->addApplet("My Applet", new InkHUD::MyApplet);`

您需要将这些行添加到将使用您的小程序的任何变体中。

#### 小程序基类

如果您需要创建几个类似的小程序，创建一个可重用的基类可能是有意义的。其中一些已经存在于`src/graphics/niche/InkHUD/Applets/Bases`中，但请谨慎使用这些，因为它们可能会在将来被修改。

#### 系统小程序

到目前为止，我们一直在讨论"用户小程序"。我们还认识到另一类"系统小程序"。它们处理菜单和启动屏幕等内容。这些通常需要特殊处理，需要手动实现。

## 添加变体

在`/variants/<YOUR_VARIANT>/`中：

### platformio.ini

继承`inkhud`，然后与您的硬件变体所需的任何其他platformio配置组合。

_（示例仅显示InkHUD所需的配置。这不是完整的`env`定义。）_

```ini
[env:YOUR_VARIANT-inkhud]
extends = esp32s3_base, inkhud ; 或 nrf52840_base 等

build_src_filter =
${esp32_base.build_src_filter}
${inkhud.build_src_filter}

build_flags =
${esp32s3_base.build_flags}
${inkhud.build_flags}

lib_deps =
${inkhud.lib_deps} ; InkHUD库优先，这样我们得到GFXRoot而不是AdafruitGFX
${esp32s3_base.lib_deps}
```

### nicheGraphics.h

应该包含一个`setupNicheGraphics`方法，该方法为InkHUD创建和配置各种组件。

有关注释完善的示例，请参阅：

- `/variants/heltec_vision_master_e290/nicheGraphics.h`（ESP32）
- `/variants/ELECROW-ThinkNode-M1/nicheGraphics.h`（NRF52）

概述：

- 显示器
  - 启动SPI
  - 创建显示驱动程序
- InkHUD
  - 创建InkHUD实例
  - 设置电子墨水快速刷新限制（`setDisplayResilience`）
  - 设置字体
  - 设置默认用户设置
  - 选择要构建的小程序（`addApplet`）
  - 启动InkHUD
- 按钮
  - 设置`TwoButton`驱动程序（用户按钮，可选的"辅助"按钮）
  - 连接到InkHUD处理程序（使用lambda表达式）

## 字体

InkHUD使用AdafruitGFX字体。所有小程序都可以使用三种共享字体（小、中、大）。这些在nicheGraphics.h中按变体设置。

```cpp
// 准备字体
InkHUD::Applet::fontLarge = FREESANS_12PT_WIN1252;
InkHUD::Applet::fontMedium = FREESANS_9PT_WIN1252;
InkHUD::Applet::fontSmall = FREESANS_6PT_WIN1252;

// 使用通用AdafruitGFX字体替代：
// InkHUD::Applet::fontLarge = FreeSerif18pt7b;
```

可以使用任何通用AdafruitGFX字体，但随InkHUD捆绑的字体已通过扩展ASCII字符集和表情符号进行了自定义。

### 解析Unicode文本

固件接收的文本以UTF-8编码。

小程序必须手动解析可能包含非ASCII字符的任何文本。应解析文本消息和节点名称等字符串。

```cpp
std::string greeting = "Góðan daginn!";
std::string parsed = parse(greeting);
```

这将重新编码字符以匹配InkHUD构建时使用的扩展ASCII字体。

有限的表情符号集已[嵌入字体中的未使用代码点](#emoji)。

### 本地化

InkHUD捆绑了扩展ASCII字体，支持：

- Windows-1250（中欧）
- Windows-1251（西里尔文）
- Windows-1252（西欧）

默认构建使用Windows-1252编码。这可以在nicheGraphics.h中更改。

```cpp
InkHUD::Applet::fontLarge = FREESANS_12PT_WIN1250;
InkHUD::Applet::fontMedium = FREESANS_9PT_WIN1250;
InkHUD::Applet::fontSmall = FREESANS_6PT_WIN1250;

InkHUD::Applet::fontLarge = FREESANS_12PT_WIN1251;
InkHUD::Applet::fontMedium = FREESANS_9PT_WIN1251;
InkHUD::Applet::fontSmall = FREESANS_6PT_WIN1251;
```

### 创建/修改

对于基本转换和编辑，在线工具可能就足够了：

- [https://rop.nl/truetype2gfx/](https://rop.nl/truetype2gfx/) - 从ttf转换
- [https://tchapi.github.io/Adafruit-GFX-Font-Customiser/](https://tchapi.github.io/Adafruit-GFX-Font-Customiser/) - 编辑字形

对于重度编辑，建议使用以下离线工作流程：

- [FontForge](https://fontforge.org/en-US/)
  - 重新排序字形
    - 编码 > 加载编码
    - 编码 > 重新编码
  - .ttf到.bdf转换
    - 元素 > 可用位图
    - 文件 > 生成字体
- [GFXFontEditor](https://github.com/ScottFerg56/GFXFontEditor)
  - 手动字形修正
  - .bdf到AdafruitGFX .h转换
    - 文件 > 编辑字体属性
    - 右键单击字形列表，展平字体
    - 文件 > 另存为
    - 手动编辑导出的.h
      - 删除`#include <AdafruitGFX.h>`

如果可能，自定义的扩展ASCII字体应使用InkHUD已支持的编码之一。如果不可能，则需要添加新编码的映射。

有关使用扩展ASCII字体的详细信息，请参见[编码](#编码)。

## 类说明

### `InkHUD::InkHUD`

_`src/graphics/niche/InkHUD/InkHUD.h`_

- 单例
- InkHUD其他组件之间的中介

#### `getInstance()`

获取对该类的访问权限。
第一次`getInstance`调用实例化该类及其子类：

- `InkHUD::Persistence`
- `InkHUD::WindowManager`
- `InkHUD::Renderer`
- `InkHUD::Events`

为了方便，许多InkHUD组件在`begin`时调用此方法，并将其存储为`InkHUD* inkhud`。

---

### `InkHUD::Persistence`

_`src/graphics/niche/InkHUD/Persistence.h`_

将InkHUD数据存储在闪存中

- 设置
- 最近接收到的文本消息（广播和DM）

在极少数情况下，小程序可能会单独存储自己的特定数据（例如`ThreadedMessageApplet`）

数据仅在关闭/重新启动时保存。如果意外断电，数据将不会保存。

---

### `InkHUD::Persistence::Settings`

_`src/graphics/niche/InkHUD/Persistence.h`_

与InkHUD相关的设置。大部分是用户的自定义，但有些值记录UI的状态（例如`tips.safeShutdownSeen`）

- 使用`FlashData.h`（共享的Niche Graphics工具）存储
- 不编码为protobufs
- 直接序列化为结构体的字节

#### 默认值

全局默认值在定义结构体时设置（Persistence.h）。
变体特定的默认值通过在`setupNicheGraphics()`期间修改settings实例的值来设置，在调用`inkhud->begin`之前。

```cpp
inkhud->persistence->settings.userTiles.count = 2;
inkhud->persistence->settings.userTiles.maxCount = 4;
inkhud->persistence->settings.rotation = 3;
```

通过在此时修改值，如果我们无法从闪存加载先前的设置（尚未保存、旧版本等），将使用这些值。

---

### `InkHUD::Persistence::LatestMessage`

_`src/graphics/niche/InkHUD/Persistence.h`_

最近接收到的文本消息

- 最近的DM
- 最近的广播

收集在这里，以便各种用户小程序不必都存储自己的此信息副本。

我们无法为此目的使用`devicestate.rx_text_message`，因为：

- 它会被传出的文本消息清除
- 我们希望同时存储最近的广播和最近的DM

#### 保存/加载

_有点hack..._
使用`InkHUD::MessageStore`存储到闪存，它实际上是为存储消息线程而设计的（请参阅`ThreadedMessageApplet`）。使用它是因为它比`FlashData.h`更有效地存储字符串。

hack如下：

- 如果最近的消息是DM，我们只存储DM。
- 如果最近的消息是广播，我们同时存储DM和广播。DM可能是空字符串。

---

### `InkHUD::WindowManager`

_`src/graphics/niche/InkHUD/WindowManager.h`_

管理显示哪些小程序及其大小/位置（通过操作"瓦片"）

- 拥有`Tile`实例
- 创建和销毁瓦片；设置大小和位置：
  - 启动时
  - 运行时，当配置更改时（布局、旋转等）
- 激活（或停用）小程序
- 循环切换小程序（例如按下按钮时）

窗口管理器不处理像素；这由`InkHUD::Tile`对象处理。

注意：某些方法（包括`changeLayout`、`changeActivatedApplets`）本身不会触发更改。它们应该在修改`inkhud->persistence->settings`中的相关值_之后_调用。

---

### `InkHUD::Renderer`

_`src/graphics/niche/InkHUD/Renderer.h`_

从小程序（通过瓦片）获取像素输出，合并，并传递给驱动程序。

- 由`requestUpdate`或`forceUpdate`触发
- 不立即运行：允许多个小程序共享一个渲染周期
- 为相关小程序调用`Applet::onRender`
- 应用全局旋转
- 将最终图像传递给驱动程序

`requestUpdate`适用于小程序（用户或系统）。如果小程序可见，渲染器将执行请求。`forceUpdate`可在任何地方使用，但请不要从小程序使用。

#### 异步更新

`requestUpdate`和`forceUpdate`不会阻塞代码执行。它们使用`Renderer::runOnce`将渲染调度为"尽快"。然后渲染器从小程序获取像素输出，并将组装好的图像交给驱动程序。驱动程序的更新过程也是异步的。如果在调用`requestUpdate`或`forceUpdate`时驱动程序正忙，将尽快运行另一个渲染。这由`Renderer::runOnce`处理。

#### 阻塞更新

如果需要，调用`forceUpdate`时使用可选参数`async=false`等待更新运行（> 1秒）。此外，`awaitUpdate`方法可用于阻塞直到任何先前的更新完成。一个使用示例是等待绘制关机屏幕。

#### 全局旋转

InkHUD小程序的确切大小/位置/旋转由用户配置。为实现这一点，小程序在0,0和`Applet::width()`、`Applet::height()`之间绘制像素。

- **缩放**：渲染开始前，小程序的`width()`和`height()`由`Tile`设置
- **平移**：`Tile`将小程序像素向上/向下/向左/向右移动
- **旋转**：`Renderer`在将所有接收到的像素放入最终图像缓冲区之前对其进行旋转

---

### `InkHUD::Renderer::DisplayHealth`

_`src/graphics/niche/InkHUD/DisplayHealth.h`_

负责通过优化FAST与FULL刷新的比例来维护显示器健康

- 计数FAST与FULL刷新的次数（债务）
- 建议使用FAST或FULL类型
- 如果需要，定期无提示地进行FULL刷新

#### 背景信息

当电子墨水显示器上的图像更新时，可以使用不同的过程将像素移动到新状态。我们定义了两个过程：`FAST`和`FULL`。

`FAST`更新直接将像素从旧位置移动到新位置。这在美学上令人愉悦，并且速度快，但_对显示硬件来说具有挑战性_。如果过度使用，像素会积累残余电荷，这会对显示器的寿命和图像质量产生负面影响。

`FULL`更新首先让所有像素在黑色和白色之间移动，然后让它们最终停留在最终位置。这会导致显示图像不愉快地闪烁，但对显示健康和图像质量最好。

大多数显示器只要偶尔执行`FULL`更新，就很容易容忍`FAST`更新。这个`FULL`更新的频率取决于显示器型号。

#### 债务

`InkHUD::DisplayHealth`记录自上次`FULL`刷新以来发生的`FAST`刷新次数。

这被称为"完全刷新债务"。

如果请求/强制特定类型（`FULL`/`FAST`）的更新，这将被批准。

如果请求/强制更新_不指定_类型（`UpdateTypes::UNSPECIFIED`），`DisplayHealth`将选择`FAST`或`FULL`，以尝试维持快速更新与完全更新的目标比率。

这个目标在`nichegraphics.h`中设置时通过`InkHUD::setDisplayResilience`设置。

如果连续执行_过多_的`FAST`刷新，`DisplayHealth`将开始人为地增加完全刷新债务。这将导致接下来的几个`UNSPECIFIED`更新_全部_作为`FULL`执行，同时偿还债务。

这种"完全刷新债务"系统允许我们在用户交互期间通过容忍显示器上的额外压力来提高感知响应性，并尝试在用户交互停止后"修复损害"。

#### 维护

"完全刷新债务"系统假设显示器将在用户交互期间执行许多`UNSPECIFIED`类型的更新。根据网格流量/小程序选择的数量，情况可能并非如此。

如果债务特别高，并且没有自然发生更新，`DisplayHealth`将开始不频繁地执行`FULL`更新，纯粹是为了偿还完全刷新债务。

---

### `InkHUD::Events`

处理通常影响InkHUD系统的事件（例如关闭、按钮按下）。

小程序本身也会单独监听各种事件，但目的是收集它们想要显示的信息。

#### 按钮

按钮输入有时由系统小程序处理。`InkHUD::Events`确定按钮是应由特定系统小程序处理，还是应触发默认行为。

#### 恢复出厂设置

Events类处理触发恢复出厂设置的管理消息。我们设置`Events::eraseOnReboot = true`，这导致`Events::onReboot`擦除InkHUD数据目录的内容。我们这样做是因为一些小程序（例如ThreadedMessageApplet）将自己的数据保存到闪存，所以如果我们更早擦除，这些数据会在重启期间被重写。

---

### `InkHUD::Applet`

小程序的基类。小程序是一个"程序"，可能在显示器上显示信息。

简单来说，所有InkHUD的"底层"代码只存在于支持小程序。小程序是真正向用户显示有用信息的部分。这个基类公开了编写小程序所需的功能。

#### 绘图方法

`Applet`实现了大多数AdafruitGFX绘图方法。文本处理是个例外。应该使用`printAt`、`printWrapped`和`printThick`代替。这些旨在更加方便，但它们也实现了支持外文字母的字符替换系统。

`Applet`还添加了绘制在InkHUD中常用的几个设计元素的方法。

#### InkHUD事件

小程序会经历许多状态变化：用户激活/停用，用户按钮按下导致前景/背景切换等。`Applet`类提供了一组虚拟方法，小程序可以覆盖这些方法来适当处理这些事件。

`onRender`虚拟方法就是一个例子。当小程序被渲染时调用，应执行所有绘图代码。小程序_必须_实现此方法。

#### 响应式设计

小程序的大小将根据屏幕大小和用户的布局（多路复用）而变化。在调用`onRender`之前，小程序的尺寸会立即更新，因此`width()`和`height()`将给出所需的大小。小程序应相对于这些值绘制其图形元素。还提供了`X(float)`和`Y(float)`方法以方便使用。

| 边缘   | 坐标      | 简写     |
| ------ | --------- | -------- |
| 左     | 0         | `X(0.0)` |
| 上     | 0         | `Y(0.0)` |
| 右     | `width()` | `X(1.0)` |
| 下     | `height()` | `Y(1.0)` |

对于绘制文本也适用相同的原则。`AppletFont::lineHeight`和`getTextWidth`等方法在这里很有用。

小程序应该始终相对于其左上角在_x=0, y=0_处绘制。小程序的像素由InkHUD::Tile自动移动到屏幕上的正确位置。

#### 用户小程序

用户小程序是"普通"小程序，每个都向用户显示特定的信息集。它们可以在运行时使用屏幕菜单激活/停用。例如`DMApplet.h`和`PositionsApplet.h`。用户小程序不应与InkHUD代码的较低层交互。

用户小程序在变体的`setupNicheGraphics`方法中实例化，并传递给`InkHUD::addApplet`。它们的类不应在其他地方提及，以便在变体未实现特定小程序时可以在编译期间剥离其代码。用户小程序的内部处理仅将它们视为通用的`Applet`类型。

#### 激活/停用

用户小程序可以被激活或停用。这在运行时会改变：用户使用屏幕菜单选择应该激活哪些小程序。小程序在停用状态下不应处理数据。它可以取消观察任何可观察者，忽略`handleReceived`调用等。

小程序可以实现虚拟`onActivate`和`onDeactivate`方法来处理这种状态变化。它可以通过调用`isActive`在内部检查此状态。

系统小程序不能被停用。

#### 前景/背景

激活的小程序可以是_前景_或_背景_。前景小程序是在屏幕更新时将渲染到瓦片的小程序。背景小程序不会被绘制。当用户按下按钮时发生的小程序循环是使用前景/背景实现的。

无论它是前景还是背景，激活的小程序都应该继续收集/处理数据，并在有新信息显示时请求更新。这是因为_autoshow_机制可能会将背景小程序带到前景以显示其数据。如果小程序保持在背景，其更新请求将被安全地忽略。

#### Autoshow

Autoshow是一项功能，允许用户选择他们希望自动显示的小程序（如果有）。如果为小程序启用了autoshow，当它有新信息显示时，它将被带到前景。用户使用屏幕菜单按小程序授予此权限。如果事件导致小程序被自动显示，则不应对同一事件显示NotificationApplet。

小程序需要决定何时有值得自动显示的信息。它通过在通常的`requestUpdate`调用之外调用`requestAutoshow`来发出信号。

---

### `InkHUD::SystemApplet`

_系统小程序_是具有特殊角色的小程序，需要特殊处理。例如`BatteryIconApplet.h`和`LogoApplet.h`。这些是在`WindowManager.h`中手动一个一个实现的。

这个类是`Applet`的轻微扩展。它为一些仅限于系统小程序的特殊功能添加了额外的标志：独占使用显示器和处理用户输入。拥有单独的系统小程序类还允许我们在代码中明确处理系统小程序而不是用户小程序的时候。

我们将这些引用存储为`vector<SystemApplets*>`。这与我们对待用户小程序的方式相似，使渲染变得方便。
由于系统小程序确实有独特的角色，有时我们需要与特定的小程序交互。我们不从额外的引用集中获取它们，而是从`vector<SystemApplet*>`中访问它们。使用`InkHUD::getSystemApplet`通过其`Applet::name`值访问小程序，然后进行类型转换。

---

### `InkHUD::Tile`

瓦片表示显示器的一个区域。瓦片控制小程序的大小和位置。

小程序要渲染，必须分配给瓦片。当小程序分配给瓦片时，两者变得链接。小程序知道瓦片；瓦片知道小程序。小程序不能共享瓦片；分配不同的小程序会删除任何现有的链接。

在小程序渲染之前，其宽度和高度被设置为瓦片的尺寸。在`onRender`期间，小程序的绘图方法在_x=0, y=0_和_x=Applet::width(), y=Applet::height()_之间生成像素。这些像素被传递给其瓦片的`Tile::handleAppletPixel`方法。然后瓦片应用x和y偏移，将这些像素"平移"到显示器的瓦片区域。这些平移后的像素然后传递给`InkHUD::Renderer`。

![瓦片平移小程序像素的示意图](./tile_translation.png)

#### 用户瓦片

_用户小程序_是"普通"小程序。它们可以在运行时使用屏幕菜单激活/停用。用户小程序渲染到**用户瓦片**之一。

用户可以使用屏幕菜单自定义"布局"。根据他们选择的布局，会创建一定数量的_用户瓦片_。这些瓦片自动定位和调整大小，以便填满整个屏幕。

通常，用户启用的小程序数量会超过他们拥有的瓦片数量。按下用户按钮将循环浏览这些小程序。旧的小程序被发送到_背景_，新的小程序被带到_前景_。当用户小程序被带到前景时，它被分配给用户瓦片（焦点瓦片）。当它渲染时，其大小将由该瓦片设置，其像素将被平移到该瓦片的区域。被发送到背景的用户小程序失去其分配；它不再有分配的瓦片。

#### 焦点瓦片

焦点瓦片是用户瓦片之一。这是当用户按下按钮时其小程序会更改的瓦片。这也是长按菜单会出现的瓦片。焦点瓦片通过其在`vector<Tile*> userTiles`中的索引来识别。

#### 高亮

除了用户按钮外，一些设备还有第二个"辅助按钮"。此按钮的功能可能因设备而异，但有时用于聚焦不同的瓦片。发生这种情况时，新聚焦的瓦片会通过绘制边框临时"高亮"。此边框在几秒后自动移除。由于绘图代码只能由小程序执行，因此这种高亮是`Tile`和`Applet`之间的协作：在`Applet::render`中执行，在虚拟`onRender`方法已经运行之后。

高亮仅在辅助按钮触发`nextTile`时使用。如果通过屏幕菜单执行，则不会发生高亮。

#### 系统瓦片

_系统小程序_是具有特殊角色的小程序，需要特殊处理。例如`BatteryIconApplet.h`和`LogoApplet.h`。_大多数情况下_，这些小程序不会渲染到用户瓦片。相反，它们被赋予自己独特的瓦片，手动定位/调整尺寸。我们保留的这些特殊瓦片的唯一引用存储在链接的系统小程序中。可以使用`Applet::getTile`访问它们。

---

### `InkHUD::AppletFont`

扩展AdafruitGFX字体功能的包装器。

#### 尺寸信息

AppletFont类预先计算了一些关于字体尺寸的信息，这对设计很有用（`AppletFont::lineHeight`），并用于支持InkHUD的自定义文本处理。

默认的AdafruitGFX文本处理将字符"放在一条线上"，就像手写在一张有横线的纸上一样。`InkHUD::AppletFont`测量字体的字符集，以便我们改为绘制固定高度的文本行，通过边界框定位，具有可选的水平和垂直对齐。

![InkHUD vs AdafruitGFX中的文本原点](./appletfont.png)

此框的高度是`AppletFont::lineHeight`，即字体中最高字符的高度。这给我们提供了一个文本的固定高度，比AdafruitGFX的默认行间距更紧凑。

#### 编码

AppletFont可以从标准的7位ASCII AdafruitGFX字体构造，但是InkHUD也支持8位扩展ASCII字体。

为此，在实例化AppletFont时必须指定编码。

```cpp
InkHUD::AppletFont(FreeSans9pt_Win1250, InkHUD::AppletFont::WINDOWS_1250);
```

当前支持的编码有：

- ASCII
- Windows-1250（中欧）
- Windows-1251（西里尔文）
- Windows-1252（西欧）

要添加对其他编码的支持，请在`AppletFont::Encodings`枚举中添加，然后在`AppletFont::applyEncoding`中定义从unicode的映射。

#### 自定义行高

一些字体可能有少数特别高的字符，尤其是带有变音符号的扩展ASCII字体。理想情况下，应该修改字体来帮助解决这个问题，但如果问题仍然存在，可以在构造函数中指定对自动确定的行高的手动偏移。

```cpp
// 上方-2 px的填充，下方+1 px的填充
InkHUD::AppletFont(FreeSans9pt7b, ASCII, -2, 1);
```

#### 表情符号

AdafruitGFX字体限制为255个字符。InkHUD支持一组受限的表情符号，它们存储在ASCII控制字符的未使用代码点中（`'\x01'`、`'\x02'`等）。

标准AdafruitGFX字体不包含低于`'\x20'`的字形，因此会忽略这些尝试解析表情符号的操作。

表情符号到控制字符的映射是相当任意的。选择受到[PR #3940 OLED屏幕表情符号](https://github.com/meshtastic/firmware/pull/3940)和[表情符号频率电子表格](https://docs.google.com/spreadsheets/d/1Zs13WJYdZL1pNZP0dCIXkWau_tZOjK3mmJz0KNq4I30/)的影响。

| 代码点 | 表情符号                                      |
| ------ | --------------------------------------------- |
| ~~`0x00`~~ | （空终止符，未使用）                          |
| `0x01`     | 👍                                            |
| `0x02`     | 👎                                            |
| `0x03`     | 🙂                                            |
| `0x04`     | 😆                                            |
| `0x05`     | 👋                                            |
| `0x06`     | ☀                                            |
| ~~`0x07`~~ | （铃字符，未使用）                            |
| `0x08`     | 🌧                                            |
| `0x09`     | ☁                                            |
| ~~`0x0A`~~ | （换行符，未使用）                            |
| `0x0B`     | ♥                                            |
| `0x0C`     | 💩                                            |
| ~~`0x0D`~~ | （回车符，未使用）                            |
| `0x0E`     | 🔔                                            |
| `0x0F`     | 😭                                            |
| `0x1A`     | （替换字符"⍰"，用于不可打印字符）            |
| `0x1B`     | 🤗                                            |
| `0x1C`     | 😉                                            |
| `0x1D`     | 😏                                            |
| `0x1E`     | 🫡（敬礼表情）                                |
| `0x1F`     | 👌                                            |