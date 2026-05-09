# 团队协作规范 (CONTRIBUTING.md)

> 本规范由主程制定，适用于本仓库所有贡献者。违反红线的 PR 将被打回。

---

## 1. 仓库上传纪律

### 红线：以下文件严禁入库

| 类别 | 禁止上传的文件 | 原因 |
|------|---------------|------|
| 编译产物 | `*.o`, `*.bin`, `*.elf`, `*.out`, `*.map`, `*.hex` | 每次编译都会重新生成，污染历史 |
| SDK 工具链 | `HiSpark_Toolchain/`, `*.zip`, `*.7z`, `*.exe` | 单个文件可能超过 100MB，GitHub 会直接拒绝推送 |
| 编译缓存 | `out/`, `build/`, `dist/`, `.ninja*` | 编译中间产物，无版本控制价值 |
| 敏感信息 | `.env`, `*.key`, `*.pem`, `secrets/` | API Key 泄露 = 安全事故 |

### 每次 commit 前的自检清单

```bash
# 1. 查看哪些文件将被提交
git status

# 2. 确认没有大文件混入（超过 10MB 的文件要警惕）
git ls-files | xargs ls -la 2>/dev/null | sort -k5 -n -r | head -10

# 3. 确认无误后再 add
git add <你要提交的文件>

# 4. 绝对不要无脑 git add . 或 git add -A
```

### 我们的 `.gitignore` 已经做了什么

项目根目录的 `.gitignore` 已配置了以下排除规则：

- 所有编译产物（`.o`, `.bin`, `.elf`, `.out`, `.map`, `.hex`）
- SDK 工具链目录（`HiSpark_Toolchain/`）
- 压缩包（`.zip`, `.7z`, `.exe`）
- 编译缓存（`out/`, `build/`, `dist/`）
- 敏感文件（`.env`, `*.key`, `*.pem`）

**但 `.gitignore` 只能拦截未跟踪的文件。** 如果某个文件已经被 `git add` 过再加入 `.gitignore`，git 不会自动取消跟踪。所以关键还是靠你自己在 commit 前 `git status` 检查。

> 历史教训：项目初期曾有 1.5GB 工具链被误提交，导致 `git push` 持续失败。前车之鉴，不可重蹈。

---

## 2. 分支与合并策略

### 严禁直接在 main 分支开发

`main` 分支是稳定分支，只包含经过验证的代码。所有新功能开发必须在个人分支上进行。

### 分支命名规范

| 前缀 | 用途 | 示例 |
|------|------|------|
| `feature/` | 新功能开发 | `feature/wifi-sta-connect`, `feature/heart-rate-algo` |
| `fix/` | Bug 修复 | `fix/uart-baudrate`, `fix/ble-disconnect` |
| `docs/` | 文档更新 | `docs/readme-update`, `docs/api-spec` |
| `test/` | 测试相关 | `test/unit-adc`, `test/integration-sle` |

### 合并流程

```bash
# 1. 从最新的 main 拉取个人分支
git checkout main
git pull origin main
git checkout -b feature/你的功能名

# 2. 在个人分支上开发、提交
git add <文件>
git commit -m "feat: 实现 xxx 功能"

# 3. 合并前：切回 main 拉取最新，再合并你的分支
git checkout main
git pull origin main          # 拉取队友的最新代码
git checkout feature/你的功能名
git rebase main               # 将你的提交嫁接到最新 main 上

# 4. 本地编译验证（必须通过才能推送）
# 在开发板编译环境中执行：
./build.py ws63-liteos-app -j2

# 5. 编译通过后，推送分支并发起 PR
git push origin feature/你的功能名
```

### 合并前的硬性要求

- 本地必须先 `git pull origin main` 拉取最新代码
- 必须在本地完成编译，确认无报错
- 不允许"先推上去再说"的心态

---

## 3. 代码修改原则

### 原子化提交 (Atomic Changes)

一次 commit 只做一件事。拆分标准：

| 场景 | 正确做法 | 错误做法 |
|------|---------|---------|
| 既改了 WiFi 连接又改了心率算法 | 拆成 2 个 commit | 混在 1 个 commit 里 |
| 修了 Bug 顺手重构了函数名 | 拆成 1 个 fix + 1 个 refactor | 混在一起 |
| 新增功能 + 更新文档 | 拆成 1 个 feat + 1 个 docs | 混在一起 |

### Commit Message 规范

严格遵循 Conventional Commits 格式：

```
<类型>: <简短描述>

<可选的详细说明>
```

| 类型 | 用途 | 示例 |
|------|------|------|
| `feat` | 新功能 | `feat: 实现 WiFi STA 模式连接` |
| `fix` | 修复 Bug | `fix: 修复 UART 波特率配置错误` |
| `docs` | 文档更新 | `docs: 补充北向传输 API 说明` |
| `refactor` | 重构（不改变功能） | `refactor: 拆分 wifi_connect 为独立模块` |
| `chore` | 构建/工具变更 | `chore: 更新 .gitignore 排除编译产物` |
| `test` | 测试相关 | `test: 添加 ADC 采样单元测试` |

---

## 4. 硬件与编译环境对齐

### 统一开发环境

所有组员必须使用相同版本的 SDK 和工具链，避免"我这边能编译你那边不行"的问题。

| 项目 | 要求 |
|------|------|
| 芯片 | 海思 Hi3863 (ws63) |
| SDK | 小熊派 bearpi-pico_h3863 SDK |
| 编译命令 | `./build.py ws63-liteos-app -j2` |
| 工具链 | SDK 自带，不要单独安装其他版本 |

### 环境一致性检查

如果编译报错，先检查以下几点：

1. SDK 版本是否与仓库一致
2. 代码是否已同步到 SDK 的 `application/samples/` 目录下
3. Kconfig 配置是否正确（`./build.py menuconfig ws63-liteos-app`）

---

## 5. 代码审查清单

提交 PR 前，自查以下项目：

- [ ] 编译通过，无 warning（或已知 warning 已标注原因）
- [ ] 没有提交编译产物或大文件
- [ ] Commit message 符合规范
- [ ] 每个 commit 是原子的
- [ ] 新增的对外接口有头文件声明
- [ ] 硬件相关代码（寄存器、中断、DMA）有注释说明用途

---

> 规范不是束缚，是保护。遵守规范的团队，才能把精力花在真正有价值的开发上。
