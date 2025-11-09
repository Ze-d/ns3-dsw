# ns3-dsw

| 日期/版本        | 更新内容   | 更新人 |
| ---------------- | ---------- | ------ |
| 2025.10.17  V0.0 | 网络层仿真 | zjy    |
|                  |            |        |
|                  |            |        |

# 环境构建

# 编译构建

```bash

# 1.1 渲染图片
dot -Tpng scratch/topo.dot -o scratch/topo.png
# 1.2 动态图片【todo】
NetAnim 载入文件：scratch/topo_figure.xml
# 2.1 使用csv自带的delay
./ns3 run "scratch/topo_figure_flowmon_cfg \
  --nodes=scratch/nodes.csv \
  --links=scratch/links.csv \
  --delayByDist=0 \
  --stop=25 \
  --anim=1 \
  --animXml=scratch/topo_figure.xml \
  --dot=scratch/topo.dot \
  --statsCsv=scratch/flowstats.csv \
  --flowXml=scratch/flowmon.xml \
  --log=debug"


```

# git workflow

**master分支受保护，短周期分支修改**

## 1.开发流程

```bash
# 第一次拉代码
git clone git@github.com:<org>/<repo>.git
cd <repo>
git pull --ff-only

# 1. 新建短分支（从 main 开）
git switch -c feature/hello-readme

# 2. 代码开发

# 3. 提交并推到远端
git add .
git commit -m " " # commit标准见备注
git push -u origin feature/hello-readme

```

## 2.PR流程

1. 打开 GitHub 仓库，会看到黄色条提示 "Compare & pull request"，点它。
2. 确认 `base: main`，`compare: feature/hello-readme`。
3. 填标题和描述（例如在描述里写 `Closes #1` 关联 issue）。
4. 选择 **Reviewers**（把 Alice 勾上），点 **Create pull request**。
5. 等 CI 变绿、处理评论，必要时继续在该分支提交并 push，PR 会自动更新。
6. 审核通过后，点 **Squash and merge** → **Delete branch**（删除远端分支）。

## 3.合并后本地操作

```bash
# 回到 main，拉最新
git switch master
git pull --ff-only

# 删本地已合并的短分支
git branch -d feature/hello-readme

# （可选）删远端短分支，如果 PR 没自动删：
git push origin --delete feature/hello-readme

```

## 4.附录

### commit标准

只使用[feature]/[fix]/[doc]/[other]
