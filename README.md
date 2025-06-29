# EasyProxyClient

EasyProxyClient 是面向 [EasyProxy](https://github.com/SwartzMss/EasyProxy) 的客户端示例。项目打算使用 Qt 开发图形界面程序，通过 HTTPS 使用自签证书的 EasyProxy 代理来连接网站。

在界面中，可以输入目标网址，并配置以下信息：
- 代理的 IP/域名 和端口
- 代理用户名与密码
- 自签的 CA 证书文件

程序会根据这些信息自动配置网络请求，使用户能安全地通过 EasyProxy 访问指定网站。

## 特色
- 展示如何配合 EasyProxy 使用
- 并非 CLI，以 Qt 实现的 GUI
- 支持加载自签证书，进行 HTTPS 互动

## License
MIT License
