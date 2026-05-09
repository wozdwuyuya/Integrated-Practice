# Dynamic Skill Management Protocol

在执行任何任务的 [Thinking Process] 阶段，必须先核对以下 5 个核心 Skill 的状态，并严格根据条件进行动态开关：

## Skill 清单与状态

### 1. Locode (算力调度员)
- 默认状态：**ON（常驻）**
- 作用：作为底层拦截器，将简单的日志解释、基础提问路由到 `E:\AI_Models` 下的本地模型以降低成本。
- 触发条件：始终激活，无需触发词。

### 2. Claude-Hud (状态监视器)
- 默认状态：**ON（常驻）**
- 作用：在终端底部显示 Token 消耗和当前激活的 Skill 状态。
- 触发条件：始终激活，无需触发词。

### 3. Superpowers (纪律执行官)
- 默认状态：**OFF**
- 作用：强制执行 TDD 和原子化修改 (Atomic Changes)。
- 触发条件（满足任一即激活）：
  - 用户 Prompt 中出现以下文件扩展名：`.c`, `.cpp`, `.h`, `.hpp`, `.s`, `.S`
  - 用户 Prompt 中出现以下嵌入式关键词：`内存分配`, `malloc`, `free`, `星闪`, `SLE`, `寄存器`, `register`, `中断处理`, `ISR`, `NVIC`, `DMA`, `外设驱动`, `HAL`, `RTOS`, `任务调度`
  - 用户明确要求编写或重构 C/C++ 核心代码
- 卸载条件：代码修改完成并通过初步测试后，立即关闭。

### 4. Bug Fixer / 深度自审 Skill (Reflexion)
- 默认状态：**OFF**
- 作用：执行 Reflexion 反思链，对高风险底层代码进行对抗性审查，定位 Root Cause。
- 触发条件（满足任一即激活）：
  - 用户 Prompt 中出现以下错误关键词：`Error`, `WHEA`, `死机`, `段错误`, `Segmentation fault`, `panic`, `HardFault`, `BusFault`, `UsageFault`, `堆栈溢出`, `stack overflow`, `内存泄漏`, `memory leak`
  - 用户明确说"找不出bug"、"死活找不到原因"、"不知道哪里错了"
  - 用户执行 `git commit` 前的代码审查阶段
- 卸载条件：找出 Root Cause 并输出报告后，立即关闭。

### 5. 官方文档写作 Skill
- 默认状态：**OFF**
- 作用：提炼和更新项目 Readme 或接口文档。
- 触发条件（满足任一即激活）：
  - 代码完全跑通，用户进入更新文档阶段
  - 用户 Prompt 中出现：`写文档`, `更新 README`, `接口文档`, `API 文档`
- 卸载条件：文档生成完毕后，立即关闭。

---

## 【人类最高指令】

1. **强制覆写**：如果用户的 Prompt 包含 `[Force ON: Skill名]` 或 `[Force OFF: Skill名]`，无条件覆写上述所有自动判断规则，强制执行人工意图。

2. **手动状态查询**：如果用户输入 `[Check Skills]` 或询问"当前开了什么技能/插件"，必须跳过所有常规思考，直接以列表形式打印出当前被激活的 Skill 状态，格式如下：

```
=== Skill Status Report ===
[ON]  Locode        - 算力调度员 (常驻)
[ON]  Claude-Hud    - 状态监视器 (常驻)
[OFF] Superpowers   - 纪律执行官
[OFF] Bug Fixer     - 深度自审
[OFF] Doc Writer    - 文档写作
============================
```
