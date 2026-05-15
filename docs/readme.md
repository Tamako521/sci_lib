# 科学文献管理系统使用说明

本项目基于 DBLP 本地数据集实现科学文献管理系统。程序采用“先构建索引，再运行 GUI”的方式：`index_builder.exe` 负责读取 `data/dblp.xml` 并生成 `data/index/` 索引文件，`sci_lib.exe` 负责打开索引并显示图形化界面。

## 1. 数据准备

将 DBLP 原始 XML 文件放到项目的 `data/` 目录下：

```text
data/dblp.xml
```

索引构建完成后会生成：

```text
data/index/
```

GUI 运行时只读取 `data/index/`，不会在 GUI 中解析 `dblp.xml`。

## 2. 编译 index_builder.exe

在项目根目录下使用 Qt/MinGW 编译索引构建工具：

```powershell
qmake.exe index_builder.pro -spec win32-g++ CONFIG+=release
mingw32-make -f Makefile.Release
```

编译完成后会生成：

```text
release/index_builder.exe
```

## 3. 构建索引数据文件

运行：

```text
release/index_builder.exe
```

它会默认读取：

```text
data/dblp.xml
```

并生成：

```text
data/index/
```

完整 DBLP 数据构建索引耗时较长，通常大约需要 40-50 分钟。构建期间请等待程序结束，不要关闭窗口。

如果检测到旧索引，程序会询问是否重新构建：

- 输入 `y`：删除旧索引并重新构建。
- 输入 `n`：保留旧索引并退出。

## 4. 编译 sci_lib.exe

索引构建完成后，编译 GUI 程序：

```powershell
qmake.exe sci_lib.pro -spec win32-g++ CONFIG+=release
mingw32-make -f Makefile.Release
```

编译完成后会生成：

```text
release/sci_lib.exe
```

## 5. 运行 GUI

双击运行：

```text
release/sci_lib.exe
```

程序启动后会自动检测 `data/index/manifest.bin`。如果索引不存在，会提示：

```text
未检测到索引，请先运行 index_builder.exe 构建索引。
```

此时需要先完成第 3 步索引构建。

## 6. 如果双击运行不了

如果双击 `sci_lib.exe` 没有反应，或提示缺少运行库、无法定位入口点、DLL 相关错误，请将项目 `ddl/` 目录下的文件复制到 `sci_lib.exe` 所在目录，也就是 `release/` 目录。

需要复制的文件包括：

```text
ddl/libgcc_s_seh-1.dll
ddl/libstdc++-6.dll
ddl/libwinpthread-1.dll
```

复制后的目录应类似：

```text
release/sci_lib.exe
release/libgcc_s_seh-1.dll
release/libstdc++-6.dll
release/libwinpthread-1.dll
```

如果 `index_builder.exe` 也出现同类 DLL 错误，也可以把这三个 DLL 放到 `release/` 中，因为两个 exe 都在同一目录下。

## 7. 推荐完整流程

```text
1. 将 dblp.xml 放入 data/dblp.xml
2. 编译 index_builder.exe
3. 运行 index_builder.exe 构建 data/index/，约 40-50 分钟
4. 编译 sci_lib.exe
5. 双击 release/sci_lib.exe 打开 GUI
6. 若无法运行，将 ddl/ 中的 DLL 复制到 release/
```
