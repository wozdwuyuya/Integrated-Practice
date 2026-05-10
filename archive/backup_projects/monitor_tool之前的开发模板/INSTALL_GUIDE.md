# 安装指南

## 当前状态

根据检查结果：
- ✅ **pyserial** - 已安装（版本 3.5）
- ❌ **matplotlib** - 未安装（SSL连接失败）
- ❌ **numpy** - 未安装（SSL连接失败）

## 问题说明

遇到了 SSL 证书连接错误，无法从 PyPI 下载包。这可能是因为：
1. 网络环境限制（公司网络/防火墙）
2. SSL证书配置问题
3. 代理设置问题

## 解决方案

### 方案1：使用 --trusted-host 参数（推荐）

在命令行中运行：

```bash
python -m pip install matplotlib numpy --trusted-host pypi.org --trusted-host files.pythonhosted.org
```

或者使用国内镜像：

```bash
python -m pip install matplotlib numpy -i https://pypi.tuna.tsinghua.edu.cn/simple --trusted-host pypi.tuna.tsinghua.edu.cn
```

或者使用阿里云镜像：

```bash
python -m pip install matplotlib numpy -i https://mirrors.aliyun.com/pypi/simple/ --trusted-host mirrors.aliyun.com
```

### 方案2：禁用SSL验证（不推荐，但可以临时使用）

```bash
python -m pip install matplotlib numpy --trusted-host pypi.org --trusted-host pypi.python.org --trusted-host files.pythonhosted.org
```

### 方案3：配置pip使用代理

如果有代理，可以配置：

```bash
python -m pip install matplotlib numpy --proxy http://proxy.example.com:port
```

### 方案4：手动下载安装包

1. 访问 https://pypi.org/project/matplotlib/#files
2. 下载对应的 .whl 文件（Windows Python 3.8 64位）
3. 访问 https://pypi.org/project/numpy/#files
4. 下载对应的 .whl 文件
5. 使用以下命令安装：
   ```bash
   python -m pip install 下载的文件路径/matplotlib-xxx.whl
   python -m pip install 下载的文件路径/numpy-xxx.whl
   ```

### 方案5：使用conda（如果你使用conda环境）

```bash
conda install matplotlib numpy
```

## 验证安装

安装完成后，运行以下命令验证：

```bash
python -c "import matplotlib; import numpy; print('安装成功！')"
```

## 快速安装命令（复制粘贴即可）

推荐使用以下命令（使用阿里云镜像 + 信任主机）：

```bash
python -m pip install matplotlib numpy -i https://mirrors.aliyun.com/pypi/simple/ --trusted-host mirrors.aliyun.com
```
