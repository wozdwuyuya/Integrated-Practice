# POST_MORTEM_AND_TEMPLATES.md

> BearPi-H3863 Smart Health Monitor | 工程资产萃取文档
> 封存日期：2026-05-12 | 最后提交：`d2effd6`
> 项目仓库：https://github.com/wozdwuyuya/Integrated-Practice.git

---

## 一、经验平移：从 I2C 总线到异步并发的降维打击心法

### 1.1 本次踩坑全景

本项目在 `lib/system/i2c_master.c` 和 `lib/app/health_monitor_main.c` 中完成了三轮
I2C 总线安全改造。核心矛盾：两个 RTOS 线程（MainTask 优先级 17、TCPServerTask 优先级 12）
共享一条物理 I2C 总线，挂载 OLED(0x3C) 和 MPU6050(0x68) 两个设备。

踩坑序列：

| 阶段 | 问题 | 根因 | 解法 |
|------|------|------|------|
| 初始设计 | 无总线保护 | 缺乏并发意识 | 全局 `osMutexId_t g_i2c_bus_mutex` |
| Plan B | `osWaitForever` 永久阻塞 | 总线挂死时锁永远不释放 | 500ms 超时 + GPIO 脉冲恢复 |
| Plan A | OLED 刷屏期间 MPU6050 延迟抖动 | 锁粒度为单次 I2C 事务 | `_locked` 变体，整个刷屏持锁 |
| Plan A 二次踩坑 | 同线程重复加锁死锁 | CMSIS Mutex 非递归 | 底层 `i2c_write_raw` 绕过内部锁 |
| Plan C | TCP 推送 JSON 数据撕裂 | 无跨线程数据快照保护 | 独立 `g_data_mutex` |

### 1.2 通用并发避坑指南（前端 / Python / 任意异步场景）

以下原则从嵌入式 Mutex 实战中提炼，适用于任何存在并发访问的系统：

#### 原则 1：锁的粒度决定性能与安全的平衡点

```
嵌入式教训：OLED 单事务锁导致刷屏被 MPU6050 穿插，延迟抖动 40ms
前端映射：  React setState 在高频事件中被拆分，UI 出现中间态闪烁
Python映射：asyncio 中 await 点导致共享状态被其他协程修改
```

**规则**：锁的范围应覆盖一个完整的"逻辑原子操作"，而非单个底层调用。
在前端中，这意味着批量 DOM 更新应在一个 `requestAnimationFrame` 或
`unstable_batchedUpdates` 内完成。在 Python asyncio 中，共享状态的
读-改-写序列不应在中间插入 `await`。

#### 原则 2：永远不要信任"永远等待"

```
嵌入式教训：osWaitForever + 总线挂死 = 系统冻结
前端映射：  await fetch() 无超时 = 用户界面永久 Loading
Python映射：socket.recv() 无 timeout = 线程泄漏
```

**规则**：任何等待外部资源的操作都必须有超时和降级路径。
`i2c_master_lock()` 从 `osWaitForever` 改为 500ms 超时 + 恢复重试，
等价于前端的 `AbortController` + 重试逻辑，等价于 Python 的
`asyncio.wait_for(coro, timeout=5.0)`。

#### 原则 3：警惕"锁套锁"——确认你的锁是否可重入

```
嵌入式教训：update_oled_display() 外层加锁 + ssd1306_SendData() 内层加锁 = 死锁
前端映射：  嵌套的 useSelector/useDispatch 在 concurrent mode 下的 tearing
Python映射：threading.Lock() 不可重入，RLock() 才是递归锁
```

**规则**：在加锁前，必须审计被调用链中是否已有锁操作。
如果无法确认，要么使用可重入锁（ReentrantMutex / RLock），
要么提供 `_nolock` / `_internal` 变体函数供已持锁的调用方使用。
本项目的 `ssd1306_UpdateScreen_locked()` 就是这一策略的产物。

#### 原则 4：数据快照与数据操作分离

```
嵌入式教训：TCP 线程读取 g_heart_rate 时主线程正在更新，JSON 数据撕裂
前端映射：  Redux 中间件读取 state 时 dispatch 正在进行 reducer
Python映射：多线程读写 dict 导致 RuntimeError: dictionary changed size
```

**规则**：生产者和消费者共享数据时，要么用锁保护读写窗口（本项目方案），
要么使用不可变快照（Copy-on-Write）。
在前端中，React 的 `useDeferredValue` 和 Zustand 的 `subscribe` 就是
快照思想的体现。在 Python 中，`queue.Queue` 和 `asyncio.Queue` 是
线程安全的生产者-消费者原语。

---

## 二、下一代 Agent 宪法模板

> 以下模板已去除 Hi3863 硬件特异性描述，保留通用工程纪律。
> 可直接复制到任意项目的 `CLAUDE.md` 中使用。

