请执行以下 Git 同步流程：

1. 运行 `git status` 和 `git diff` 分析我刚才所做的所有代码修改。
2. 运行 `git add .` 将修改加入暂存区。
3. 根据分析结果，用中文严格按照 Conventional Commits 规范（如 feat:, fix:, docs:, chore:）生成一段清晰的 Commit Message，向组员解释我改了什么。
4. 运行 `git commit -m` 提交代码。
5. 成功后，在终端用高亮字体提醒我：'代码已在本地 Commit 完毕，请检查无误后手动输入 git push origin main 推送到远程仓库！'

**注意**：当前仓库已通过 `.gitignore` 自动排除大型二进制文件（包括 `*.zip`、`*.exe`、`*.7z`、`HiSpark_Toolchain/` 等），提交前请确认不会意外包含超过 100MB 的文件，否则 GitHub 将拒绝推送。

如果传入了参数，请将其作为补充说明融入 Commit Message：$ARGUMENTS
