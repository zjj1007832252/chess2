# 中国象棋 (Chinese Chess) — C++ / Qt 6

一个使用 C++17 和 Qt 6 编写的单机中国象棋游戏，支持三种玩法：

- **人机对战** — 带 Alpha-Beta 剪枝的搜索引擎，可调节难度（简单/普通/困难）。
- **好友联机** — 基于 TCP 的房间制：一方“创建房间（主机）”，另一方“加入房间”。
- **本地双人** — 两人共用一台电脑轮流操作，棋盘每回合自动翻转视角。

## 目录结构

```
chess/
├── CMakeLists.txt       CMake 构建配置
├── main.cpp             程序入口
├── board.h / board.cpp  棋盘模型：走子生成、将军/将杀/困毙判定
├── ai.h / ai.cpp        引擎：Negamax + Alpha-Beta + 局面估值
├── boardview.h/.cpp     棋盘绘制与鼠标输入（QWidget）
├── networkmanager.h/.cpp TCP 联机：主机/加入 + 行/JSON 协议
├── gamemodedialog.h/.cpp 模式选择对话框
├── mainwindow.h / .cpp  主控制器：菜单、状态栏、走子、悔棋、聊天
└── build/               构建产物（chess.exe + Qt 运行时）
```

## 构建说明

依赖：MinGW-w64 g++ 与 Qt 6（需在系统 PATH 中）。

在 **cmd**（或 PowerShell）中，确保 Qt 和 MinGW 在 PATH 里：

```bat
set PATH=C:\mingw64\bin;C:\Qt\6.x.x\mingw_64\bin;%PATH%
```

然后使用 CMake 构建：

```bat
cd chess
mkdir build && cd build
cmake -G "MinGW Makefiles" ..
mingw32-make -j4
```

构建完成后，`chess.exe` 和 Qt 运行时 DLL 位于 `build/deploy/` 目录下。

## 玩法

启动后弹出模式选择窗：

- **与人机对战**：选择难度与执子方（红方先手 / 黑方后手）。
- **创建联机房间（主机）**：选择端口；窗口会显示本机局域网 IP，把它和端口告诉好友。主机执红方、先走。
- **加入联机房间**：输入好友的 IP 与端口；加入者执黑方。
- **本地双人对战**：两人轮流点击走子，棋盘每回合翻转。

操作：先点己方棋子（绿色高亮 + 走法提示点），再点目标格完成走子。
菜单“游戏”提供 **新游戏 / 悔棋 / 认输**；右侧面板显示走子记录、被吃棋子与聊天框（联机时可用）。

## 联机协议

简单的、面向行的 JSON（每条消息以 `\n` 结尾）：

```
{"t":"move","fr":7,"fc":4,"tr":7,"tc":4}   走子
{"t":"chat","text":"你好"}                  聊天
{"t":"resign"}                              认输
{"t":"rematch"}                             再来一局
{"t":"hello","name":"玩家"}                  通报昵称
```

主机即 TCP 服务端，加入者即客户端；连接建立后双方是对等收发。

## 设计要点

- **棋盘模型**与界面解耦：`Board` 是纯数据结构，提供伪合法走子与“是否使己方将处于将军”的判定，`generateLegalMoves` 在其上过滤。
- **将帅照面**（飞将）规则在 `inCheck` 中处理。
- **AI** 在 `QtConcurrent` 工作线程里思考，主界面不卡顿；通过 `QFutureWatcher` 回收选定的走法。
- **视角翻转**：联机/人机时本方始终在下方；本地双人模式每回合翻转到当前行棋方。