```markdown
# Claude Code Agent Execution Protocol (Universal)

## 0. Project Map (Key Paths)
*说明：这是架构师定义的核心工作区。除非得到特殊指令，否则探索范围仅限于此。*
- 工作区根目录: `<ROOT>`
- 应用入口 (Entry): `<ENTRY_FILE>`
- 核心业务逻辑 (Core): `<CORE_MODULE>`
- 只读参考 (Reference - DO NOT TOUCH): `<REFERENCE_DIR>`

## 1. 角色定位 (Role)
你是一个极其严谨的高级软件工程师。核心职责：精准执行指令、分析本地源码、
严格按规范修改代码并保证构建零报错。

## 2. 核心工作流 (Agent Execution Protocol)

### 2.1 Explore First
未读取源码前严禁给出建议。优先查阅 Project Map 定位。

### 2.2 Pre-computation
修改前必须在脑海模拟对构建配置（CMakeLists / package.json / pyproject.toml）
或依赖项的影响。

### 2.3 Atomic Changes
修改必须原子化。顺序：修改逻辑 -> 修改配置 -> 更新文档 -> 独立 Commit。
**单次 Commit 只解决一个问题。** 多个问题 = 多个 Commit。

### 2.4 Tool-Loop（反思循环）
修改后严禁干等。主动搜索测试或要求开发者验证，遇错触发 Reflexion：
1. 复现错误
2. 定位根因（不是症状）
3. 修复根因
4. 验证修复不引入新问题

### 2.5 Plan Mode 协作协议
- 架构师（用户）定义目标和约束
- 执行者（Agent）输出 Plan，等待批准后才能写代码
- Plan 阶段只读不写，修改阶段严格按 Plan 执行

## 3. 防崩溃绝对红线

### 3.1 拒绝幻觉
第三方库无此函数时严禁捏造 API。必须先 `grep` 或读文档确认。

### 3.2 并发与内存安全
- 数组操作必查越界
- `malloc/free` 必成对
- 共享资源必配互斥锁，锁必须有超时
- 锁嵌套前必须审计调用链是否已持锁

### 3.3 反懒惰禁止占位符
修改代码绝对禁止使用 `// ... existing code ...` 或 `/* 略 */`。
必须输出完整函数块，保护单点真实源。这是对代码库的尊重，
也是对审查者的负责。

## 4. 高危操作阻断
- 只读目录严禁写入
- `git rm`、`rm -rf`、覆盖构建配置必须请求人工授权

## 5. 输出格式
- **[Thinking Process]**：探勘、拆解、反思
- **[Action Plan]**：文件路径 + 预期改动
- **[Final Answer]**：核心代码块
- **极致降噪**：汇报极度简练，严禁客套话

## 6. 文档与日志管理
- 增量追加更新，严禁大面积覆盖
- 连续 Commit 或完成 Phase 后，主动读取 `git log` 提炼更新日志
```

---

## 三、单兵作战工程反思

### 3.1 为何"一人 Carry"时原子化提交是生命线

当一个开发者同时扮演架构师、执行者、测试者三个角色时，
最大的敌人不是技术难度，而是**上下文丢失**。

本次项目的真实案例：

1. Plan B 提交 `a58a40c` 后，我立即开始 Plan A。
   如果 Plan A 引入了回归 bug，`git bisect` 可以精确指向 `8aa7fb2`。
   如果我把 B 和 A 混在一个 Commit 里，排查范围将扩大 2 倍。

2. Plan A 开发过程中发现了同线程死锁问题（`ssd1306_SendData` 内部锁）。
   这个发现直接影响了实现方案——从"调用方加锁"改为"驱动层 `_locked` 变体"。
   原子化提交让这个决策过程被完整保留在 Commit message 中，
   未来的维护者可以理解**为什么**有 `_locked` 后缀的函数。

**教训**：单兵作战时，Commit 就是你的外部记忆。
每个 Commit 应该是一个自洽的、可回滚的、有明确意图的变更单元。

### 3.2 本次工程化改造的纪念碑

以下文件是本次安全加固的核心战场，记录于此供未来参考：

| 文件 | 角色 | 关键改造 |
|------|------|----------|
| `lib/system/i2c_master.c` | I2C 总线管理 | 超时机制 + 运行时总线恢复 |
| `lib/output/ssd1306.c` | OLED 驱动 | `_locked` 变体规避同线程死锁 |
| `lib/output/ssd1306.h` | OLED 头文件 | 新增 `ssd1306_UpdateScreen_locked` 声明 |
| `lib/app/health_monitor_main.c` | 业务主循环 | 数据快照互斥锁 + OLED 持锁刷屏 |
| `lib/app/health_monitor_main.h` | 业务头文件 | `data_lock`/`data_unlock` 公共接口 |

### 3.3 项目生命周期数据

```
总提交数：     15+
开发阶段：     Phase 1 (Sensors) → Phase 2 (WiFi AP) → Phase 3 (TCP Server) → Security Hardening
代码行数：     ~3000 行 C（lib/ 目录）
外设驱动：     MPU6050, SSD1306, KY-039, SW-420, RGB LED, Buzzer, Vibration Motor
通信协议：     SLE (星闪) + WiFi TCP (SoftAP)
Android 客户端：Java/Gradle, 6 个 Activity
开发工具链：    Python serial monitor + AI engine
```

---

## 四、项目封存检查清单

- [x] I2C 总线安全：超时 + 恢复 + 持锁刷屏
- [x] 跨线程数据同步：`g_data_mutex`
- [x] CHANGELOG.md 已更新至最新状态
- [x] 所有代码提交已推送到 main 分支
- [x] 工程模板已萃取至本文档
- [x] 远程仓库指向 `https://github.com/wozdwuyuya/Integrated-Practice.git`

**项目状态：FROZEN**

下一步赛道：跨平台 App 与电商 AI 工具开发。
