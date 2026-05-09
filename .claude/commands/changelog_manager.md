# Changelog Manager - 更新日志自动同步 Skill

> 本 Skill 在代码修复 (fix)、重构 (refactor) 或功能更新 (feat) 类的 Git Commit 执行完毕后自动激活。

## 触发时机

满足以下任一条件时激活：

1. 用户完成 `git commit` 且 commit message 前缀为 `fix:`、`refactor:`、`feat:` 之一
2. 用户执行 `/project:sync`（推送前强制检查）
3. 用户明确说"更新 CHANGELOG"、"同步日志"、"记录变更"

## 执行逻辑

### 第一步：收集变更信息

执行以下命令获取自上一个版本 tag（或首个 commit）以来的所有变更：

```bash
# 获取最近一次 tag，若无 tag 则使用首个 commit
LAST_TAG=$(git describe --tags --abbrev=0 2>/dev/null || git rev-list --max-parents=0 HEAD)

# 获取自上次 tag 以来的所有 commit 列表
git log ${LAST_TAG}..HEAD --oneline --no-merges

# 获取变更文件统计
git diff --stat ${LAST_TAG}..HEAD

# 获取变更文件列表（按类型分类）
git diff --name-status ${LAST_TAG}..HEAD
```

### 第二步：分类变更内容

根据 commit message 前缀将变更归入 Keep a Changelog 标准分类：

| Commit 前缀 | CHANGELOG 分类 | 说明 |
|-------------|---------------|------|
| `fix:` | `### Fixed` | Bug 修复，需标注严重级别（P0/P1/P2） |
| `feat:` | `### Added` | 新增功能或文件 |
| `refactor:` | `### Changed` | 重构、迁移、架构调整 |
| `docs:` | `### Changed` | 文档更新（归入 Changed 而非 Added） |
| `chore:` | `### Changed` | 构建/工具变更 |
| `test:` | `### Added` | 新增测试 |

### 第三步：生成 CHANGELOG 条目

在 `CHANGELOG.md` 的 `## [Unreleased]` 章节下追加新内容，格式如下：

```markdown
## [Unreleased] - YYYY-MM-DD

### 变更主题：一句话概括本次更新

> commit 范围：`旧commit`..`新commit`，共 N 个提交

---

### Fixed

#### 缺陷标题（commit `hash`）

**P0 级（必须修复）：**

| 缺陷 | 文件 | 修复方案 |
|------|------|---------|
| 缺陷描述 | `filename.c` | 修复方案简述 |

**P1 级（建议修复）：**

| 缺陷 | 文件 | 修复方案 |
|------|------|---------|
| 缺陷描述 | `filename.c` | 修复方案简述 |

---

### Changed

#### 变更标题（commit `hash`）

- 变更点 1
- 变更点 2

**涉及文件：** N 个文件，+X / -Y 行

---

### Added

#### 新增内容标题（commit `hash`）

- 新增项 1
- 新增项 2
```

### 第四步：写入文件

使用 Edit 工具将生成的内容插入 `CHANGELOG.md`，位于 `## [Unreleased]` 标题之后、上一次记录内容之前。

**关键规则：**
- 不要覆盖已有内容，只在 `[Unreleased]` 章节顶部追加
- 保持 `[Unreleased]` 和 `[历史记录]` 两个大区块的分隔
- 每次追加的内容必须包含日期标记

## 硬件/SDK 兼容性说明要求

当变更涉及以下类别时，**必须**在 CHANGELOG 条目中附加兼容性说明：

| 变更类别 | 必须包含的兼容性信息 |
|---------|-------------------|
| 引脚定义修改 | 列出旧引脚 → 新引脚映射，标注开发板型号 |
| I2C/SPI 地址变更 | 列出旧地址 → 新地址，标注影响的设备 |
| 新增传感器/外设 | 标注挂载的总线、地址、GPIO 引脚 |
| SDK API 变更 | 标注旧 API → 新 API，说明 SDK 版本要求 |
| CMakeLists.txt 修改 | 说明新增/删除的源文件、include 路径 |
| Kconfig 配置变更 | 说明新增配置项的默认值和用途 |

格式示例：

```markdown
**硬件兼容性：**
- 引脚变更：RGB LED R/G/B 从 GPIO13/14/2 改为 GPIO6/7/8（共阳接法）
- 新增设备：SSD1315 OLED (I2C 0x3C) 挂载于 I2C1 (SCL=GPIO15, SDA=GPIO16)
- SDK 要求：需 bearpi-pico_h3863 SDK，编译命令 `./build.py ws63-liteos-app`
```

## 联动机制

### /project:sync 前置检查

当用户执行 `/project:sync` 时，**必须**在推送前执行以下检查：

```bash
# 检查自上次 CHANGELOG 更新以来是否有新的 fix/refactor/feat commit
git log --oneline $(git log -1 --format=%H -- CHANGELOG.md)..HEAD | grep -E '^[a-f0-9]+ (fix|refactor|feat):'
```

如果有匹配的 commit，**必须**在推送前提示用户：

```
检测到自上次 CHANGELOG 更新以来有 N 个代码变更 commit：
  - fix: xxx
  - feat: xxx
  - refactor: xxx

是否需要同步更新 CHANGELOG.md？
  [Y] 更新后再推送
  [N] 跳过，直接推送
```

用户选择 Y 时，执行完整的 CHANGELOG 更新流程后再推送。
用户选择 N 时，跳过更新直接推送。

### commit 后自动触发

当用户完成 `git commit` 且 commit message 匹配 `fix:`/`refactor:`/`feat:` 时，自动输出：

```
[Changelog Manager] 检测到代码变更 commit，建议同步更新 CHANGELOG.md。
输入 /project:changelog_manager 即可触发自动更新。
```

## Skill 状态报告格式

当用户查询 `[Check Skills]` 时，本 Skill 显示为：

```
[OFF] Changelog Mgr  - 日志同步
```

默认状态为 **OFF**，仅在触发条件下临时激活，更新完成后立即关闭。

## 卸载条件

CHANGELOG.md 更新完毕并确认内容准确后，本 Skill 自动关闭。
