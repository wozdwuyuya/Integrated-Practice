# 纠错账本 (Error Log)

## 禁止提交超过 100MB 的单文件

GitHub 单文件上限 100MB，推送会被 pre-receive hook 拒绝。

## 已知事故：1.5GB 工具链误入库

**现象**：`HiSpark_Toolchain` 目录（含 193MB zip、大量 .whl/.exe）被提交到 git 历史，导致 `git push` 持续失败，即使后续 commit 删除了该目录，历史对象仍存在。

**根因**：`.gitignore` 在文件已被 track 后才添加，git 不会自动取消跟踪已入库的文件。

**应对方案**：
1. **预防**：首次 `git init` 后立即配置 `.gitignore`，在 `git add .` 前生效。
2. **修复（历史污染）**：使用 `git filter-branch` 或 `git filter-repo` 从全部历史中移除大文件，再 force push。
3. **修复（核弹方案）**：删除 `.git` 目录重新 `git init`，彻底清除历史，适合单人项目。
4. **验证**：push 前运行 `git ls-files | xargs ls -la | sort -k5 -n -r | head -20` 检查最大文件。
