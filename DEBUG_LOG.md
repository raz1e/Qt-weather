# qss 文件修改后需要清理重建

修改 .qss 文件后，Qt 资源编译不会自动检测变化，导致界面不更新。

**解决方法：**
构建 → 清理项目 → 重新构建所有项目

---

# 和风天气 API 认证问题

## 问题
使用 API Key 请求和风天气接口时返回 404 错误，无法连接。

## 原因
查询官方文档后发现：
- API Key 认证方式已被限制使用
- 官方更推荐使用 JWT Token 认证（更安全）

## 解决步骤
1. 访问和风天气控制台：https://console.qweather.com/
2. 进入项目 → 密钥管理 → 获取 JWT Token
3. 在代码中添加 Authorization 请求头：
   ```cpp
   request.setRawHeader("Authorization", "Bearer your_jwt_token");
   ```
4. 使用用户专属 API Host（如 `j87p4468c6.re.qweatherapi.com`）

## 结果
成功连接并获取实时天气数据。

---

# JWT Token 自动刷新问题

## 问题
JWT Token 有效期只有 15 分钟，过期后无法访问 API，导致认证失败（HTTP 401）。

## 原因
1. Token 有效期短（15分钟）
2. 手动更新繁琐
3. 脚本路径问题（程序在 build 目录运行，找不到项目根目录的脚本）

## 解决步骤
1. 创建 `generate_jwt.py` 脚本生成 JWT
2. 在 CMakeLists.txt 中添加自动复制规则
3. 使用 QTimer 每 14 分钟刷新一次
4. 使用 QProcess 调用 Python 脚本获取 Token

## 关键注意事项
- `add_custom_command` 必须放在目标定义之后
- 使用 `QCoreApplication::applicationDirPath()` 获取运行目录
- 刷新时间设置为 14 分钟（比有效期提前 1 分钟）

## 结果
程序启动时自动获取 JWT，每 14 分钟自动刷新，不再出现认证失败问题。





# 打包exe交互

## 问题
将程序发给同学后，显示网络错误，无法获取天气数据。

## 原因
1. **缺少 Qt 平台插件**：`qwindows.dll` 没有放在正确的 `platforms/` 目录下
2. **需要 Python 环境**：`generate_jwt.py` 需要 Python 和 `pyjwt` 库才能运行

## 解决步骤
1. **创建 platforms 文件夹**，将 `qwindows.dll` 放入其中
2. **使用 pyinstaller 打包 Python 脚本**：
   ```bash
   pip install pyinstaller
   pyinstaller --onefile generate_jwt.py
