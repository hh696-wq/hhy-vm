# 60–90 秒终端演示

该演示用真实的 HHY 二进制依次展示版本、静态检查、Flow 执行、脱敏执行计划，
最后运行 SiteGraph Auditor 的确定性端到端自测。默认节奏约 75 秒；不伪造输出。

```sh
make
./scripts/terminal-demo.sh
```

快速回归（取消演示停顿）：

```sh
HHY_DEMO_DELAY=0 ./scripts/terminal-demo.sh
```

录制时建议使用 120×32 终端、深色主题和 18–20px 等宽字体。完整运行后再裁去构建过程，
保留开头版本、Flow 输出、dry-run 与 SiteGraph 自测通过四个证据点。
