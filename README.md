# EasyProxyClient

EasyProxyClient 是面向 [EasyProxy](https://github.com/SwartzMss/EasyProxy) 的客户端示例。项目使用 Qt 开发图形界面程序，通过 HTTPS 使用自签证书的 EasyProxy 代理来连接网站。

在界面中，可以输入目标网址，并配置以下信息：
- 代理的 IP/域名 和端口
- 代理用户名与密码
- 自签的 CA 证书文件

程序会根据这些信息自动配置网络请求，使用户能安全地通过 EasyProxy 访问指定网站。

## 特色
- 展示如何配合 EasyProxy 使用
- 并非 CLI，以 Qt 实现的 GUI
- 支持加载自签证书，进行 HTTPS 互动
- 现代化的用户界面
- 实时显示网络请求状态和响应结果
- **配置保存和加载功能**
- **菜单栏操作界面**

## 系统要求

- Windows 10 或更高版本
- Qt 6.0 或更高版本
- Visual Studio 2019 或更高版本（包含MSVC编译器）

## 构建说明

### 1. 安装依赖

#### 安装 Qt6
- 下载并安装 Qt 官方安装器：https://www.qt.io/download
- 选择 Qt 6.x 版本和 MSVC 编译器
- 确保安装了 Qt Core、Qt Widgets 和 Qt Network 模块

#### 安装 Visual Studio
- 下载 Visual Studio Community：https://visualstudio.microsoft.com/
- 安装时选择"C++桌面开发"工作负载

### 2. 构建项目

#### 方法一：使用 Qt Creator（推荐）
1. 打开 Qt Creator
2. 选择"打开项目"
3. 选择项目根目录下的 `EasyProxyClient.pro` 文件
4. 配置项目（选择MSVC编译器）
5. 点击构建按钮或按 Ctrl+B

#### 方法二：使用命令行
```cmd
# 进入项目目录
cd EasyProxyClient

# 生成 Makefile
qmake EasyProxyClient.pro

# 构建项目（Release版本）
nmake release

# 或者构建 Debug 版本
nmake debug
```

### 3. 运行程序

```cmd
# Release版本
.\bin\release\EasyProxyClient.exe

# Debug版本
.\bin\debug\EasyProxyClient.exe
```

## 使用说明

1. **配置目标网址**: 在"目标网址"输入框中输入要访问的网站地址
2. **配置代理设置**: 
   - 输入代理服务器的IP地址或域名
   - 设置代理端口（默认8080）
   - 如果代理需要认证，输入用户名和密码
3. **配置SSL证书**: 点击"浏览"按钮选择自签的CA证书文件（.pem, .crt, .cer格式）
4. **发起连接**: 点击"连接"按钮开始网络请求
5. **查看结果**: 在"响应结果"区域查看服务器返回的内容

### 配置管理

程序支持配置的保存和加载：

- **自动保存**: 程序关闭时自动保存当前设置
- **手动保存**: 点击"保存配置"按钮或通过"文件"菜单的"保存设置"选项
- **加载设置**: 程序启动时自动加载上次的设置，或通过"文件"菜单的"加载设置"选项
- **重置设置**: 通过"设置"菜单的"重置为默认值"选项

**配置文件位置**: `程序目录/config.ini`（与exe文件同级目录）

**配置内容**:
- 代理主机地址和端口
- 代理用户名和密码
- SSL证书路径
- 最后使用的URL
- 窗口大小

## 功能特性

- **代理支持**: 支持HTTP代理，包括认证功能
- **SSL/TLS支持**: 支持自签证书，确保HTTPS连接安全
- **实时反馈**: 显示连接进度和状态信息
- **错误处理**: 完善的错误提示和SSL错误处理
- **响应显示**: 智能识别内容类型，支持文本和二进制内容显示
- **配置持久化**: 自动保存和加载用户设置
- **菜单操作**: 完整的菜单栏界面

## 开发说明

### 项目结构
```
EasyProxyClient/
├── EasyProxyClient.pro    # qmake项目文件
├── README.md              # 项目说明文档
├── LICENSE                # 许可证文件
├── .gitignore            # Git忽略文件
├── config_example.ini    # 配置文件示例
└── src/                   # 源代码目录
    ├── main.cpp           # 程序入口
    ├── mainwindow.h       # 主窗口头文件
    ├── mainwindow.cpp     # 主窗口实现
    ├── proxyclient.h      # 代理客户端头文件
    ├── proxyclient.cpp    # 代理客户端实现
    ├── configmanager.h    # 配置管理头文件
    └── configmanager.cpp  # 配置管理实现
```

### 主要类说明

- `MainWindow`: 主窗口类，负责GUI界面和用户交互
- `ProxyClient`: 代理客户端类，负责网络请求和代理连接
- `ConfigManager`: 配置管理类，负责设置的保存和加载
- `QNetworkAccessManager`: 网络请求管理器
- `QNetworkProxy`: 代理配置
- `QSslConfiguration`: SSL配置

### 扩展功能

可以根据需要添加以下功能：
- 支持多种代理类型（SOCKS等）
- 请求历史记录
- 更详细的网络诊断信息
- 配置导入/导出功能
- 多语言支持

## 故障排除

### 常见问题

1. **SSL证书错误**: 确保选择了正确的CA证书文件
2. **代理连接失败**: 检查代理服务器地址、端口和认证信息
3. **编译错误**: 确保安装了正确版本的Qt和Visual Studio
4. **配置保存失败**: 检查程序是否有写入权限

### 调试模式

在Qt Creator中：
1. 选择Debug构建配置
2. 按F5开始调试

在命令行中：
```cmd
qmake EasyProxyClient.pro
nmake debug
```

## 贡献

欢迎提交Issue和Pull Request来改进这个项目。

## License

MIT License
